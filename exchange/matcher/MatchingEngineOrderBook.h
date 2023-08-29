#pragma once

#include "common/Types.h"
#include "common/MemoryPool.h"
#include "common/Logging.h"
#include "order_server/ClientResponse.h"
#include "market_data/MarketUpdate.h"

#include "MatchingEngineOrder.h"

using namespace Common;

namespace Exchange
{
    class CMatchingEngine;

    class CMEOrderBook final
    {
    public:
        explicit CMEOrderBook(TickerId ticker_id, CLogger *logger, CMatchingEngine *matching_engine);

        ~CMEOrderBook();

        /// Create and add a new order in the order book with provided attributes.
        /// It will check to see if this new order matches an existing passive order with opposite side, and perform the matching if that is the case.
        auto AddOrder(ClientId client_id, OrderId client_order_id, TickerId ticker_id, ESide side, Price price, Qty qty) noexcept -> void;

        /// Attempt to cancel an order in the order book, issue a cancel-rejection if order does not exist.
        auto CancelOrder(ClientId client_id, OrderId order_id, TickerId ticker_id) noexcept -> void;

        auto ToString(bool detailed, bool validity_check) const -> std::string;

        /// Deleted default, copy & move constructors and assignment-operators.
        CMEOrderBook() = delete;
        CMEOrderBook(const CMEOrderBook &) = delete;
        CMEOrderBook(const CMEOrderBook &&) = delete;
        CMEOrderBook &operator=(const CMEOrderBook &) = delete;
        CMEOrderBook &operator=(const CMEOrderBook &&) = delete;

    private:
        TickerId m_tickerId = TickerId_INVALID;

        /// The parent matching engine instance, used to publish market data and client responses.
        CMatchingEngine* m_pMatchingEngine = nullptr;

        /// Hash map from ClientId -> OrderId -> SMEOrder.
        ClientOrderHashMap m_cidOidToOrder;

        /// Memory pool to manage SMEOrdersAtPrice objects.
        CMemoryPool<SMEOrdersAtPrice> m_ordersAtPricePool;

        /// Pointers to beginning / best prices / top of book of buy and sell price levels.
        SMEOrdersAtPrice* m_pBidsByPrice = nullptr;
        SMEOrdersAtPrice* m_pAsksByPrice = nullptr;

        /// Hash map from Price -> SMEOrdersAtPrice.
        OrdersAtPriceHashMap m_priceOrdersAtPrice;

        /// Memory pool to manage SMEOrder objects.
        CMemoryPool<SMEOrder> m_orderPool;

        /// These are used to publish client responses and market updates.
        SMEClientResponse m_clientResponse;
        SMEMarketUpdate  m_marketUpdate;

        OrderId m_nextMarketOrderId = 1;

        std::string m_timeStr;
        CLogger*    m_pLogger = nullptr;

    private:
        auto GenerateNewMarketOrderId() noexcept -> OrderId
        {
            return m_nextMarketOrderId++;
        }

        auto PriceToIndex(Price price) const noexcept
        {
            return (price % ME_MAX_PRICE_LEVELS);
        }

        /// Fetch and return the SMEOrdersAtPrice corresponding to the provided price.
        auto GetOrdersAtPrice(Price price) const noexcept -> SMEOrdersAtPrice *
        {
            return m_priceOrdersAtPrice.at(PriceToIndex(price));
        }

