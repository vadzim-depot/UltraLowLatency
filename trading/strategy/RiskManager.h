#pragma once

#include "common/Macros.h"
#include "common/Logging.h"

#include "PositionKeeper.h"
#include "OrderManagerOrder.h"

using namespace Common;

namespace Trading
{
    class COrderManager;

    /// Enumeration that captures the result of a risk check - ALLOWED means it passed all risk checks, the other values represent the failure reason.
    enum class ERiskCheckResult : int8_t
    {
        INVALID = 0,
        ORDER_TOO_LARGE = 1,
        POSITION_TOO_LARGE = 2,
        LOSS_TOO_LARGE = 3,
        ALLOWED = 4
    };

    inline auto riskCheckResultToString(ERiskCheckResult result)
    {
        switch (result)
        {
        case ERiskCheckResult::INVALID:
            return "INVALID";
        case ERiskCheckResult::ORDER_TOO_LARGE:
            return "ORDER_TOO_LARGE";
        case ERiskCheckResult::POSITION_TOO_LARGE:
            return "POSITION_TOO_LARGE";
        case ERiskCheckResult::LOSS_TOO_LARGE:
            return "LOSS_TOO_LARGE";
        case ERiskCheckResult::ALLOWED:
            return "ALLOWED";
        }

        return "";
    }

    /// Structure that represents the information needed for risk checks for a single trading instrument.
    struct SRiskInfo
    {
        const SPositionInfo* pPositionInfo = nullptr;

        ERiskCfg riskCfg;

        /// Check risk to see if we are allowed to send an order of the specified quantity on the specified side.
        /// Will return a ERiskCheckResult value to convey the output of the risk check.
        auto checkPreTradeRisk(ESide side, Qty qty) const noexcept
        {
            // check order-size
            if (UNLIKELY(qty > riskCfg.max_order_size_))
                return ERiskCheckResult::ORDER_TOO_LARGE;
            if (UNLIKELY(std::abs(pPositionInfo->position + SideToValue(side) * static_cast<int32_t>(qty)) > static_cast<int32_t>(riskCfg.max_position_)))
                return ERiskCheckResult::POSITION_TOO_LARGE;
            if (UNLIKELY(pPositionInfo->totalPnL < riskCfg.max_loss_))
                return ERiskCheckResult::LOSS_TOO_LARGE;

            return ERiskCheckResult::ALLOWED;
        }

        auto ToString() const
        {
            std::stringstream ss;
            ss << "SRiskInfo"
               << "["
               << "pos:" << pPositionInfo->ToString() << " "
               << riskCfg.ToString()
               << "]";

            return ss.str();
        }
    };

    /// Hash map from TickerId -> SRiskInfo.
    typedef std::array<SRiskInfo, ME_MAX_TICKERS> TickerRiskInfoHashMap;

    /// Top level risk manager class to compute and check risk across all trading instruments.
    class CRiskManager
    {
    public:
        CRiskManager(Common::CLogger* pLogger, const EPositionKeeper* pPositionKeeper, const TradeEngineCfgHashMap& tickerCfg);

        auto checkPreTradeRisk(TickerId tickerId, ESide side, Qty qty) const noexcept
        {
            return m_tickerRisk.at(tickerId).checkPreTradeRisk(side, qty);
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CRiskManager() = delete;
        CRiskManager(const CRiskManager &) = delete;
        CRiskManager(const CRiskManager &&) = delete;
        CRiskManager &operator=(const CRiskManager &) = delete;
        CRiskManager &operator=(const CRiskManager &&) = delete;

    private:
        std::string      m_timeStr;
        Common::CLogger* m_pLogger = nullptr;

        /// Hash map container from TickerId -> SRiskInfo.
        TickerRiskInfoHashMap m_tickerRisk;
    };
}
