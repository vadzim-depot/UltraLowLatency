#pragma once

#include <cstdint>
#include <limits>
#include <sstream>
#include <array>

#include "common/Macros.h"

namespace Common
{
    /// Constants used across the ecosystem to represent upper bounds on various containers.
    /// Trading instruments / TickerIds from [0, ME_MAX_TICKERS].
    constexpr size_t ME_MAX_TICKERS = 8;

    /// Maximum size of lock free queues used to transfer client requests, client responses and market updates between components.
    constexpr size_t ME_MAX_CLIENT_UPDATES = 256 * 1024;
    constexpr size_t ME_MAX_MARKET_UPDATES = 256 * 1024;

    /// Maximum trading clients.
    constexpr size_t ME_MAX_NUM_CLIENTS = 256;

    /// Maximum number of orders per trading client.
    constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;

    /// Maximum price level depth in the order books.
    constexpr size_t ME_MAX_PRICE_LEVELS = 256;

    typedef uint64_t OrderId;
    constexpr auto OrderId_INVALID = std::numeric_limits<OrderId>::max();

    inline auto OrderIdToString(OrderId orderId) -> std::string
    {
        if (UNLIKELY(orderId == OrderId_INVALID))
        {
            return "INVALID";
        }

        return std::to_string(orderId);
    }

    typedef uint32_t TickerId;
    constexpr auto TickerId_INVALID = std::numeric_limits<TickerId>::max();

    inline auto TickerIdToString(TickerId tickerId) -> std::string
    {
        if (UNLIKELY(tickerId == TickerId_INVALID))
        {
            return "INVALID";
        }

        return std::to_string(tickerId);
    }

    typedef uint32_t ClientId;
    constexpr auto ClientId_INVALID = std::numeric_limits<ClientId>::max();

    inline auto ClientIdToString(ClientId clientId) -> std::string
    {
        if (UNLIKELY(clientId == ClientId_INVALID))
        {
            return "INVALID";
        }

        return std::to_string(clientId);
    }

    typedef int64_t Price;
    constexpr auto Price_INVALID = std::numeric_limits<Price>::max();

    inline auto PriceToString(Price price) -> std::string
    {
        if (UNLIKELY(price == Price_INVALID))
        {
            return "INVALID";
        }

        return std::to_string(price);
    }

    typedef uint32_t Qty;
    constexpr auto Qty_INVALID = std::numeric_limits<Qty>::max();

    inline auto QtyToString(Qty qty) -> std::string
    {
        if (UNLIKELY(qty == Qty_INVALID))
        {
            return "INVALID";
        }

        return std::to_string(qty);
    }

    /// Priority represents position in the FIFO queue for all orders with the same side and price attributes.
    typedef uint64_t Priority;
    constexpr auto Priority_INVALID = std::numeric_limits<Priority>::max();

    inline auto PriorityToString(Priority priority) -> std::string
    {
        if (UNLIKELY(priority == Priority_INVALID))
        {
            return "INVALID";
        }

        return std::to_string(priority);
    }

    enum class ESide : int8_t
    {
        INVALID = 0,
        BUY = 1,
        SELL = -1,
        MAX = 2
    };

    inline auto SideToString(ESide side) -> std::string
    {
        switch (side)
        {
            case ESide::BUY:
                return "BUY";
            case ESide::SELL:
                return "SELL";
            case ESide::INVALID:
                return "INVALID";
            case ESide::MAX:
                return "MAX";
            }

            return "UNKNOWN";
    }

    /// Convert ESide to an index which can be used to index into a std::array.
    inline constexpr auto SideToIndex(ESide side) noexcept
    {
        return static_cast<size_t>(side) + 1;
    }

    /// Convert ESide::BUY=1 and ESide::SELL=-1.
    inline constexpr auto SideToValue(ESide side) noexcept
    {
        return static_cast<int>(side);
    }

    /// Type of trading algorithm.
    enum class EAlgoType : int8_t
    {
        INVALID = 0,
        RANDOM = 1,
        MAKER = 2,
        TAKER = 3,
        MAX = 4
    };

    inline auto AlgoTypeToString(EAlgoType type) -> std::string
    {
        switch (type)
        {
        case EAlgoType::RANDOM:
            return "RANDOM";
        case EAlgoType::MAKER:
            return "MAKER";
        case EAlgoType::TAKER:
            return "TAKER";
        case EAlgoType::INVALID:
            return "INVALID";
        case EAlgoType::MAX:
            return "MAX";
        }

        return "UNKNOWN";
    }

    inline auto StringToAlgoType(const std::string &str) -> EAlgoType
    {
        for (auto i = static_cast<int>(EAlgoType::INVALID); i <= static_cast<int>(EAlgoType::MAX); ++i)
        {
            const auto algo_type = static_cast<EAlgoType>(i);
            if (AlgoTypeToString(algo_type) == str)
                return algo_type;
        }

        return EAlgoType::INVALID;
    }

    /// Risk configuration containing limits on risk parameters for the CRiskManager.
    struct ERiskCfg
    {
        Qty max_order_size_ = 0;
        Qty max_position_ = 0;
        double max_loss_ = 0;

        auto ToString() const
        {
            std::stringstream ss;

            ss << "ERiskCfg{"
               << "max-order-size:" << QtyToString(max_order_size_) << " "
               << "max-position:" << QtyToString(max_position_) << " "
               << "max-loss:" << max_loss_
               << "}";

            return ss.str();
        }
    };

    /// Top level configuration to configure the CTradeEngine, trading algorithm and CRiskManager.
    struct ETradeEngineCfg
    {
        Qty clip_ = 0;
        double threshold_ = 0;
        ERiskCfg riskCfg;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "ETradeEngineCfg{"
               << "clip:" << QtyToString(clip_) << " "
               << "thresh:" << threshold_ << " "
               << "risk:" << riskCfg.ToString()
               << "}";

            return ss.str();
        }
    };

    /// Hash map from TickerId -> ETradeEngineCfg.
    typedef std::array<ETradeEngineCfg, ME_MAX_TICKERS> TradeEngineCfgHashMap;
}