        /// Add a new SMEOrdersAtPrice at the correct price into the containers - the hash map and the doubly linked list of price levels.
        auto AddOrdersAtPrice(SMEOrdersAtPrice* pNewOrdersAtPrice) noexcept
        {
            m_priceOrdersAtPrice.at(PriceToIndex(pNewOrdersAtPrice->price)) = pNewOrdersAtPrice;

            const auto best_orders_by_price = (pNewOrdersAtPrice->side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice);
            if (UNLIKELY(!best_orders_by_price))
            {
                (pNewOrdersAtPrice->side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = pNewOrdersAtPrice;
                pNewOrdersAtPrice->pPrevEntry = pNewOrdersAtPrice->pNextEntry = pNewOrdersAtPrice;
            }
            else
            {
                auto target = best_orders_by_price;
                bool add_after = ((pNewOrdersAtPrice->side == ESide::SELL && pNewOrdersAtPrice->price > target->price) ||
                                  (pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price < target->price));
                if (add_after)
                {
                    target = target->pNextEntry;
                    add_after = ((pNewOrdersAtPrice->side == ESide::SELL && pNewOrdersAtPrice->price > target->price) ||
                                 (pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price < target->price));
                }
                while (add_after && target != best_orders_by_price)
                {
                    add_after = ((pNewOrdersAtPrice->side == ESide::SELL && pNewOrdersAtPrice->price > target->price) ||
                                 (pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price < target->price));

                    if (add_after)
                    {
                        target = target->pNextEntry;
                    }
                }

                if (add_after)
                { // add pNewOrdersAtPrice after target.
                    if (target == best_orders_by_price)
                    {
                        target = best_orders_by_price->pPrevEntry;
                    }
                    pNewOrdersAtPrice->pPrevEntry = target;
                    target->pNextEntry->pPrevEntry = pNewOrdersAtPrice;
                    pNewOrdersAtPrice->pNextEntry = target->pNextEntry;
                    target->pNextEntry = pNewOrdersAtPrice;
                }
                else
                { // add pNewOrdersAtPrice before target.
                    pNewOrdersAtPrice->pPrevEntry = target->pPrevEntry;
                    pNewOrdersAtPrice->pNextEntry = target;
                    target->pPrevEntry->pNextEntry = pNewOrdersAtPrice;
                    target->pPrevEntry = pNewOrdersAtPrice;

                    if ((pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price > best_orders_by_price->price) ||
                        (pNewOrdersAtPrice->side == ESide::SELL && pNewOrdersAtPrice->price < best_orders_by_price->price))
                    {
                        target->pNextEntry = (target->pNextEntry == best_orders_by_price ? pNewOrdersAtPrice : target->pNextEntry);
                        (pNewOrdersAtPrice->side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = pNewOrdersAtPrice;
                    }
                }
            }
        }

        /// Remove the SMEOrdersAtPrice from the containers - the hash map and the doubly linked list of price levels.
        auto RemoveOrdersAtPrice(ESide side, Price price) noexcept
        {
            const auto best_orders_by_price = (side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice);
            auto orders_at_price = GetOrdersAtPrice(price);

            if (UNLIKELY(orders_at_price->pNextEntry == orders_at_price))
            { // empty side of book.
                (side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = nullptr;
            }
            else
            {
                orders_at_price->pPrevEntry->pNextEntry = orders_at_price->pNextEntry;
                orders_at_price->pNextEntry->pPrevEntry = orders_at_price->pPrevEntry;

                if (orders_at_price == best_orders_by_price)
                {
                    (side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = orders_at_price->pNextEntry;
                }

                orders_at_price->pPrevEntry = orders_at_price->pNextEntry = nullptr;
            }

            m_priceOrdersAtPrice.at(PriceToIndex(price)) = nullptr;

            m_ordersAtPricePool.Deallocate(orders_at_price);
        }

        auto GetNextPriority(Price price) noexcept
        {
            const auto orders_at_price = GetOrdersAtPrice(price);
            if (!orders_at_price)
                return 1lu;

            return orders_at_price->pFirstMeOrder->pPrevOrder->priority + 1;
        }

        /// Match a new aggressive order with the provided parameters against a passive order held in the bid_itr object and generate client responses and market updates for the match.
        /// It will update the passive order (bid_itr) based on the match and possibly remove it if fully matched.
        /// It will return remaining quantity on the aggressive order in the leaves_qty parameter.
        auto Match(TickerId ticker_id, ClientId client_id, ESide side, OrderId client_order_id, OrderId new_market_order_id, SMEOrder *bid_itr, Qty *leaves_qty) noexcept;

        /// Check if a new order with the provided attributes would match against existing passive orders on the other side of the order book.
        /// This will call the match() method to perform the match if there is a match to be made and return the quantity remaining if any on this new order.
        auto CheckForMatch(ClientId client_id, OrderId client_order_id, TickerId ticker_id, ESide side, Price price, Qty qty, Qty new_market_order_id) noexcept;

        /// Remove and de-Allocate provided order from the containers.
        auto RemoveOrder(SMEOrder *order) noexcept
        {
            auto orders_at_price = GetOrdersAtPrice(order->price);

            if (order->pPrevOrder == order)
            { // only one element.
                RemoveOrdersAtPrice(order->side, order->price);
            }
            else
            { // remove the link.
                const auto order_before = order->pPrevOrder;
                const auto order_after = order->pNextOrder;
                order_before->pNextOrder = order_after;
                order_after->pPrevOrder = order_before;

                if (orders_at_price->pFirstMeOrder == order)
                {
                    orders_at_price->pFirstMeOrder = order_after;
                }

                order->pPrevOrder = order->pNextOrder = nullptr;
            }

            m_cidOidToOrder.at(order->clientId).at(order->clientOrderId) = nullptr;
            m_orderPool.Deallocate(order);
        }

        /// Add a single order at the end of the FIFO queue at the price level that this order belongs in.
        auto AddOrder(SMEOrder *order) noexcept
        {
            const auto orders_at_price = GetOrdersAtPrice(order->price);

            if (!orders_at_price)
            {
                order->pNextOrder = order->pPrevOrder = order;

                auto pNewOrdersAtPrice = m_ordersAtPricePool.Allocate(order->side, order->price, order, nullptr, nullptr);
                AddOrdersAtPrice(pNewOrdersAtPrice);
            }
            else
            {
                auto first_order = (orders_at_price ? orders_at_price->pFirstMeOrder : nullptr);

                first_order->pPrevOrder->pNextOrder = order;
                order->pPrevOrder = first_order->pPrevOrder;
                order->pNextOrder = first_order;
                first_order->pPrevOrder = order;
            }

            m_cidOidToOrder.at(order->clientId).at(order->clientOrderId) = order;
        }
    };

    /// A hash map from TickerId -> CMEOrderBook.
    typedef std::array<CMEOrderBook *, ME_MAX_TICKERS> OrderBookHashMap;
}
