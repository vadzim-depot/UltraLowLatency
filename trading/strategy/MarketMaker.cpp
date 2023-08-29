#include "MarketMaker.h"

#include "TradeEngine.h"

namespace Trading
{
    CMarketMaker::CMarketMaker(Common::CLogger *logger, CTradeEngine *pTradeEngine, const CFeatureEngine *feature_engine,
                             COrderManager *order_manager, const TradeEngineCfgHashMap &ticker_cfg)
        : m_pFeatureEngine(feature_engine), m_pOrderManager(order_manager), m_pLogger(logger),
          m_tickerCfg(ticker_cfg)
    {
        pTradeEngine->m_algoOnOrderBookUpdate = [this](auto ticker_id, auto price, auto side, auto book)
        {
            onOrderBookUpdate(ticker_id, price, side, book);
        };
        pTradeEngine->m_algoOnTradeUpdate = [this](auto market_update, auto book)
        { onTradeUpdate(market_update, book); };
        pTradeEngine->m_algoOnOrderUpdate = [this](auto client_response)
        { onOrderUpdate(client_response); };
    }
}
