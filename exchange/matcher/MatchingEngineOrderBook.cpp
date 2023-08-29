#include "MatchingEngineOrderBook.h"

#include "matcher/MatchingEngine.h"

namespace Exchange
{
    CMEOrderBook::CMEOrderBook(TickerId ticker_id, CLogger *logger, CMatchingEngine *matching_engine)
        : m_tickerId(ticker_id), m_pMatchingEngine(matching_engine), m_ordersAtPricePool(ME_MAX_PRICE_LEVELS), m_orderPool(ME_MAX_ORDER_IDS),
          m_pLogger(logger)
    {
    }

    CMEOrderBook::~CMEOrderBook()
    {
        m_pLogger->Log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                     ToString(false, true));

        m_pMatchingEngine = nullptr;
        m_pBidsByPrice = m_pAsksByPrice = nullptr;
        for (auto &itr : m_cidOidToOrder)
        {
            itr.fill(nullptr);
        }
    }

    /// Match a new aggressive order with the provided parameters against a passive order held in the bid_itr object and generate client responses and market updates for the match.
    /// It will update the passive order (bid_itr) based on the match and possibly remove it if fully matched.
    /// It will return remaining quantity on the aggressive order in the leaves_qty parameter.
    auto CMEOrderBook::Match(TickerId ticker_id, ClientId client_id, ESide side, OrderId client_order_id, OrderId new_market_order_id, SMEOrder *itr, Qty *leaves_qty) noexcept
    {
        const auto order = itr;
        const auto order_qty = order->qty;
        const auto fill_qty = std::min(*leaves_qty, order_qty);

        *leaves_qty -= fill_qty;
        order->qty -= fill_qty;

        m_clientResponse = {EClientResponseType::FILLED, client_id, ticker_id, client_order_id,
                            new_market_order_id, side, itr->price, fill_qty, *leaves_qty};
        m_pMatchingEngine->SendClientResponse(&m_clientResponse);

        m_clientResponse = {EClientResponseType::FILLED, order->clientId, ticker_id, order->clientOrderId,
                            order->marketOrderId, order->side, itr->price, fill_qty, order->qty};
        m_pMatchingEngine->SendClientResponse(&m_clientResponse);

        m_marketUpdate = {EMarketUpdateType::TRADE, OrderId_INVALID, ticker_id, side, itr->price, fill_qty, Priority_INVALID};
        m_pMatchingEngine->SendMarketUpdate(&m_marketUpdate);

        if (!order->qty)
        {
            m_marketUpdate = {EMarketUpdateType::CANCEL, order->marketOrderId, ticker_id, order->side,
                              order->price, order_qty, Priority_INVALID};
            m_pMatchingEngine->SendMarketUpdate(&m_marketUpdate);

            START_MEASURE(Exchange_MEOrderBook_removeOrder);
            RemoveOrder(order);
            END_MEASURE(Exchange_MEOrderBook_removeOrder, (*m_pLogger));
        }
        else
        {
            m_marketUpdate = {EMarketUpdateType::MODIFY, order->marketOrderId, ticker_id, order->side,
                              order->price, order->qty, order->priority};
            m_pMatchingEngine->SendMarketUpdate(&m_marketUpdate);
        }
    }

    /// Check if a new order with the provided attributes would match against existing passive orders on the other side of the order book.
    /// This will call the match() method to perform the match if there is a match to be made and return the quantity remaining if any on this new order.
    auto CMEOrderBook::CheckForMatch(ClientId client_id, OrderId client_order_id, TickerId ticker_id, ESide side, Price price, Qty qty, Qty new_market_order_id) noexcept
    {
        auto leaves_qty = qty;

        if (side == ESide::BUY)
        {
            while (leaves_qty && m_pAsksByPrice)
            {
                const auto ask_itr = m_pAsksByPrice->pFirstMeOrder;
                if (LIKELY(price < ask_itr->price))
                {
                    break;
                }

                START_MEASURE(Exchange_MEOrderBook_match);
                Match(ticker_id, client_id, side, client_order_id, new_market_order_id, ask_itr, &leaves_qty);
                END_MEASURE(Exchange_MEOrderBook_match, (*m_pLogger));
            }
        }
        if (side == ESide::SELL)
        {
            while (leaves_qty && m_pBidsByPrice)
            {
                const auto bid_itr = m_pBidsByPrice->pFirstMeOrder;
                if (LIKELY(price > bid_itr->price))
                {
                    break;
                }

                START_MEASURE(Exchange_MEOrderBook_match);
                Match(ticker_id, client_id, side, client_order_id, new_market_order_id, bid_itr, &leaves_qty);
                END_MEASURE(Exchange_MEOrderBook_match, (*m_pLogger));
            }
        }

        return leaves_qty;
    }

    /// Create and add a new order in the order book with provided attributes.
    /// It will check to see if this new order matches an existing passive order with opposite side, and perform the matching if that is the case.
    auto CMEOrderBook::AddOrder(ClientId client_id, OrderId client_order_id, TickerId ticker_id, ESide side, Price price, Qty qty) noexcept -> void
    {
        const auto new_market_order_id = GenerateNewMarketOrderId();
        m_clientResponse = {EClientResponseType::ACCEPTED, client_id, ticker_id, client_order_id, new_market_order_id, side, price, 0, qty};
        m_pMatchingEngine->SendClientResponse(&m_clientResponse);

        START_MEASURE(Exchange_MEOrderBook_checkForMatch);
        const auto leaves_qty = CheckForMatch(client_id, client_order_id, ticker_id, side, price, qty, new_market_order_id);
        END_MEASURE(Exchange_MEOrderBook_checkForMatch, (*m_pLogger));

        if (LIKELY(leaves_qty))
        {
            const auto priority = GetNextPriority(price);

            auto order = m_orderPool.Allocate(ticker_id, client_id, client_order_id, new_market_order_id, side, price, leaves_qty, priority, nullptr,
                                              nullptr);
            START_MEASURE(Exchange_MEOrderBook_addOrder);
            AddOrder(order);
            END_MEASURE(Exchange_MEOrderBook_addOrder, (*m_pLogger));

            m_marketUpdate = {EMarketUpdateType::ADD, new_market_order_id, ticker_id, side, price, leaves_qty, priority};
            m_pMatchingEngine->SendMarketUpdate(&m_marketUpdate);
        }
    }

    /// Attempt to cancel an order in the order book, issue a cancel-rejection if order does not exist.
    auto CMEOrderBook::CancelOrder(ClientId client_id, OrderId order_id, TickerId ticker_id) noexcept -> void
    {
        auto is_cancelable = (client_id < m_cidOidToOrder.size());
        SMEOrder *exchange_order = nullptr;
        if (LIKELY(is_cancelable))
        {
            auto &co_itr = m_cidOidToOrder.at(client_id);
            exchange_order = co_itr.at(order_id);
            is_cancelable = (exchange_order != nullptr);
        }

        if (UNLIKELY(!is_cancelable))
        {
            m_clientResponse = {EClientResponseType::CANCEL_REJECTED, client_id, ticker_id, order_id, OrderId_INVALID,
                                ESide::INVALID, Price_INVALID, Qty_INVALID, Qty_INVALID};
        }
        else
        {
            m_clientResponse = {EClientResponseType::CANCELED, client_id, ticker_id, order_id, exchange_order->marketOrderId,
                                exchange_order->side, exchange_order->price, Qty_INVALID, exchange_order->qty};
            m_marketUpdate = {EMarketUpdateType::CANCEL, exchange_order->marketOrderId, ticker_id, exchange_order->side, exchange_order->price, 0,
                              exchange_order->priority};

            START_MEASURE(Exchange_MEOrderBook_removeOrder);
            RemoveOrder(exchange_order);
            END_MEASURE(Exchange_MEOrderBook_removeOrder, (*m_pLogger));

            m_pMatchingEngine->SendMarketUpdate(&m_marketUpdate);
        }

        m_pMatchingEngine->SendClientResponse(&m_clientResponse);
    }

    auto CMEOrderBook::ToString(bool detailed, bool validity_check) const -> std::string
    {
        std::stringstream ss;
        std::string time_str;

        auto printer = [&](std::stringstream &ss, SMEOrdersAtPrice *itr, ESide side, Price &last_price, bool sanity_check)
        {
            char buf[4096];
            Qty qty = 0;
            size_t num_orders = 0;

            for (auto o_itr = itr->pFirstMeOrder;; o_itr = o_itr->pNextOrder)
            {
                qty += o_itr->qty;
                ++num_orders;
                if (o_itr->pNextOrder == itr->pFirstMeOrder)
                    break;
            }
            sprintf(buf, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)",
                    PriceToString(itr->price).c_str(), PriceToString(itr->pPrevEntry->price).c_str(), PriceToString(itr->pNextEntry->price).c_str(),
                    PriceToString(itr->price).c_str(), QtyToString(qty).c_str(), std::to_string(num_orders).c_str());
            ss << buf;
            for (auto o_itr = itr->pFirstMeOrder;; o_itr = o_itr->pNextOrder)
            {
                if (detailed)
                {
                    sprintf(buf, "[oid:%s q:%s p:%s n:%s] ",
                            OrderIdToString(o_itr->marketOrderId).c_str(), QtyToString(o_itr->qty).c_str(),
                            OrderIdToString(o_itr->pPrevOrder ? o_itr->pPrevOrder->marketOrderId : OrderId_INVALID).c_str(),
                            OrderIdToString(o_itr->pNextOrder ? o_itr->pNextOrder->marketOrderId : OrderId_INVALID).c_str());
                    ss << buf;
                }
                if (o_itr->pNextOrder == itr->pFirstMeOrder)
                    break;
            }

            ss << std::endl;

            if (sanity_check)
            {
                if ((side == ESide::SELL && last_price >= itr->price) || (side == ESide::BUY && last_price <= itr->price))
                {
                    FATAL("Bids/Asks not sorted by ascending/descending prices last:" + PriceToString(last_price) + " itr:" + itr->ToString());
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
