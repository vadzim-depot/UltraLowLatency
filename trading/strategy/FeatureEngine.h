#pragma once

#include "common/Macros.h"
#include "common/Logging.h"

using namespace Common;

namespace Trading
{
    /// Sentinel value to represent invalid / uninitialized feature value.
    constexpr auto Feature_INVALID = std::numeric_limits<double>::quiet_NaN();

    class CFeatureEngine
    {
    public:
        CFeatureEngine(Common::CLogger* pLogger)
            : m_pLogger(pLogger)
        {
        }

        /// Process a change in order book and in this case compute the fair market price.
        auto onOrderBookUpdate(TickerId ticker_id, Price price, ESide side, CMarketOrderBook *book) noexcept -> void
        {
            const auto bbo = book->GetBBO();
            if (LIKELY(bbo->bidPrice != Price_INVALID && bbo->askPrice != Price_INVALID))
            {
                mkt_price_ = (bbo->bidPrice * bbo->askQty + bbo->askPrice * bbo->bidQty) / static_cast<double>(bbo->bidQty + bbo->askQty);
            }

            m_pLogger->Log("%:% %() % ticker:% price:% side:% mkt-price:% agg-trade-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                         Common::GetCurrentTimeStr(&m_timeStr), ticker_id, Common::PriceToString(price).c_str(),
                         Common::SideToString(side).c_str(), mkt_price_, agg_trade_qty_ratio_);
        }

        /// Process a trade event and in this case compute the feature to capture aggressive trade quantity ratio against the BBO quantity.
        auto onTradeUpdate(const Exchange::SMEMarketUpdate *market_update, CMarketOrderBook *book) noexcept -> void
        {
            const auto bbo = book->GetBBO();
            if (LIKELY(bbo->bidPrice != Price_INVALID && bbo->askPrice != Price_INVALID))
            {
                agg_trade_qty_ratio_ = static_cast<double>(market_update->qty) / (market_update->side == ESide::BUY ? bbo->askQty : bbo->bidQty);
            }

            m_pLogger->Log("%:% %() % % mkt-price:% agg-trade-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                         Common::GetCurrentTimeStr(&m_timeStr),
                         market_update->ToString().c_str(), mkt_price_, agg_trade_qty_ratio_);
        }

        auto getMktPrice() const noexcept
        {
            return mkt_price_;
        }

        auto getAggTradeQtyRatio() const noexcept
        {
            return agg_trade_qty_ratio_;
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CFeatureEngine() = delete;
        CFeatureEngine(const CFeatureEngine &) = delete;
        CFeatureEngine(const CFeatureEngine &&) = delete;
        CFeatureEngine &operator=(const CFeatureEngine &) = delete;
        CFeatureEngine &operator=(const CFeatureEngine &&) = delete;

    private:
        std::string m_timeStr;
        Common::CLogger* m_pLogger = nullptr;

        /// The two features we compute in our feature engine.
        double mkt_price_ = Feature_INVALID, agg_trade_qty_ratio_ = Feature_INVALID;
    };
}
