#pragma once

#include "common/Types.h"
#include "common/MemoryPool.h"
#include "common/Logging.h"

#include "MarketOrder.h"
#include "exchange/market_data/MarketUpdate.h"

namespace Trading
{
    class CTradeEngine;

    class CMarketOrderBook final
    {
    public:
        CMarketOrderBook(TickerId tickerId, CLogger* pLogger);
        ~CMarketOrderBook();

        /// Process market data update and update the limit order book.
        auto OnMarketUpdate(const Exchange::SMEMarketUpdate* pMarketUpdate) noexcept -> void;

        auto SetTradeEngine(CTradeEngine* pTradeEngine)
        {
            m_pTradeEngine = pTradeEngine;
        }

        /// Update the BBO abstraction, the two boolean parameters represent if the buy or the sekk (or both) sides or both need to be updated.
        auto UpdateBBO(bool updateBid, bool updateAsk) noexcept
        {
            if (updateBid)
            {
                if (m_pBidsByPrice)
                {
                    m_pBbo.bidPrice = m_pBidsByPrice->price;
                    m_pBbo.bidQty = m_pBidsByPrice->pFirstMktOrder->qty;
                    for (auto pOrder = m_pBidsByPrice->pFirstMktOrder->pNextOrder; pOrder != m_pBidsByPrice->pFirstMktOrder; pOrder = pOrder->pNextOrder)
                        m_pBbo.bidQty += pOrder->qty;
                }
                else
                {
                    m_pBbo.bidPrice = Price_INVALID;
                    m_pBbo.bidQty = Qty_INVALID;
                }
            }

            if (updateAsk)
            {
                if (m_pAsksByPrice)
                {
                    m_pBbo.askPrice = m_pAsksByPrice->price;
                    m_pBbo.askQty = m_pAsksByPrice->pFirstMktOrder->qty;
                    for (auto pOrder = m_pAsksByPrice->pFirstMktOrder->pNextOrder; pOrder != m_pAsksByPrice->pFirstMktOrder; pOrder = pOrder->pNextOrder)
                        m_pBbo.askQty += pOrder->qty;
                }
                else
                {
                    m_pBbo.askPrice = Price_INVALID;
                    m_pBbo.askQty = Qty_INVALID;
                }
            }
        }

        auto GetBBO() const noexcept -> const SBBO*
        {
            return &m_pBbo;
        }

        auto ToString(bool detailed, bool validity_check) const -> std::string;

        /// Deleted default, copy & move constructors and assignment-operators.
        CMarketOrderBook() = delete;
        CMarketOrderBook(const CMarketOrderBook &) = delete;
        CMarketOrderBook(const CMarketOrderBook &&) = delete;
        CMarketOrderBook &operator=(const CMarketOrderBook &) = delete;
        CMarketOrderBook &operator=(const CMarketOrderBook &&) = delete;

    private:
        const TickerId m_tickerId;

        /// Parent trade engine that owns this limit pOrder book, used to send notifications when book changes or trades occur.
        CTradeEngine* m_pTradeEngine = nullptr;

        /// Hash map from OrderId -> MarketOrder.
        OrderHashMap m_oidToOrder;

        /// Memory pool to manage MarketOrdersAtPrice objects.
        CMemoryPool<SMarketOrdersAtPrice> m_ordersAtPricePool;

        /// Pointers to beginning / best prices / top of book of buy and sell price levels.
        SMarketOrdersAtPrice* m_pBidsByPrice = nullptr;
        SMarketOrdersAtPrice* m_pAsksByPrice = nullptr;

        /// Hash map from Price -> MarketOrdersAtPrice.
        OrdersAtPriceHashMap m_priceOrdersAtPrice;

        /// Memory pool to manage MarketOrder objects.
        CMemoryPool<SMarketOrder> m_orderPool;

        SBBO m_pBbo;

        std::string m_timeStr;
        CLogger*    m_logger = nullptr;

    private:
        auto PriceToIndex(Price price) const noexcept
        {
            return (price % ME_MAX_PRICE_LEVELS);
        }

        /// Fetch and return the MarketOrdersAtPrice corresponding to the provided price.
        auto GetOrdersAtPrice(Price price) const noexcept -> SMarketOrdersAtPrice *
        {
            return m_priceOrdersAtPrice.at(PriceToIndex(price));
        }

