#include "TradeEngine.h"

namespace Trading
{
    CTradeEngine::CTradeEngine(Common::ClientId clientId,
                             EAlgoType algoType,
                             const TradeEngineCfgHashMap& tickerCfg,
                             Exchange::ClientRequestLFQueue* pClientRequests,
                             Exchange::ClientResponseLFQueue* pClientResponses,
                             Exchange::MEMarketUpdateLFQueue* pMarketUpdates)
        : m_clientId(clientId)
        , pOutgoingOgwRequests(pClientRequests)
        , pIncomingOgwResponses(pClientResponses)
        , pIncomingMdUpdates(pMarketUpdates)
        , m_logger("trading_engine_" + std::to_string(clientId) + ".log")
        , m_featureEngine(&m_logger)
        , m_positionKeeper(&m_logger)
        , m_orderManager(&m_logger, this, m_pRiskManager)
        , m_pRiskManager(&m_logger, &m_positionKeeper, tickerCfg)
    {
        for (size_t i = 0; i < m_tickerOrderBook.size(); ++i)
        {
            m_tickerOrderBook[i] = new CMarketOrderBook(i, &m_logger);
            m_tickerOrderBook[i]->SetTradeEngine(this);
        }

        // Initialize the function wrappers for the callbacks for order book changes, trade events and client responses.
        m_algoOnOrderBookUpdate = [this](auto ticker_id, auto price, auto side, auto book)
        {
            defaultAlgoOnOrderBookUpdate(ticker_id, price, side, book);
        };

        m_algoOnTradeUpdate = [this](auto market_update, auto book)
        { 
            defaultAlgoOnTradeUpdate(market_update, book);
        };

        m_algoOnOrderUpdate = [this](auto client_response)
        {
            defaultAlgoOnOrderUpdate(client_response);
        };

        // Create the trading algorithm instance based on the EAlgoType provided.
        // The constructor will override the callbacks above for order book changes, trade events and client responses.
        if (algoType == EAlgoType::MAKER)
        {
            m_pMmAlgo = new CMarketMaker(&m_logger, this, &m_featureEngine, &m_orderManager, tickerCfg);
        }
        else if (algoType == EAlgoType::TAKER)
        {
            m_pTakerAlgo = new CLiquidityTaker(&m_logger, this, &m_featureEngine, &m_orderManager, tickerCfg);
        }

        for (TickerId i = 0; i < tickerCfg.size(); ++i)
        {
            m_logger.Log("%:% %() % Initialized % Ticker:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::GetCurrentTimeStr(&m_timeStr),
                        AlgoTypeToString(algoType), i,
                        tickerCfg.at(i).ToString());
        }
    }

    CTradeEngine::~CTradeEngine()
    {
        m_isRunning = false;

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);

        delete m_pMmAlgo;
        m_pMmAlgo = nullptr;
        delete m_pTakerAlgo;
        m_pTakerAlgo = nullptr;

        for (auto &order_book : m_tickerOrderBook)
        {
            delete order_book;
            order_book = nullptr;
        }

