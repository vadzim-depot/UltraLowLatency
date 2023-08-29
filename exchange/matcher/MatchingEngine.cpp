#include "MatchingEngine.h"

namespace Exchange
{
    CMatchingEngine::CMatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses,
                                   MEMarketUpdateLFQueue *market_updates)
        : m_pIncomingRequests(client_requests), m_pOutgoingOgwResponses(client_responses), m_pOutgoingMdUpdates(market_updates),
          m_logger("exchange_matching_engine.log")
    {
        for (size_t i = 0; i < m_tickerOrderBook.size(); ++i)
        {
            m_tickerOrderBook[i] = new CMEOrderBook(i, &m_logger, this);
        }
    }

    CMatchingEngine::~CMatchingEngine()
    {
        Stop();

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);

        m_pIncomingRequests = nullptr;
        m_pOutgoingOgwResponses = nullptr;
        m_pOutgoingMdUpdates = nullptr;

        for (auto &order_book : m_tickerOrderBook)
        {
            delete order_book;
            order_book = nullptr;
        }
    }

    /// Start and stop the matching engine main thread.
    auto CMatchingEngine::Start() -> void
    {
        m_isRunning = true;
        ASSERT(Common::CreateAndStartThread(-1, "Exchange/MatchingEngine", [this]()
                                            { Run(); }) != nullptr,
               "Failed to start MatchingEngine thread.");
    }

    auto CMatchingEngine::Stop() -> void
    {
        m_isRunning = false;
    }
}
