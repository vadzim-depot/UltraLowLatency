#pragma once

#include "common/Macros.h"
#include "common/Logging.h"

#include "OrderManager.h"
#include "FeatureEngine.h"

using namespace Common;

namespace Trading
{
    class CMarketMaker
    {
    public:
        CMarketMaker(Common::CLogger* pLogger, CTradeEngine* pTradeEngine, const CFeatureEngine* pFeatureEngine,
                    COrderManager* pOrderManager,
                    const TradeEngineCfgHashMap& tickerCfg);

        /// Process order book updates, fetch the fair market price from the feature engine, check against the trading threshold and modify the passive orders.
        auto onOrderBookUpdate(TickerId tickerId, Price price, ESide side, const CMarketOrderBook* pBook) noexcept -> void
        {
            m_pLogger->Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                         Common::GetCurrentTimeStr(&m_timeStr), tickerId, Common::PriceToString(price).c_str(),
                         Common::SideToString(side).c_str());

            const auto bbo = pBook->GetBBO();
            const auto fairPrice = m_pFeatureEngine->getMktPrice();

            if (LIKELY(bbo->bidPrice != Price_INVALID && bbo->askPrice != Price_INVALID && fairPrice != Feature_INVALID))
            {
                m_pLogger->Log("%:% %() % % fair-price:%\n", __FILE__, __LINE__, __FUNCTION__,
                             Common::GetCurrentTimeStr(&m_timeStr),
                             bbo->ToString().c_str(), fairPrice);

                const auto clip = m_tickerCfg.at(tickerId).clip_;
                const auto threshold = m_tickerCfg.at(tickerId).threshold_;

                const auto bid_price = bbo->bidPrice - (fairPrice - bbo->bidPrice >= threshold ? 0 : 1);
                const auto ask_price = bbo->askPrice + (bbo->askPrice - fairPrice >= threshold ? 0 : 1);

                START_MEASURE(Trading_OrderManager_moveOrders);
                m_pOrderManager->moveOrders(tickerId, bid_price, ask_price, clip);
                END_MEASURE(Trading_OrderManager_moveOrders, (*m_pLogger));
            }
        }

        /// Process trade events, which for the market making algorithm is none.
        auto onTradeUpdate(const Exchange::SMEMarketUpdate* pMarketUpdate, CMarketOrderBook* /* book */) noexcept -> void
        {
            m_pLogger->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                         pMarketUpdate->ToString().c_str());
        }

        /// Process client responses for the strategy's orders.
        auto onOrderUpdate(const Exchange::SMEClientResponse* pClientResponse) noexcept -> void
        {
            m_pLogger->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                         pClientResponse->ToString().c_str());

            START_MEASURE(Trading_OrderManager_onOrderUpdate);
            m_pOrderManager->onOrderUpdate(pClientResponse);
            END_MEASURE(Trading_OrderManager_onOrderUpdate, (*m_pLogger));
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CMarketMaker() = delete;
        CMarketMaker(const CMarketMaker &) = delete;
        CMarketMaker(const CMarketMaker &&) = delete;
        CMarketMaker &operator=(const CMarketMaker &) = delete;
        CMarketMaker &operator=(const CMarketMaker &&) = delete;

    private:
        /// The feature engine that drives the market making algorithm.
        const CFeatureEngine* m_pFeatureEngine = nullptr;

        /// Used by the market making algorithm to manage its passive orders.
        COrderManager* m_pOrderManager = nullptr;

        std::string      m_timeStr;
        Common::CLogger* m_pLogger = nullptr;

        /// Holds the trading configuration for the market making algorithm.
        const TradeEngineCfgHashMap m_tickerCfg;
    };
}
