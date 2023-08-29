#pragma once

#include "common/Macros.h"
#include "common/Types.h"
#include "common/Logging.h"

#include "exchange/order_server/ClientResponse.h"

#include "MarketOrderBook.h"

using namespace Common;

namespace Trading
{
    /// SPositionInfo tracks the position, pnl (realized and unrealized) and volume for a single trading instrument.
    struct SPositionInfo
    {
        int32_t position  = 0;
        double  realPnL   = 0;
        double  unrealPnL = 0;
        double  totalPnL  = 0;

        std::array<double, SideToIndex(ESide::MAX) + 1> openVWAP;

        Qty         volume = 0;
        const SBBO* pBbo = nullptr;

        auto ToString() const
        {
            std::stringstream ss;
            ss << "Position{"
               << "pos:" << position
               << " u-pnl:" << unrealPnL
               << " r-pnl:" << realPnL
               << " t-pnl:" << totalPnL
               << " vol:" << QtyToString(volume)
               << " vwaps:[" << (position ? openVWAP.at(SideToIndex(ESide::BUY)) / std::abs(position) : 0)
               << "X" << (position ? openVWAP.at(SideToIndex(ESide::SELL)) / std::abs(position) : 0)
               << "] "
               << (pBbo ? pBbo->ToString() : "") << "}";

            return ss.str();
        }

        /// Process an execution and update the position, pnl and volume.
        auto addFill(const Exchange::SMEClientResponse *pClientResponse, CLogger *pLogger) noexcept
        {
            const auto oldPosition = position;
            const auto sideIndex = SideToIndex(pClientResponse->side);
            const auto oppSideIndex = SideToIndex(pClientResponse->side == ESide::BUY ? ESide::SELL : ESide::BUY);
            const auto sideValue = SideToValue(pClientResponse->side);
            position += pClientResponse->execQty * sideValue;
            volume += pClientResponse->execQty;

            if (oldPosition * SideToValue(pClientResponse->side) >= 0)
            { // opened / increased position.
                openVWAP[sideIndex] += (pClientResponse->price * pClientResponse->execQty);
            }
            else
            { // decreased position.
                const auto opp_side_vwap = openVWAP[oppSideIndex] / std::abs(oldPosition);
                openVWAP[oppSideIndex] = opp_side_vwap * std::abs(position);
                realPnL += std::min(static_cast<int32_t>(pClientResponse->execQty), std::abs(oldPosition)) *
                             (opp_side_vwap - pClientResponse->price) * SideToValue(pClientResponse->side);
                if (position * oldPosition < 0)
                { // flipped position to opposite sign.
                    openVWAP[sideIndex] = (pClientResponse->price * std::abs(position));
                    openVWAP[oppSideIndex] = 0;
                }
            }

            if (!position)
            { // flat
                openVWAP[SideToIndex(ESide::BUY)] = openVWAP[SideToIndex(ESide::SELL)] = 0;
                unrealPnL = 0;
            }
            else
            {
                if (position > 0)
                    unrealPnL =
                        (pClientResponse->price - openVWAP[SideToIndex(ESide::BUY)] / std::abs(position)) *
                        std::abs(position);
                else
                    unrealPnL =
                        (openVWAP[SideToIndex(ESide::SELL)] / std::abs(position) - pClientResponse->price) *
                        std::abs(position);
            }

            totalPnL = unrealPnL + realPnL;

            std::string time_str;
            pLogger->Log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&time_str),
                         ToString(), pClientResponse->ToString().c_str());
        }

        /// Process a change in top-of-book prices (BBO), and update unrealized pnl if there is an open position.
        auto UpdateBBO(const SBBO *pBbo, CLogger *pLogger) noexcept
        {
            std::string time_str;
            pBbo = pBbo;

            if (position && pBbo->bidPrice != Price_INVALID && pBbo->askPrice != Price_INVALID)
            {
                const auto mid_price = (pBbo->bidPrice + pBbo->askPrice) * 0.5;
                if (position > 0)
                    unrealPnL =
                        (mid_price - openVWAP[SideToIndex(ESide::BUY)] / std::abs(position)) *
                        std::abs(position);
                else
                    unrealPnL =
                        (openVWAP[SideToIndex(ESide::SELL)] / std::abs(position) - mid_price) *
                        std::abs(position);

                const auto old_total_pnl = totalPnL;
                totalPnL = unrealPnL + realPnL;

                if (totalPnL != old_total_pnl)
                    pLogger->Log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&time_str),
                                 ToString(), pBbo->ToString());
            }
        }
    };

    /// Top level position keeper class to compute position, pnl and volume for all trading instruments.
    class EPositionKeeper
    {
    public:
        EPositionKeeper(Common::CLogger *pLogger)
            : m_pLogger(pLogger)
        {
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        EPositionKeeper() = delete;
        EPositionKeeper(const EPositionKeeper&) = delete;
        EPositionKeeper(const EPositionKeeper&&) = delete;
        EPositionKeeper& operator=(const EPositionKeeper&) = delete;
        EPositionKeeper& operator=(const EPositionKeeper&&) = delete;

    private:
        std::string      m_timeStr;
        Common::CLogger* m_pLogger = nullptr;

        /// Hash map container from TickerId -> SPositionInfo.
        std::array<SPositionInfo, ME_MAX_TICKERS> m_tickerPosition;

    public:
        auto addFill(const Exchange::SMEClientResponse* pClientResponse) noexcept
        {
            m_tickerPosition.at(pClientResponse->tickerId).addFill(pClientResponse, m_pLogger);
        }

        auto UpdateBBO(TickerId tickerId, const SBBO* pBbo) noexcept
        {
            m_tickerPosition.at(tickerId).UpdateBBO(pBbo, m_pLogger);
        }

        auto getPositionInfo(TickerId tickerId) const noexcept
        {
            return &(m_tickerPosition.at(tickerId));
        }

        auto ToString() const
        {
            double totalPnl = 0;
            Qty totalVol = 0;

            std::stringstream ss;
            for (TickerId i = 0; i < m_tickerPosition.size(); ++i)
            {
                ss << "TickerId:" << TickerIdToString(i) << " " << m_tickerPosition.at(i).ToString() << "\n";

                totalPnl += m_tickerPosition.at(i).totalPnL;
                totalVol += m_tickerPosition.at(i).volume;
            }
            ss << "Total PnL:" << totalPnl << " Vol:" << totalVol << "\n";

            return ss.str();
        }
    };
}
