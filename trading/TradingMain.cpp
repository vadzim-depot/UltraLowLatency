#include <csignal>

#include "strategy/TradeEngine.h"
#include "order_gw/OrderGateway.h"
#include "market_data/MarketDataConsumer.h"

#include "common/Logging.h"

/// Main components.
Common::CLogger*              pLogger = nullptr;
Trading::CTradeEngine*        pTradeEngine = nullptr;
Trading::CMarketDataConsumer* pMarketDataConsumer = nullptr;
Trading::COrderGateway*       pOrderGateway = nullptr;

/// ./trading_main CLIENT_ID ALGO_TYPE [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        FATAL("USAGE trading_main CLIENT_ID ALGO_TYPE [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...");
    }

    const Common::ClientId clientId = atoi(argv[1]);
    srand(clientId);

    const auto algoType = StringToAlgoType(argv[2]);

    pLogger = new Common::CLogger("trading_main_" + std::to_string(clientId) + ".log");

    const int sleepTime = 20 * 1000;

    // The lock free queues to facilitate communication between order gateway <-> trade engine and market data consumer -> trade engine.
    Exchange::ClientRequestLFQueue  clientRequests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue clientResponses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue marketUpdates(ME_MAX_MARKET_UPDATES);

    std::string timeStr;

    TradeEngineCfgHashMap tickerCfg;

    // Parse and initialize the TradeEngineCfgHashMap above from the command line arguments.
    // [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...
    size_t nextTickerId = 0;
    for (int i = 3; i < argc; i += 5, ++nextTickerId)
    {
        tickerCfg.at(nextTickerId) = {static_cast<Qty>(std::atoi(argv[i])), std::atof(argv[i + 1]), {static_cast<Qty>(std::atoi(argv[i + 2])), static_cast<Qty>(std::atoi(argv[i + 3])), std::atof(argv[i + 4])}};
    }

    pLogger->Log("%:% %() % Starting Trade Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&timeStr));
    pTradeEngine = new Trading::CTradeEngine(clientId, algoType,
                                            tickerCfg,
                                            &clientRequests,
                                            &clientResponses,
                                            &marketUpdates);
    pTradeEngine->Start();

    const std::string orderGwIp = "127.0.0.1";
    const std::string orderGwIface = "lo";
    const int orderGwPort = 12345;

    pLogger->Log("%:% %() % Starting Order Gateway...\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&timeStr));
    pOrderGateway = new Trading::COrderGateway(clientId, &clientRequests, &clientResponses, orderGwIp, orderGwIface, orderGwPort);
    pOrderGateway->Start();

    const std::string mktDataIface = "lo";
    const std::string snapshotIp = "233.252.14.1";
    const int snapshotPort = 20000;
    const std::string incrementalIp = "233.252.14.3";
    const int incrementalPort = 20001;

    pLogger->Log("%:% %() % Starting Market Data Consumer...\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&timeStr));
    pMarketDataConsumer = new Trading::CMarketDataConsumer(clientId, &marketUpdates, mktDataIface, snapshotIp, snapshotPort, incrementalIp, incrementalPort);
    pMarketDataConsumer->Start();

    usleep(10 * 1000 * 1000);

    pTradeEngine->initLastEventTime();

    // For the random trading algorithm, we simply implement it here instead of creating a new trading algorithm which is another possibility.
    // Generate random orders with random attributes and randomly cancel some of them.
    if (algoType == EAlgoType::RANDOM)
    {
        Common::OrderId orderId = clientId * 1000;
        std::vector<Exchange::SMEClientRequest> clientRequestsVec;
        std::array<Price, ME_MAX_TICKERS> tickerBasePrice;

        for (size_t i = 0; i < ME_MAX_TICKERS; ++i)
        {
            tickerBasePrice[i] = (rand() % 100) + 100;
        }

        for (size_t i = 0; i < 10000; ++i)
        {
            const Common::TickerId tickerId = rand() % Common::ME_MAX_TICKERS;
            const Price price = tickerBasePrice[tickerId] + (rand() % 10) + 1;
            const Qty qty = 1 + (rand() % 100) + 1;
            const ESide side = (rand() % 2 ? Common::ESide::BUY : Common::ESide::SELL);

            Exchange::SMEClientRequest newRequest{Exchange::EClientRequestType::NEW, clientId, tickerId, orderId++, side, price, qty};
            pTradeEngine->sendClientRequest(&newRequest);
            usleep(sleepTime);

            clientRequestsVec.push_back(newRequest);
            const auto cxlIndex = rand() % clientRequestsVec.size();
            auto cxlRequest = clientRequestsVec[cxlIndex];
            cxlRequest.type = Exchange::EClientRequestType::CANCEL;
            pTradeEngine->sendClientRequest(&cxlRequest);
            usleep(sleepTime);

            if (pTradeEngine->silentSeconds() >= 60)
            {
                pLogger->Log("%:% %() % Stopping early because been silent for % seconds...\n", __FILE__, __LINE__, __FUNCTION__,
                            Common::GetCurrentTimeStr(&timeStr), pTradeEngine->silentSeconds());

                break;
            }
        }
    }

    while (pTradeEngine->silentSeconds() < 60)
    {
        pLogger->Log("%:% %() % Waiting till no activity, been silent for % seconds...\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::GetCurrentTimeStr(&timeStr), pTradeEngine->silentSeconds());

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(30s);
    }

    pTradeEngine->Stop();
    pMarketDataConsumer->Stop();
    pOrderGateway->Stop();

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10s);

    delete pLogger;
    pLogger = nullptr;
    delete pTradeEngine;
    pTradeEngine = nullptr;
    delete pMarketDataConsumer;
    pMarketDataConsumer = nullptr;
    delete pOrderGateway;
    pOrderGateway = nullptr;

    std::this_thread::sleep_for(10s);

    exit(EXIT_SUCCESS);
}
