#pragma once

#include "common/ThreadUtils.h"
#include "common/LockFreeQueue.h"
#include "common/Macros.h"

#include "order_server/ClientRequest.h"
#include "order_server/ClientResponse.h"
#include "market_data/MarketUpdate.h"

#include "MatchingEngineOrderBook.h"

namespace Exchange
{
    class CMatchingEngine final
    {
    public:
        CMatchingEngine(ClientRequestLFQueue *client_requests,
                       ClientResponseLFQueue *client_responses,
                       MEMarketUpdateLFQueue *market_updates);

        ~CMatchingEngine();

        /// Start and stop the matching engine main thread.
        auto Start() -> void;

        auto Stop() -> void;

        /// Called to process a client request read from the lock free queue sent by the order server.
        auto ProcessClientRequest(const SMEClientRequest *client_request) noexcept
        {
            auto order_book = m_tickerOrderBook[client_request->tickerId];
            switch (client_request->type)
            {
            case EClientRequestType::NEW:
            {
                START_MEASURE(Exchange_MEOrderBook_add);
                order_book->AddOrder(client_request->clientId, client_request->orderId, client_request->tickerId,
                                client_request->side, client_request->price, client_request->qty);
                END_MEASURE(Exchange_MEOrderBook_add, m_logger);
            }
            break;

            case EClientRequestType::CANCEL:
            {
                START_MEASURE(Exchange_MEOrderBook_cancel);
                order_book->CancelOrder(client_request->clientId, client_request->orderId, client_request->tickerId);
                END_MEASURE(Exchange_MEOrderBook_cancel, m_logger);
            }
            break;

            default:
            {
                FATAL("Received invalid client-request-type:" + ClientRequestTypeToString(client_request->type));
            }
            break;
            }
        }

        /// Write client responses to the lock free queue for the order server to consume.
        auto SendClientResponse(const SMEClientResponse *client_response) noexcept
        {
            m_logger.Log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), client_response->ToString());
            auto next_write = m_pOutgoingOgwResponses->GetNextToWriteTo();
            *next_write = std::move(*client_response);
            m_pOutgoingOgwResponses->UpdateWriteIndex();
            TTT_MEASURE(T4t_MatchingEngine_LFQueue_write, m_logger);
        }

        /// Write market data update to the lock free queue for the market data publisher to consume.
        auto SendMarketUpdate(const SMEMarketUpdate *market_update) noexcept
        {
            m_logger.Log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), market_update->ToString());
            auto next_write = m_pOutgoingMdUpdates->GetNextToWriteTo();
            *next_write = *market_update;
            m_pOutgoingMdUpdates->UpdateWriteIndex();
            TTT_MEASURE(T4_MatchingEngine_LFQueue_write, m_logger);
        }

        /// Main loop for this thread - processes incoming client requests which in turn generates client responses and market updates.
        auto Run() noexcept
        {
            m_logger.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
            while (m_isRunning)
            {
                const auto me_client_request = m_pIncomingRequests->GetNextToRead();
                if (LIKELY(me_client_request))
                {
                    TTT_MEASURE(T3_MatchingEngine_LFQueue_read, m_logger);

                    m_logger.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                                me_client_request->ToString());
                    START_MEASURE(Exchange_MatchingEngine_processClientRequest);
                    ProcessClientRequest(me_client_request);
                    END_MEASURE(Exchange_MatchingEngine_processClientRequest, m_logger);
                    m_pIncomingRequests->UpdateReadIndex();
                }
            }
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CMatchingEngine() = delete;
        CMatchingEngine(const CMatchingEngine &) = delete;
        CMatchingEngine(const CMatchingEngine &&) = delete;
        CMatchingEngine &operator=(const CMatchingEngine &) = delete;
        CMatchingEngine &operator=(const CMatchingEngine &&) = delete;

    private:
        /// Hash map container from TickerId -> CMEOrderBook.
        OrderBookHashMap m_tickerOrderBook;

        /// Lock free queues.
        /// One to consume incoming client requests sent by the order server.
        /// Second to publish outgoing client responses to be consumed by the order server.
        /// Third to publish outgoing market updates to be consumed by the market data publisher.
        ClientRequestLFQueue*  m_pIncomingRequests     = nullptr;
        ClientResponseLFQueue* m_pOutgoingOgwResponses = nullptr;
        MEMarketUpdateLFQueue* m_pOutgoingMdUpdates    = nullptr;

        volatile bool m_isRunning = false;

        std::string m_timeStr;
        CLogger m_logger;
    };
}
