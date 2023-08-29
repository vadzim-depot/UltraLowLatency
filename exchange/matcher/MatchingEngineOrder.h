#pragma once

#include <array>
#include <sstream>
#include "common/Types.h"

using namespace Common;

namespace Exchange
{
    /// Used by the matching engine to represent a single order in the limit order book.
    struct SMEOrder
    {
        TickerId tickerId      = TickerId_INVALID;
        ClientId clientId      = ClientId_INVALID;
        OrderId  clientOrderId = OrderId_INVALID;
        OrderId  marketOrderId = OrderId_INVALID;
        ESide    side          = ESide::INVALID;
        Price    price         = Price_INVALID;
        Qty      qty           = Qty_INVALID;
        Priority priority      = Priority_INVALID;

        /// SMEOrder also serves as a node in a doubly linked list of all orders at price level arranged in FIFO order.
        SMEOrder* pPrevOrder = nullptr;
        SMEOrder* pNextOrder = nullptr;

        /// Only needed for use with CMemoryPool.
        SMEOrder() = default;

        SMEOrder(TickerId ticker_id, ClientId client_id, OrderId client_order_id, OrderId market_order_id, ESide side, Price price,
                Qty qty, Priority priority, SMEOrder *prev_order, SMEOrder *next_order) noexcept
            : tickerId(ticker_id)
            , clientId(client_id)
            , clientOrderId(client_order_id)
            , marketOrderId(market_order_id)
            , side(side)
            , price(price)
            , qty(qty)
            , priority(priority)
            , pPrevOrder(prev_order)
            , pNextOrder(next_order) {}

        auto ToString() const -> std::string;
    };

    /// Hash map from OrderId -> SMEOrder.
    typedef std::array<SMEOrder *, ME_MAX_ORDER_IDS> OrderHashMap;

    /// Hash map from ClientId -> OrderId -> SMEOrder.
    typedef std::array<OrderHashMap, ME_MAX_NUM_CLIENTS> ClientOrderHashMap;

    /// Used by the matching engine to represent a price level in the limit order book.
    /// Internally maintains a list of SMEOrder objects arranged in FIFO order.
    struct SMEOrdersAtPrice
    {
        ESide side  = ESide::INVALID;
        Price price = Price_INVALID;

        SMEOrder* pFirstMeOrder = nullptr;

        /// SMEOrdersAtPrice also serves as a node in a doubly linked list of price levels arranged in order from most aggressive to least aggressive price.
        SMEOrdersAtPrice* pPrevEntry = nullptr;
        SMEOrdersAtPrice* pNextEntry = nullptr;

        /// Only needed for use with CMemoryPool.
        SMEOrdersAtPrice() = default;

        SMEOrdersAtPrice(ESide side, Price price, SMEOrder* first_me_order, SMEOrdersAtPrice* prev_entry, SMEOrdersAtPrice* next_entry)
            : side(side)
            , price(price)
            , pFirstMeOrder(first_me_order)
            , pPrevEntry(prev_entry)
            , pNextEntry(next_entry)
        {            
        }

        auto ToString() const
        {
            std::stringstream ss;
            ss << "SMEOrdersAtPrice["
               << "side:" << SideToString(side) << " "
               << "price:" << PriceToString(price) << " "
               << "first_me_order:" << (pFirstMeOrder ? pFirstMeOrder->ToString() : "null") << " "
               << "prev:" << PriceToString(pPrevEntry ? pPrevEntry->price : Price_INVALID) << " "
               << "next:" << PriceToString(pNextEntry ? pNextEntry->price : Price_INVALID) << "]";

            return ss.str();
        }
    };

    /// Hash map from Price -> SMEOrdersAtPrice.
    typedef std::array<SMEOrdersAtPrice *, ME_MAX_PRICE_LEVELS> OrdersAtPriceHashMap;
}
