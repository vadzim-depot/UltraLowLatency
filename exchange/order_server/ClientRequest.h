#pragma once

#include <sstream>

#include "common/Types.h"
#include "common/LockFreeQueue.h"

using namespace Common;

namespace Exchange
{
    /// Type of the order request sent by the trading client to the exchange.
    enum class EClientRequestType : uint8_t
    {
        INVALID = 0,
        NEW = 1,
        CANCEL = 2
    };

    inline std::string ClientRequestTypeToString(EClientRequestType type)
    {
        switch (type)
        {
            case EClientRequestType::NEW:
                return "NEW";
            case EClientRequestType::CANCEL:
                return "CANCEL";
            case EClientRequestType::INVALID:
                return "INVALID";
        }
        return "UNKNOWN";
    }

    /// These structures go over the wire / network, so the binary structures are packed to remove system dependent extra padding.
#pragma pack(push, 1)

    /// Client request structure used internally by the matching engine.
    struct SMEClientRequest
    {
        EClientRequestType type = EClientRequestType::INVALID;

        ClientId clientId = ClientId_INVALID;
        TickerId tickerId = TickerId_INVALID;
        OrderId  orderId  = OrderId_INVALID;
        ESide    side     = ESide::INVALID;
        Price    price    = Price_INVALID;
        Qty      qty      = Qty_INVALID;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "SMEClientRequest"
               << " ["
               << "type:"    << ClientRequestTypeToString(type)
               << " client:" << ClientIdToString(clientId)
               << " ticker:" << TickerIdToString(tickerId)
               << " oid:"    << OrderIdToString(orderId)
               << " side:"   << SideToString(side)
               << " qty:"    << QtyToString(qty)
               << " price:"  << PriceToString(price)
               << "]";
            return ss.str();
        }
    };

    /// Client request structure published over the network by the order gateway client.
    struct SOMClientRequest
    {
        size_t           seqNum = 0;
        SMEClientRequest meClientRequest;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "SOMClientRequest"
               << " ["
               << "seq:" << seqNum
               << " " << meClientRequest.ToString()
               << "]";
            return ss.str();
        }
    };

#pragma pack(pop) // Undo the packed binary structure directive moving forward.

    /// Lock free queues of matching engine client order request messages.
    typedef CLockFreeQueue<SMEClientRequest> ClientRequestLFQueue;
}
