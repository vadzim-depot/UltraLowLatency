#pragma once

#include <functional>

#include "common/ThreadUtils.h"
#include "common/TimeUtils.h"
#include "common/LockFreeQueue.h"
#include "common/Macros.h"
#include "common/Logging.h"

#include "exchange/order_server/ClientRequest.h"
#include "exchange/order_server/ClientResponse.h"
#include "exchange/market_data/MarketUpdate.h"

#include "MarketOrderBook.h"

#include "FeatureEngine.h"
#include "PositionKeeper.h"
#include "OrderManager.h"
#include "RiskManager.h"

#include "MarketMaker.h"
#include "LiquidityTaker.h"

namespace Trading
{
    class CTradeEngine
    {
    public:
        CTradeEngine(Common::ClientId clientId,
                    EAlgoType algoType,
                    const TradeEngineCfgHashMap& tickerCfg,
                    Exchange::ClientRequestLFQueue* pClientRequests,
                    Exchange::ClientResponseLFQueue* pClientResponses,
                    Exchange::MEMarketUpdateLFQueue* pMarketUpdates);

        ~CTradeEngine();

        /// Start and stop the trade engine main thread.
        auto Start() -> void
        {
            m_isRunning = true;
            ASSERT(Common::CreateAndStartThread(-1, "Trading/TradeEngine", [this]
                                                { Run(); }) != nullptr,
                   "Failed to start TradeEngine thread.");
        }

        auto Stop() -> void
        {
            while (pIncomingOgwResponses->size() || pIncomingMdUpdates->size())
            {
                m_logger.Log("%:% %() % Sleeping till all updates are consumed ogw-size:% md-size:%\n", __FILE__, __LINE__, __FUNCTION__,
                            Common::GetCurrentTimeStr(&m_timeStr), pIncomingOgwResponses->size(), pIncomingMdUpdates->size());

                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(10ms);
            }

            m_logger.Log("%:% %() % POSITIONS\n%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                        m_positionKeeper.ToString());

            m_isRunning = false;
        }

        /// Main loop for this thread - processes incoming client responses and market data updates which in turn may generate client requests.
        auto Run() noexcept -> void;

        /// Write a client request to the lock free queue for the order server to consume and send to the exchange.
        auto sendClientRequest(const Exchange::SMEClientRequest* pClientRequest) noexcept -> void;

        /// Process changes to the order book - updates the position keeper, feature engine and informs the trading algorithm about the update.
        auto onOrderBookUpdate(TickerId tickerId, Price price, ESide side, CMarketOrderBook* pBook) noexcept -> void;

        /// Process trade events - updates the  feature engine and informs the trading algorithm about the trade event.
        auto onTradeUpdate(const Exchange::SMEMarketUpdate* pMarketUpdate, CMarketOrderBook* pBook) noexcept -> void;

        /// Process client responses - updates the position keeper and informs the trading algorithm about the response.
        auto onOrderUpdate(const Exchange::SMEClientResponse* pClientResponse) noexcept -> void;

        /// Function wrappers to dispatch order book updates, trade events and client responses to the trading algorithm.
        std::function<void(const Exchange::SMEClientResponse* )>                                 m_algoOnOrderUpdate;
        std::function<void(const Exchange::SMEMarketUpdate*, CMarketOrderBook*)>                 m_algoOnTradeUpdate;
        std::function<void(TickerId tickerId, Price price, ESide side, CMarketOrderBook* pBook)> m_algoOnOrderBookUpdate;

        auto initLastEventTime()
        {
            m_lastEventTime = Common::GetCurrentNanos();
        }

        auto silentSeconds()
        {
            return (Common::GetCurrentNanos() - m_lastEventTime) / NANOS_TO_SECS;
        }

        auto GetClientId() const
        {
            return m_clientId;
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CTradeEngine() = delete;
        CTradeEngine(const CTradeEngine &) = delete;
        CTradeEngine(const CTradeEngine &&) = delete;
        CTradeEngine &operator=(const CTradeEngine &) = delete;
        CTradeEngine &operator=(const CTradeEngine &&) = delete;

    private:
        /// This trade engine's ClientId.
        const ClientId m_clientId;

        /// Hash map container from TickerId -> CMarketOrderBook.
        MarketOrderBookHashMap m_tickerOrderBook;

        /// Lock free queues.
        /// One to publish outgoing client requests to be consumed by the order gateway and sent to the exchange.
        /// Second to consume incoming client responses from, written to by the order gateway based on data received from the exchange.
        /// Third to consume incoming market data updates from, written to by the market data consumer based on data received from the exchange.
        Exchange::ClientRequestLFQueue*  pOutgoingOgwRequests  = nullptr;
        Exchange::ClientResponseLFQueue* pIncomingOgwResponses = nullptr;
        Exchange::MEMarketUpdateLFQueue* pIncomingMdUpdates    = nullptr;

        Nanos m_lastEventTime = 0;
        volatile bool m_isRunning = false;

        std::string m_timeStr;
        CLogger     m_logger;

        /// Feature engine for the trading algorithms.
        CFeatureEngine m_featureEngine;

        /// Position keeper to track position, pnl and volume.
        EPositionKeeper m_positionKeeper;

        /// Order manager to simplify the task of managing orders for the trading algorithms.
        COrderManager m_orderManager;

        /// Risk manager to track and perform pre-trade risk checks.
        CRiskManager m_pRiskManager;

        /// Market making or liquidity taking algorithm instance - only one of these is created in a single trade engine instance.
        CMarketMaker*    m_pMmAlgo = nullptr;
        CLiquidityTaker* m_pTakerAlgo = nullptr;

        /// Default methods to initialize the function wrappers.
        auto defaultAlgoOnOrderBookUpdate(TickerId tickerId, Price price, ESide side, CMarketOrderBook* ) noexcept -> void
        {
            m_logger.Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::GetCurrentTimeStr(&m_timeStr), tickerId, Common::PriceToString(price).c_str(),
                        Common::SideToString(side).c_str());
        }

        auto defaultAlgoOnTradeUpdate(const Exchange::SMEMarketUpdate* pMarketUpdate, CMarketOrderBook *) noexcept -> void
        {
            m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                        pMarketUpdate->ToString().c_str());
        }

        auto defaultAlgoOnOrderUpdate(const Exchange::SMEClientResponse* pClientResponse) noexcept -> void
        {
            m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                        pClientResponse->ToString().c_str());
        }
    };
}
