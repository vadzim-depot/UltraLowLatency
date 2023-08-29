#include "LiquidityTaker.h"

#include "TradeEngine.h"

namespace Trading
{
    CLiquidityTaker::CLiquidityTaker(Common::CLogger* pLogger, CTradeEngine* pTradeEngine, const CFeatureEngine* pFeatureEngine,
                                   COrderManager* pOrderManager,
                                   const TradeEngineCfgHashMap& tickerCfg)
        : m_pFeatureEngine(pFeatureEngine)
        , m_pOrderManager(pOrderManager)
        , m_pLogger(pLogger)
        , m_tickerCfg(tickerCfg)
    {
        pTradeEngine->m_algoOnOrderBookUpdate = [this](auto tickerId, auto price, auto side, auto book)
        {
            onOrderBookUpdate(tickerId, price, side, book);
        };
        pTradeEngine->m_algoOnTradeUpdate = [this](auto marketUpdate, auto book)
        {   
            onTradeUpdate(marketUpdate, book);
        };
        pTradeEngine->m_algoOnOrderUpdate = [this](auto clientResponse)
        {
            onOrderUpdate(clientResponse);
        };
    }
}
