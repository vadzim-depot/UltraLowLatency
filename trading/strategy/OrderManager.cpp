#include "OrderManager.h"
#include "TradeEngine.h"

namespace Trading
{
    /// Send a new order with specified attribute, and update the SOMOrder object passed here.
    auto COrderManager::newOrder(SOMOrder *order, TickerId ticker_id, Price price, ESide side, Qty qty) noexcept -> void
    {
        const Exchange::SMEClientRequest new_request{Exchange::EClientRequestType::NEW, m_pTradeEngine->GetClientId(), ticker_id,
                                                    m_nextOrderId, side, price, qty};
        m_pTradeEngine->sendClientRequest(&new_request);

        *order = {ticker_id, m_nextOrderId, side, price, qty, EOMOrderState::PENDING_NEW};
        ++m_nextOrderId;

        m_logger->Log("%:% %() % Sent new order % for %\n", __FILE__, __LINE__, __FUNCTION__,
                     Common::GetCurrentTimeStr(&m_timeStr),
                     new_request.ToString().c_str(), order->ToString().c_str());
    }

    /// Send a cancel for the specified order, and update the SOMOrder object passed here.
    auto COrderManager::cancelOrder(SOMOrder *order) noexcept -> void
    {
        const Exchange::SMEClientRequest cancel_request{Exchange::EClientRequestType::CANCEL, m_pTradeEngine->GetClientId(),
                                                       order->tickerId, order->orderId, order->side, order->price,
                                                       order->qty};
        m_pTradeEngine->sendClientRequest(&cancel_request);

        order->orderState = EOMOrderState::PENDING_CANCEL;

        m_logger->Log("%:% %() % Sent CancelOrder % for %\n", __FILE__, __LINE__, __FUNCTION__,
                     Common::GetCurrentTimeStr(&m_timeStr),
                     cancel_request.ToString().c_str(), order->ToString().c_str());
    }
}
