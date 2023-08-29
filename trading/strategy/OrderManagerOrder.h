#pragma once

#include <array>
#include <sstream>
#include "common/Types.h"

using namespace Common;

namespace Trading
{
    /// Represents the type / action in the order structure in the order manager.
    enum class EOMOrderState : int8_t
    {
        INVALID = 0,
        PENDING_NEW = 1,
        LIVE = 2,
        PENDING_CANCEL = 3,
        DEAD = 4
    };

    inline auto OMOrderStateToString(EOMOrderState side) -> std::string
    {
        switch (side)
        {
        case EOMOrderState::PENDING_NEW:
            return "PENDING_NEW";
        case EOMOrderState::LIVE:
            return "LIVE";
        case EOMOrderState::PENDING_CANCEL:
            return "PENDING_CANCEL";
        case EOMOrderState::DEAD:
            return "DEAD";
        case EOMOrderState::INVALID:
            return "INVALID";
        }

        return "UNKNOWN";
    }

    /// Internal structure used by the order manager to represent a single strategy order.
    struct SOMOrder
    {
        TickerId tickerId = TickerId_INVALID;
        OrderId orderId = OrderId_INVALID;
        ESide side = ESide::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        EOMOrderState orderState = EOMOrderState::INVALID;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "SOMOrder"
               << "["
               << "tid:" << TickerIdToString(tickerId) << " "
               << "oid:" << OrderIdToString(orderId) << " "
               << "side:" << SideToString(side) << " "
               << "price:" << PriceToString(price) << " "
               << "qty:" << QtyToString(qty) << " "
               << "state:" << OMOrderStateToString(orderState) << "]";

            return ss.str();
        }
    };

    /// Hash map from Side -> SOMOrder.
    typedef std::array<SOMOrder, SideToIndex(ESide::MAX) + 1> OMOrderSideHashMap;

    /// Hash map from TickerId -> Side -> SOMOrder.
    typedef std::array<OMOrderSideHashMap, ME_MAX_TICKERS> OMOrderTickerSideHashMap;
}
