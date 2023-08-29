#include "RiskManager.h"

#include "OrderManager.h"

namespace Trading
{
    CRiskManager::CRiskManager(Common::CLogger* pLogger, const EPositionKeeper* pPositionKeeper, const TradeEngineCfgHashMap& tickerCfg)
        : m_pLogger(pLogger)
    {
        for (TickerId i = 0; i < ME_MAX_TICKERS; ++i)
        {
            m_tickerRisk.at(i).pPositionInfo = pPositionKeeper->getPositionInfo(i);
            m_tickerRisk.at(i).riskCfg = tickerCfg[i].riskCfg;
        }
    }
}
