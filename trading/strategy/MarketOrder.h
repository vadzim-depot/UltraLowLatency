#pragma once

#include <array>
#include <sstream>
#include "common/Types.h"

using namespace Common;

namespace Trading
{
    /// Used by the trade engine to represent a single order in the limit order book.
    struct SMarketOrder
    {
        OrderId orderId = OrderId_INVALID;
        ESide side = ESide::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Priority priority = Priority_INVALID;

        /// MarketOrder also serves as a node in a doubly linked list of all orders at price level arranged in FIFO order.
        SMarketOrder *pPrevOrder = nullptr;
        SMarketOrder *pNextOrder = nullptr;

        /// Only needed for use with CMemoryPool.
        SMarketOrder() = default;

        SMarketOrder(OrderId order_id, ESide side, Price price, Qty qty, Priority priority, SMarketOrder *prev_order, SMarketOrder *next_order) noexcept
            : orderId(order_id), side(side), price(price), qty(qty), priority(priority), pPrevOrder(prev_order), pNextOrder(next_order) {}

        auto ToString() const -> std::string;
    };

    /// Hash map from OrderId -> MarketOrder.
    typedef std::array<SMarketOrder *, ME_MAX_ORDER_IDS> OrderHashMap;

    /// Used by the trade engine to represent a price level in the limit order book.
    /// Internally maintains a list of MarketOrder objects arranged in FIFO order.
    struct SMarketOrdersAtPrice
    {
        ESide side = ESide::INVALID;
        Price price = Price_INVALID;

        SMarketOrder* pFirstMktOrder = nullptr;

        /// MarketOrdersAtPrice also serves as a node in a doubly linked list of price levels arranged in order from most aggressive to least aggressive price.
        SMarketOrdersAtPrice* pPrevEntry = nullptr;
        SMarketOrdersAtPrice* pNextEntry = nullptr;

        /// Only needed for use with CMemoryPool.
        SMarketOrdersAtPrice() = default;
        SMarketOrdersAtPrice(ESide side_, Price price_, SMarketOrder* pFirstMktOrder_, SMarketOrdersAtPrice* pPrevEntry_, SMarketOrdersAtPrice* pNextEntry_)
            : side(side_)
            , price(price_)
            , pFirstMktOrder(pFirstMktOrder_)
            , pPrevEntry(pPrevEntry_)
            , pNextEntry(pNextEntry_) {}

        auto ToString() const
        {
            std::stringstream ss;
            ss << "MarketOrdersAtPrice["
               << "side:" << SideToString(side) << " "
               << "price:" << PriceToString(price) << " "
               << "first_mkt_order:" << (pFirstMktOrder ? pFirstMktOrder->ToString() : "null") << " "
               << "prev:" << PriceToString(pPrevEntry ? pPrevEntry->price : Price_INVALID) << " "
               << "next:" << PriceToString(pNextEntry ? pNextEntry->price : Price_INVALID) << "]";

            return ss.str();
        }
    };

    /// Hash map from Price -> MarketOrdersAtPrice.
    typedef std::array<SMarketOrdersAtPrice*, ME_MAX_PRICE_LEVELS> OrdersAtPriceHashMap;

    /// Represents a Best Bid Offer (BBO) abstraction for components which only need a small summary of top of book price and liquidity instead of the full order book.
    struct SBBO
    {
        Price bidPrice = Price_INVALID;
        Price askPrice = Price_INVALID;
        Qty   bidQty   = Qty_INVALID;
        Qty   askQty   = Qty_INVALID;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "BBO{"
               << QtyToString(bidQty) << "@" << PriceToString(bidPrice)
               << "X"
               << PriceToString(askPrice) << "@" << QtyToString(askQty)
               << "}";

            return ss.str();
        };
    };
}
