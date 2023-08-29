#include "MatchingEngineOrder.h"

namespace Exchange
{
    auto SMEOrder::ToString() const -> std::string
    {
        std::stringstream ss;
        ss << "SMEOrder"
           << "["
           << "ticker:" << TickerIdToString(tickerId) << " "
           << "cid:" << ClientIdToString(clientId) << " "
           << "oid:" << OrderIdToString(clientOrderId) << " "
           << "moid:" << OrderIdToString(marketOrderId) << " "
           << "side:" << SideToString(side) << " "
           << "price:" << PriceToString(price) << " "
           << "qty:" << QtyToString(qty) << " "
           << "prio:" << PriorityToString(priority) << " "
           << "prev:" << OrderIdToString(pPrevOrder ? pPrevOrder->marketOrderId : OrderId_INVALID) << " "
           << "next:" << OrderIdToString(pNextOrder ? pNextOrder->marketOrderId : OrderId_INVALID) << "]";

        return ss.str();
    }
}
