#pragma once

#include "common/Macros.h"
#include "common/Logging.h"

#include "exchange/order_server/ClientResponse.h"

#include "OrderManagerOrder.h"
#include "RiskManager.h"

using namespace Common;

namespace Trading
{
    class CTradeEngine;

    /// Manages orders for a trading algorithm, hides the complexity of order management to simplify trading strategies.
    class COrderManager
    {
    public:
        COrderManager(Common::CLogger *logger, CTradeEngine *pTradeEngine, CRiskManager &risk_manager)
            : m_pTradeEngine(pTradeEngine), m_pRiskManager(risk_manager), m_logger(logger)
        {
        }

        /// Process an order update from a client response and update the state of the orders being managed.
        auto onOrderUpdate(const Exchange::SMEClientResponse* pClientResponse) noexcept -> void
        {
            m_logger->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                         pClientResponse->ToString().c_str());
            auto pOrder = &(m_tickerSideOrder.at(pClientResponse->tickerId).at(SideToIndex(pClientResponse->side)));
            m_logger->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                         pOrder->ToString().c_str());

            switch (pClientResponse->type)
            {
                case Exchange::EClientResponseType::ACCEPTED:
                {
                    pOrder->orderState = EOMOrderState::LIVE;
                }
                break;
                case Exchange::EClientResponseType::CANCELED:
                {
                    pOrder->orderState = EOMOrderState::DEAD;
                }
                break;
                case Exchange::EClientResponseType::FILLED:
                {
                    pOrder->qty = pClientResponse->leavesQty;
                    if (!pOrder->qty)
                        pOrder->orderState = EOMOrderState::DEAD;
                }
                break;
                case Exchange::EClientResponseType::CANCEL_REJECTED:
                case Exchange::EClientResponseType::INVALID:
                {
                }
                break;
            }
        }

        /// Send a new order with specified attribute, and update the SOMOrder object passed here.
        auto newOrder(SOMOrder *pOrder, TickerId ticker_id, Price price, ESide side, Qty qty) noexcept -> void;

        /// Send a cancel for the specified order, and update the SOMOrder object passed here.
        auto cancelOrder(SOMOrder *pOrder) noexcept -> void;

        /// Move a single order on the specified side so that it has the specified price and quantity.
        /// This will perform risk checks prior to sending the order, and update the SOMOrder object passed here.
        auto moveOrder(SOMOrder *pOrder, TickerId ticker_id, Price price, ESide side, Qty qty) noexcept
        {
            switch (pOrder->orderState)
            {
            case EOMOrderState::LIVE:
            {
                if (pOrder->price != price)
                {
                    START_MEASURE(Trading_OrderManager_cancelOrder);
                    cancelOrder(pOrder);
                    END_MEASURE(Trading_OrderManager_cancelOrder, (*m_logger));
                }
            }
            break;
            case EOMOrderState::INVALID:
            case EOMOrderState::DEAD:
            {
                if (LIKELY(price != Price_INVALID))
                {
                    START_MEASURE(Trading_RiskManager_checkPreTradeRisk);
                    const auto risk_result = m_pRiskManager.checkPreTradeRisk(ticker_id, side, qty);
                    END_MEASURE(Trading_RiskManager_checkPreTradeRisk, (*m_logger));
                    if (LIKELY(risk_result == ERiskCheckResult::ALLOWED))
                    {
                        START_MEASURE(Trading_OrderManager_newOrder);
                        newOrder(pOrder, ticker_id, price, side, qty);
                        END_MEASURE(Trading_OrderManager_newOrder, (*m_logger));
                    }
                    else
                        m_logger->Log("%:% %() % Ticker:% Side:% Qty:% ERiskCheckResult:%\n", __FILE__, __LINE__, __FUNCTION__,
                                     Common::GetCurrentTimeStr(&m_timeStr),
                                     TickerIdToString(ticker_id), SideToString(side), QtyToString(qty),
                                     riskCheckResultToString(risk_result));
                }
            }
            break;
            case EOMOrderState::PENDING_NEW:
            case EOMOrderState::PENDING_CANCEL:
                break;
            }
        }

        /// Have orders of quantity clip at the specified buy and sell prices.
        /// This can result in new orders being sent if there are none.
        /// This can result in existing orders being cancelled if they are not at the specified price or of the specified quantity.
        /// Specifying Price_INVALID for the buy or sell prices indicates that we do not want an order there.
        auto moveOrders(TickerId ticker_id, Price bid_price, Price ask_price, Qty clip) noexcept
        {
            {
                auto bid_order = &(m_tickerSideOrder.at(ticker_id).at(SideToIndex(ESide::BUY)));
                START_MEASURE(Trading_OrderManager_moveOrder);
                moveOrder(bid_order, ticker_id, bid_price, ESide::BUY, clip);
                END_MEASURE(Trading_OrderManager_moveOrder, (*m_logger));
            }

            {
                auto ask_order = &(m_tickerSideOrder.at(ticker_id).at(SideToIndex(ESide::SELL)));
                START_MEASURE(Trading_OrderManager_moveOrder);
                moveOrder(ask_order, ticker_id, ask_price, ESide::SELL, clip);
                END_MEASURE(Trading_OrderManager_moveOrder, (*m_logger));
            }
        }

        /// Helper method to fetch the buy and sell OMOrders for the specified TickerId.
        auto getOMOrderSideHashMap(TickerId ticker_id) const
        {
            return &(m_tickerSideOrder.at(ticker_id));
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        COrderManager() = delete;
        COrderManager(const COrderManager &) = delete;
        COrderManager(const COrderManager &&) = delete;
        COrderManager &operator=(const COrderManager &) = delete;
        COrderManager &operator=(const COrderManager &&) = delete;

    private:
        /// The parent trade engine object, used to send out client requests.
        CTradeEngine* m_pTradeEngine = nullptr;

        /// Risk manager to perform pre-trade risk checks.
        const CRiskManager& m_pRiskManager;

        std::string      m_timeStr;
        Common::CLogger* m_logger = nullptr;

        /// Hash map container from TickerId -> Side -> SOMOrder.
        OMOrderTickerSideHashMap m_tickerSideOrder;

        /// Used to set OrderIds on outgoing new order requests.
        OrderId m_nextOrderId = 1;
    };
}
