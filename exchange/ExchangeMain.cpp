#include <csignal>

#include "matcher/MatchingEngine.h"
#include "market_data/MarketDataPublisher.h"
#include "order_server/OrderServer.h"

/// Main components, made global to be accessible from the signal handler.
Common::CLogger*                pLogger              = nullptr;
Exchange::CMatchingEngine*      pMatchingEngine      = nullptr;
Exchange::CMarketDataPublisher* pMarketDataPublisher = nullptr;
Exchange::COrderServer*         pOrderServer = nullptr;

/// Shut down gracefully on external signals to this server.
void SignalHandler(int)
{
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10s);

    delete pLogger;    
    pLogger = nullptr;    

    delete pMatchingEngine;
    pMatchingEngine = nullptr;

    delete pMarketDataPublisher;
    pMarketDataPublisher = nullptr;

    delete pOrderServer;
    pOrderServer = nullptr;

    std::this_thread::sleep_for(10s);
    exit(EXIT_SUCCESS);
}

int main(int, char **)
{
    pLogger = new Common::CLogger("exchange_main.log");

    std::signal(SIGINT, SignalHandler);

    const int sleep_time = 100 * 1000;

    // The lock free queues to facilitate communication between order server <-> matching engine and matching engine -> market data publisher.
    Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);

    std::string time_str;

    pLogger->Log("%:% %() % Starting Matching Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&time_str));
    pMatchingEngine = new Exchange::CMatchingEngine(&client_requests, &client_responses, &market_updates);
    pMatchingEngine->Start();

    const std::string mkt_pub_iface = "lo";
    const std::string snap_pub_ip = "233.252.14.1", inc_pub_ip = "233.252.14.3";
    const int snap_pub_port = 20000, inc_pub_port = 20001;

    pLogger->Log("%:% %() % Starting Market Data Publisher...\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&time_str));
    pMarketDataPublisher = new Exchange::CMarketDataPublisher(&market_updates, mkt_pub_iface, snap_pub_ip, snap_pub_port, inc_pub_ip, inc_pub_port);
    pMarketDataPublisher->Start();

    const std::string order_gw_iface = "lo";
    const int order_gw_port = 12345;

    pLogger->Log("%:% %() % Starting Order Server...\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&time_str));
    pOrderServer = new Exchange::COrderServer(&client_requests, &client_responses, order_gw_iface, order_gw_port);
    pOrderServer->Start();

    while (true)
    {
        pLogger->Log("%:% %() % Sleeping for a few milliseconds..\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&time_str));
        usleep(sleep_time * 1000);
    }
}
