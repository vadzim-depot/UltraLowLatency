#pragma once

#include "common/Macros.h"
#include "common/Logging.h"

#include "OrderManager.h"
#include "FeatureEngine.h"

using namespace Common;

namespace Trading
{
    class CLiquidityTaker
    {
    public:
        CLiquidityTaker(Common::CLogger *logger, CTradeEngine *pTradeEngine, const CFeatureEngine *feature_engine,
                       COrderManager *order_manager,
                       const TradeEngineCfgHashMap &ticker_cfg);

        /// Process order book updates, which for the liquidity taking algorithm is none.
        auto onOrderBookUpdate(TickerId ticker_id, Price price, ESide side, CMarketOrderBook *) noexcept -> void
        {
            m_pLogger->Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                         Common::GetCurrentTimeStr(&m_timeStr), ticker_id, Common::PriceToString(price).c_str(),
                         Common::SideToString(side).c_str());
        }

        /// Process trade events, fetch the aggressive trade ratio from the feature engine, check against the trading threshold and send aggressive orders.
        auto onTradeUpdate(const Exchange::SMEMarketUpdate *market_update, CMarketOrderBook *book) noexcept -> void
        {
            m_pLogger->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                         market_update->ToString().c_str());

            const auto bbo = book->GetBBO();
            const auto agg_qty_ratio = m_pFeatureEngine->getAggTradeQtyRatio();

            if (LIKELY(bbo->bidPrice != Price_INVALID && bbo->askPrice != Price_INVALID && agg_qty_ratio != Feature_INVALID))
            {
                m_pLogger->Log("%:% %() % % agg-qty-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                             Common::GetCurrentTimeStr(&m_timeStr),
                             bbo->ToString().c_str(), agg_qty_ratio);

                const auto clip = m_tickerCfg.at(market_update->tickerId).clip_;
                const auto threshold = m_tickerCfg.at(market_update->tickerId).threshold_;

                if (agg_qty_ratio >= threshold)
                {
                    START_MEASURE(Trading_OrderManager_moveOrders);
                    if (market_update->side == ESide::BUY)
                        m_pOrderManager->moveOrders(market_update->tickerId, bbo->askPrice, Price_INVALID, clip);
                    else
                        m_pOrderManager->moveOrders(market_update->tickerId, Price_INVALID, bbo->bidPrice, clip);
                    END_MEASURE(Trading_OrderManager_moveOrders, (*m_pLogger));
                }
            }
        }

        /// Process client responses for the strategy's orders.
        auto onOrderUpdate(const Exchange::SMEClientResponse *client_response) noexcept -> void
        {
            m_pLogger->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                         client_response->ToString().c_str());
            START_MEASURE(Trading_OrderManager_onOrderUpdate);
            m_pOrderManager->onOrderUpdate(client_response);
            END_MEASURE(Trading_OrderManager_onOrderUpdate, (*m_pLogger));
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CLiquidityTaker() = delete;
        CLiquidityTaker(const CLiquidityTaker &) = delete;
        CLiquidityTaker(const CLiquidityTaker &&) = delete;
        CLiquidityTaker &operator=(const CLiquidityTaker &) = delete;
        CLiquidityTaker &operator=(const CLiquidityTaker &&) = delete;

    private:
        /// The feature engine that drives the liquidity taking algorithm.
        const CFeatureEngine* m_pFeatureEngine = nullptr;

        /// Used by the liquidity taking algorithm to send aggressive orders.
        COrderManager* m_pOrderManager = nullptr;

        std::string      m_timeStr;
        Common::CLogger* m_pLogger = nullptr;

        /// Holds the trading configuration for the liquidity taking algorithm.
        const TradeEngineCfgHashMap m_tickerCfg;
    };
}