        /// Add a new MarketOrdersAtPrice at the correct price into the containers - the hash map and the doubly linked list of price levels.
        auto AddOrdersAtPrice(SMarketOrdersAtPrice* pNewOrdersAtPrice) noexcept
        {
            m_priceOrdersAtPrice.at(PriceToIndex(pNewOrdersAtPrice->price)) = pNewOrdersAtPrice;

            const auto pBestOrdersByPrice = (pNewOrdersAtPrice->side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice);
            if (UNLIKELY(!pBestOrdersByPrice))
            {
                (pNewOrdersAtPrice->side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = pNewOrdersAtPrice;
                pNewOrdersAtPrice->pPrevEntry = pNewOrdersAtPrice->pNextEntry = pNewOrdersAtPrice;
            }
            else
            {
                auto pTarget = pBestOrdersByPrice;
                bool addAfter = ((pNewOrdersAtPrice->side == ESide::SELL && pNewOrdersAtPrice->price > pTarget->price) ||
                                  (pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price < pTarget->price));
                if (addAfter)
                {
                    pTarget = pTarget->pNextEntry;
                    addAfter = ((pNewOrdersAtPrice->side == ESide::SELL && pNewOrdersAtPrice->price > pTarget->price) ||
                                 (pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price < pTarget->price));
                }
                while (addAfter && pTarget != pBestOrdersByPrice)
                {
                    addAfter = ((pNewOrdersAtPrice->side == ESide::SELL && pNewOrdersAtPrice->price > pTarget->price) ||
                                 (pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price < pTarget->price));
                    if (addAfter)
                        pTarget = pTarget->pNextEntry;
                }

                if (addAfter)
                { // add new_orders_at_price after target.
                    if (pTarget == pBestOrdersByPrice)
                    {
                        pTarget = pBestOrdersByPrice->pPrevEntry;
                    }
                    pNewOrdersAtPrice->pPrevEntry = pTarget;
                    pTarget->pNextEntry->pPrevEntry = pNewOrdersAtPrice;
                    pNewOrdersAtPrice->pNextEntry = pTarget->pNextEntry;
                    pTarget->pNextEntry = pNewOrdersAtPrice;
                }
                else
                { // add new_orders_at_price before target.
                    pNewOrdersAtPrice->pPrevEntry = pTarget->pPrevEntry;
                    pNewOrdersAtPrice->pNextEntry = pTarget;
                    pTarget->pPrevEntry->pNextEntry = pNewOrdersAtPrice;
                    pTarget->pPrevEntry = pNewOrdersAtPrice;

                    if ((pNewOrdersAtPrice->side == ESide::BUY && pNewOrdersAtPrice->price > pBestOrdersByPrice->price) ||
                        (pNewOrdersAtPrice->side == ESide::SELL &&
                         pNewOrdersAtPrice->price < pBestOrdersByPrice->price))
                    {
                        pTarget->pNextEntry = (pTarget->pNextEntry == pBestOrdersByPrice ? pNewOrdersAtPrice
                                                                                           : pTarget->pNextEntry);
                        (pNewOrdersAtPrice->side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = pNewOrdersAtPrice;
                    }
                }
            }
        }

        /// Remove the MarketOrdersAtPrice from the containers - the hash map and the doubly linked list of price levels.
        auto RemoveOrdersAtPrice(ESide side, Price price) noexcept
        {
            const auto pBestOrdersByPrice = (side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice);
            auto pOrdersAtPrice = GetOrdersAtPrice(price);

            if (UNLIKELY(pOrdersAtPrice->pNextEntry == pOrdersAtPrice))
            { // empty side of book.
                (side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = nullptr;
            }
            else
            {
                pOrdersAtPrice->pPrevEntry->pNextEntry = pOrdersAtPrice->pNextEntry;
                pOrdersAtPrice->pNextEntry->pPrevEntry = pOrdersAtPrice->pPrevEntry;

                if (pOrdersAtPrice == pBestOrdersByPrice)
                {
                    (side == ESide::BUY ? m_pBidsByPrice : m_pAsksByPrice) = pOrdersAtPrice->pNextEntry;
                }

                pOrdersAtPrice->pPrevEntry = pOrdersAtPrice->pNextEntry = nullptr;
            }

            m_priceOrdersAtPrice.at(PriceToIndex(price)) = nullptr;

            m_ordersAtPricePool.Deallocate(pOrdersAtPrice);
        }

        /// Remove and de-allocate provided pOrder from the containers.
        auto RemoveOrder(SMarketOrder* pOrder) noexcept -> void
        {
            auto pOrdersAtPrice = GetOrdersAtPrice(pOrder->price);

            if (pOrder->pPrevOrder == pOrder) // only one element.
            { 
                RemoveOrdersAtPrice(pOrder->side, pOrder->price);
            }
            else // remove the link.
            {
                const auto order_before = pOrder->pPrevOrder;
                const auto order_after = pOrder->pNextOrder;
                order_before->pNextOrder = order_after;
                order_after->pPrevOrder = order_before;

                if (pOrdersAtPrice->pFirstMktOrder == pOrder)
                {
                    pOrdersAtPrice->pFirstMktOrder = order_after;
                }

                pOrder->pPrevOrder = pOrder->pNextOrder = nullptr;
            }

            m_oidToOrder.at(pOrder->orderId) = nullptr;
            m_orderPool.Deallocate(pOrder);
        }

        /// Add a single order at the end of the FIFO queue at the price level that this pOrder belongs in.
        auto AddOrder(SMarketOrder* pOrder) noexcept -> void
        {
            const auto pOrdersAtPrice = GetOrdersAtPrice(pOrder->price);

            if (!pOrdersAtPrice)
            {
                pOrder->pNextOrder = pOrder->pPrevOrder = pOrder;

                auto pNewOrdersAtPrice = m_ordersAtPricePool.Allocate(pOrder->side, pOrder->price, pOrder, nullptr, nullptr);
                AddOrdersAtPrice(pNewOrdersAtPrice);
            }
            else
            {
                auto pFirstOrder = (pOrdersAtPrice ? pOrdersAtPrice->pFirstMktOrder : nullptr);

                pFirstOrder->pPrevOrder->pNextOrder = pOrder;
                pOrder->pPrevOrder = pFirstOrder->pPrevOrder;
                pOrder->pNextOrder = pFirstOrder;
                pFirstOrder->pPrevOrder = pOrder;
            }

            m_oidToOrder.at(pOrder->orderId) = pOrder;
        }
    };

    /// Hash map from TickerId -> CMarketOrderBook.
    typedef std::array<CMarketOrderBook *, ME_MAX_TICKERS> MarketOrderBookHashMap;
}
