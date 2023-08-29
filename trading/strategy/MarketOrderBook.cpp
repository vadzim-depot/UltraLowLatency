#include "MarketOrderBook.h"

#include "TradeEngine.h"

namespace Trading
{
    CMarketOrderBook::CMarketOrderBook(TickerId ticker_id, CLogger *logger)
        : m_tickerId(ticker_id), m_ordersAtPricePool(ME_MAX_PRICE_LEVELS), m_orderPool(ME_MAX_ORDER_IDS), m_logger(logger)
    {
    }

    CMarketOrderBook::~CMarketOrderBook()
    {
        m_logger->Log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__,
                     Common::GetCurrentTimeStr(&m_timeStr), ToString(false, true));

        m_pTradeEngine = nullptr;
        m_pBidsByPrice = m_pAsksByPrice = nullptr;
        m_oidToOrder.fill(nullptr);
    }

    /// Process market data update and update the limit order book.
    auto CMarketOrderBook::OnMarketUpdate(const Exchange::SMEMarketUpdate *market_update) noexcept -> void
    {
        const auto bid_updated = (m_pBidsByPrice && market_update->side == ESide::BUY && market_update->price >= m_pBidsByPrice->price);
        const auto ask_updated = (m_pAsksByPrice && market_update->side == ESide::SELL && market_update->price <= m_pAsksByPrice->price);

        switch (market_update->type)
        {
        case Exchange::EMarketUpdateType::ADD:
        {
            auto order = m_orderPool.Allocate(market_update->orderId, market_update->side, market_update->price,
                                              market_update->qty, market_update->priority, nullptr, nullptr);
            START_MEASURE(Trading_MarketOrderBook_addOrder);
            AddOrder(order);
            END_MEASURE(Trading_MarketOrderBook_addOrder, (*m_logger));
        }
        break;
        case Exchange::EMarketUpdateType::MODIFY:
        {
            auto order = m_oidToOrder.at(market_update->orderId);
            order->qty = market_update->qty;
        }
        break;
        case Exchange::EMarketUpdateType::CANCEL:
        {
            auto order = m_oidToOrder.at(market_update->orderId);
            START_MEASURE(Trading_MarketOrderBook_removeOrder);
            RemoveOrder(order);
            END_MEASURE(Trading_MarketOrderBook_removeOrder, (*m_logger));
        }
        break;
        case Exchange::EMarketUpdateType::TRADE:
        {
            m_pTradeEngine->onTradeUpdate(market_update, this);
            return;
        }
        break;
        case Exchange::EMarketUpdateType::CLEAR:
        { // Clear the full limit order book and Deallocate MarketOrdersAtPrice and MarketOrder objects.
            for (auto &order : m_oidToOrder)
            {
                if (order)
                    m_orderPool.Deallocate(order);
            }
            m_oidToOrder.fill(nullptr);

            if (m_pBidsByPrice)
            {
                for (auto bid = m_pBidsByPrice->pNextEntry; bid != m_pBidsByPrice; bid = bid->pNextEntry)
                    m_ordersAtPricePool.Deallocate(bid);
                m_ordersAtPricePool.Deallocate(m_pBidsByPrice);
            }

            if (m_pAsksByPrice)
            {
                for (auto ask = m_pAsksByPrice->pNextEntry; ask != m_pAsksByPrice; ask = ask->pNextEntry)
                    m_ordersAtPricePool.Deallocate(ask);
                m_ordersAtPricePool.Deallocate(m_pAsksByPrice);
            }

            m_pBidsByPrice = m_pAsksByPrice = nullptr;
        }
        break;
        case Exchange::EMarketUpdateType::INVALID:
        case Exchange::EMarketUpdateType::SNAPSHOT_START:
        case Exchange::EMarketUpdateType::SNAPSHOT_END:
            break;
        }

        START_MEASURE(Trading_MarketOrderBook_updateBBO);
        UpdateBBO(bid_updated, ask_updated);
        END_MEASURE(Trading_MarketOrderBook_updateBBO, (*m_logger));

        m_logger->Log("%:% %() % % %", __FILE__, __LINE__, __FUNCTION__,
                     Common::GetCurrentTimeStr(&m_timeStr), market_update->ToString(), m_pBbo.ToString());

        m_pTradeEngine->onOrderBookUpdate(market_update->tickerId, market_update->price, market_update->side, this);
    }

    auto CMarketOrderBook::ToString(bool detailed, bool validity_check) const -> std::string
    {
        std::stringstream ss;
        std::string time_str;

        auto printer = [&](std::stringstream &ss, SMarketOrdersAtPrice *itr, ESide side, Price &last_price,
                           bool sanity_check)
        {
            char buf[4096];
            Qty qty = 0;
            size_t num_orders = 0;

            for (auto o_itr = itr->pFirstMktOrder;; o_itr = o_itr->pNextOrder)
            {
                qty += o_itr->qty;
                ++num_orders;
                if (o_itr->pNextOrder == itr->pFirstMktOrder)
                    break;
            }
            sprintf(buf, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)",
                    PriceToString(itr->price).c_str(), PriceToString(itr->pPrevEntry->price).c_str(),
                    PriceToString(itr->pNextEntry->price).c_str(),
                    PriceToString(itr->price).c_str(), QtyToString(qty).c_str(), std::to_string(num_orders).c_str());
            ss << buf;
            for (auto o_itr = itr->pFirstMktOrder;; o_itr = o_itr->pNextOrder)
            {
                if (detailed)
                {
                    sprintf(buf, "[oid:%s q:%s p:%s n:%s] ",
                            OrderIdToString(o_itr->orderId).c_str(), QtyToString(o_itr->qty).c_str(),
                            OrderIdToString(o_itr->pPrevOrder ? o_itr->pPrevOrder->orderId : OrderId_INVALID).c_str(),
                            OrderIdToString(o_itr->pNextOrder ? o_itr->pNextOrder->orderId : OrderId_INVALID).c_str());
                    ss << buf;
                }
                if (o_itr->pNextOrder == itr->pFirstMktOrder)
                    break;
            }

            ss << std::endl;

            if (sanity_check)
            {
                if ((side == ESide::SELL && last_price >= itr->price) || (side == ESide::BUY && last_price <= itr->price))
                {
                    FATAL("Bids/Asks not sorted by ascending/descending prices last:" + PriceToString(last_price) + " itr:" +
                          itr->ToString());
                }
                last_price = itr->price;
            }
        };

        ss << "Ticker:" << TickerIdToString(m_tickerId) << std::endl;
        {
            auto ask_itr = m_pAsksByPrice;
            auto last_ask_price = std::numeric_limits<Price>::min();
            for (size_t count = 0; ask_itr; ++count)
            {
                ss << "ASKS L:" << count << " => ";
                auto next_ask_itr = (ask_itr->pNextEntry == m_pAsksByPrice ? nullptr : ask_itr->pNextEntry);
                printer(ss, ask_itr, ESide::SELL, last_ask_price, validity_check);
                ask_itr = next_ask_itr;
            }
        }

        ss << std::endl
           << "                          X" << std::endl
           << std::endl;

        {
            auto bid_itr = m_pBidsByPrice;
            auto last_bid_price = std::numeric_limits<Price>::max();
            for (size_t count = 0; bid_itr; ++count)
            {
                ss << "BIDS L:" << count << " => ";
                auto next_bid_itr = (bid_itr->pNextEntry == m_pBidsByPrice ? nullptr : bid_itr->pNextEntry);
                printer(ss, bid_itr, ESide::BUY, last_bid_price, validity_check);
                bid_itr = next_bid_itr;
            }
        }

        return ss.str();
    }
}
