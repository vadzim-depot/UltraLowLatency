#pragma once

#include <sstream>

#include "common/Types.h"
#include "common/LockFreeQueue.h"

using namespace Common;

namespace Exchange
{
    /// Represents the type / action in the market update message.
    enum class EMarketUpdateType : uint8_t
    {
        INVALID = 0,
        CLEAR = 1,
        ADD = 2,
        MODIFY = 3,
        CANCEL = 4,
        TRADE = 5,
        SNAPSHOT_START = 6,
        SNAPSHOT_END = 7
    };

    inline std::string MarketUpdateTypeToString(EMarketUpdateType type)
    {
        switch (type)
        {
            case EMarketUpdateType::CLEAR:
                return "CLEAR";
            case EMarketUpdateType::ADD:
                return "ADD";
            case EMarketUpdateType::MODIFY:
                return "MODIFY";
            case EMarketUpdateType::CANCEL:
                return "CANCEL";
            case EMarketUpdateType::TRADE:
                return "TRADE";
            case EMarketUpdateType::SNAPSHOT_START:
                return "SNAPSHOT_START";
            case EMarketUpdateType::SNAPSHOT_END:
                return "SNAPSHOT_END";
            case EMarketUpdateType::INVALID:
                return "INVALID";
        }
        return "UNKNOWN";
    }

    /// These structures go over the wire / network, so the binary structures are packed to remove system dependent extra padding.
#pragma pack(push, 1)

    /// Market update structure used internally by the matching engine.
    struct SMEMarketUpdate
    {
        EMarketUpdateType type = EMarketUpdateType::INVALID;

        OrderId  orderId  = OrderId_INVALID;
        TickerId tickerId = TickerId_INVALID;
        ESide    side     = ESide::INVALID;
        Price    price    = Price_INVALID;
        Qty      qty      = Qty_INVALID;
        Priority priority = Priority_INVALID;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "EMEMarketUpdate"
               << " ["
               << " type:"     << MarketUpdateTypeToString(type)
               << " ticker:"   << TickerIdToString(tickerId)
               << " oid:"      << OrderIdToString(orderId)
               << " side:"     << SideToString(side)
               << " qty:"      << QtyToString(qty)
               << " price:"    << PriceToString(price)
               << " priority:" << PriorityToString(priority)
               << "]";
            return ss.str();
        }
    };

    /// Market update structure published over the network by the market data publisher.
    struct MDPMarketUpdate
    {
        size_t seqNum = 0;
        SMEMarketUpdate me_market_update_;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "MDPMarketUpdate"
               << " ["
               << " seq:" << seqNum
               << " " << me_market_update_.ToString()
               << "]";
            return ss.str();
        }
    };

#pragma pack(pop) // Undo the packed binary structure directive moving forward.

    /// Lock free queues of matching engine market update messages and market data publisher market updates messages respectively.
    typedef Common::CLockFreeQueue<Exchange::SMEMarketUpdate> MEMarketUpdateLFQueue;
    typedef Common::CLockFreeQueue<Exchange::MDPMarketUpdate> MDPMarketUpdateLFQueue;
}