        pOutgoingOgwRequests = nullptr;
        pIncomingOgwResponses = nullptr;
        pIncomingMdUpdates = nullptr;
    }

    /// Write a client request to the lock free queue for the order server to consume and send to the exchange.
    auto CTradeEngine::sendClientRequest(const Exchange::SMEClientRequest *client_request) noexcept -> void
    {
        m_logger.Log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                    client_request->ToString().c_str());
        auto next_write = pOutgoingOgwRequests->GetNextToWriteTo();
        *next_write = std::move(*client_request);
        pOutgoingOgwRequests->UpdateWriteIndex();
        TTT_MEASURE(T10_TradeEngine_LFQueue_write, m_logger);
    }

    /// Main loop for this thread - processes incoming client responses and market data updates which in turn may generate client requests.
    auto CTradeEngine::Run() noexcept -> void
    {
        m_logger.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
        while (m_isRunning)
        {
            for (auto client_response = pIncomingOgwResponses->GetNextToRead(); client_response; client_response = pIncomingOgwResponses->GetNextToRead())
            {
                TTT_MEASURE(T9t_TradeEngine_LFQueue_read, m_logger);

                m_logger.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                            client_response->ToString().c_str());
                onOrderUpdate(client_response);
                pIncomingOgwResponses->UpdateReadIndex();
                m_lastEventTime = Common::GetCurrentNanos();
            }

            for (auto market_update = pIncomingMdUpdates->GetNextToRead(); market_update; market_update = pIncomingMdUpdates->GetNextToRead())
            {
                TTT_MEASURE(T9_TradeEngine_LFQueue_read, m_logger);

                m_logger.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                            market_update->ToString().c_str());

                ASSERT(market_update->tickerId < m_tickerOrderBook.size(),
                       "Unknown ticker-id on update:" + market_update->ToString());

                m_tickerOrderBook[market_update->tickerId]->OnMarketUpdate(market_update);
                pIncomingMdUpdates->UpdateReadIndex();
                m_lastEventTime = Common::GetCurrentNanos();
            }
        }
    }

    /// Process changes to the order book - updates the position keeper, feature engine and informs the trading algorithm about the update.
    auto CTradeEngine::onOrderBookUpdate(TickerId ticker_id, Price price, ESide side, CMarketOrderBook *book) noexcept -> void
    {
        m_logger.Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::GetCurrentTimeStr(&m_timeStr), ticker_id, Common::PriceToString(price).c_str(),
                    Common::SideToString(side).c_str());

        auto bbo = book->GetBBO();

        START_MEASURE(Trading_PositionKeeper_updateBBO);
        m_positionKeeper.UpdateBBO(ticker_id, bbo);
        END_MEASURE(Trading_PositionKeeper_updateBBO, m_logger);

        START_MEASURE(Trading_FeatureEngine_onOrderBookUpdate);
        m_featureEngine.onOrderBookUpdate(ticker_id, price, side, book);
        END_MEASURE(Trading_FeatureEngine_onOrderBookUpdate, m_logger);

        START_MEASURE(Trading_TradeEngine_algoOnOrderBookUpdate_);
        m_algoOnOrderBookUpdate(ticker_id, price, side, book);
        END_MEASURE(Trading_TradeEngine_algoOnOrderBookUpdate_, m_logger);
    }

    /// Process trade events - updates the  feature engine and informs the trading algorithm about the trade event.
    auto CTradeEngine::onTradeUpdate(const Exchange::SMEMarketUpdate *market_update, CMarketOrderBook *book) noexcept -> void
    {
        m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                    market_update->ToString().c_str());

        START_MEASURE(Trading_FeatureEngine_onTradeUpdate);
        m_featureEngine.onTradeUpdate(market_update, book);
        END_MEASURE(Trading_FeatureEngine_onTradeUpdate, m_logger);

        START_MEASURE(Trading_TradeEngine_algoOnTradeUpdate_);
        m_algoOnTradeUpdate(market_update, book);
        END_MEASURE(Trading_TradeEngine_algoOnTradeUpdate_, m_logger);
    }

    /// Process client responses - updates the position keeper and informs the trading algorithm about the response.
    auto CTradeEngine::onOrderUpdate(const Exchange::SMEClientResponse *client_response) noexcept -> void
    {
        m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                    client_response->ToString().c_str());

        if (UNLIKELY(client_response->type == Exchange::EClientResponseType::FILLED))
        {
            START_MEASURE(Trading_PositionKeeper_addFill);
            m_positionKeeper.addFill(client_response);
            END_MEASURE(Trading_PositionKeeper_addFill, m_logger);
        }

        START_MEASURE(Trading_TradeEngine_algoOnOrderUpdate_);
        m_algoOnOrderUpdate(client_response);
        END_MEASURE(Trading_TradeEngine_algoOnOrderUpdate_, m_logger);
    }
}
