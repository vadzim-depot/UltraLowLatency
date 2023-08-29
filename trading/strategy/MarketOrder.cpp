#include "MarketOrder.h"

namespace Trading
{
    auto SMarketOrder::ToString() const -> std::string
    {
        std::stringstream ss;
        ss << "MarketOrder"
           << "["
           << "oid:" << OrderIdToString(orderId) << " "
           << "side:" << SideToString(side) << " "
           << "price:" << PriceToString(price) << " "
           << "qty:" << QtyToString(qty) << " "
           << "prio:" << PriorityToString(priority) << " "
           << "prev:" << OrderIdToString(pPrevOrder ? pPrevOrder->orderId : OrderId_INVALID) << " "
           << "next:" << OrderIdToString(pNextOrder ? pNextOrder->orderId : OrderId_INVALID) << "]";

        return ss.str();
    }
}
