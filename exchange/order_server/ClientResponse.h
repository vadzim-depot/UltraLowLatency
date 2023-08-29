#pragma once

#include <sstream>

#include "common/Types.h"
#include "common/LockFreeQueue.h"

using namespace Common;

namespace Exchange
{
    /// Type of the order response sent by the exchange to the trading client.
    enum class EClientResponseType : uint8_t
    {
        INVALID = 0,
        ACCEPTED = 1,
        CANCELED = 2,
        FILLED = 3,
        CANCEL_REJECTED = 4
    };

    inline std::string ClientResponseTypeToString(EClientResponseType type)
    {
        switch (type)
        {
            case EClientResponseType::ACCEPTED:
                return "ACCEPTED";
            case EClientResponseType::CANCELED:
                return "CANCELED";
            case EClientResponseType::FILLED:
                return "FILLED";
            case EClientResponseType::CANCEL_REJECTED:
                return "CANCEL_REJECTED";
            case EClientResponseType::INVALID:
                return "INVALID";
        }
        return "UNKNOWN";
    }

    /// These structures go over the wire / network, so the binary structures are packed to remove system dependent extra padding.
#pragma pack(push, 1)

    /// Client response structure used internally by the matching engine.
    struct SMEClientResponse
    {
        EClientResponseType type          = EClientResponseType::INVALID;
        ClientId            clientId      = ClientId_INVALID;
        TickerId            tickerId      = TickerId_INVALID;
        OrderId             clientOrderId = OrderId_INVALID;
        OrderId             marketOrderId = OrderId_INVALID;
        ESide               side          = ESide::INVALID;
        Price               price         = Price_INVALID;
        Qty                 execQty       = Qty_INVALID;
        Qty                 leavesQty     = Qty_INVALID;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "SMEClientResponse"
               << " ["
               << "type:" << ClientResponseTypeToString(type)
               << " client:" << ClientIdToString(clientId)
               << " ticker:" << TickerIdToString(tickerId)
               << " coid:" << OrderIdToString(clientOrderId)
               << " moid:" << OrderIdToString(marketOrderId)
               << " side:" << SideToString(side)
               << " exec_qty:" << QtyToString(execQty)
               << " leaves_qty:" << QtyToString(leavesQty)
               << " price:" << PriceToString(price)
               << "]";
            return ss.str();
        }
    };

    /// Client response structure published over the network by the order server.
    struct SOMClientResponse
    {
        size_t            seqNum = 0;
        SMEClientResponse meClientResponse;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "SOMClientResponse"
               << " ["
               << "seq:" << seqNum
               << " " << meClientResponse.ToString()
               << "]";
            return ss.str();
        }
    };

#pragma pack(pop) // Undo the packed binary structure directive moving forward.

    /// Lock free queues of matching engine client order response messages.
    typedef CLockFreeQueue<SMEClientResponse> ClientResponseLFQueue;
}
