#pragma once

#include "common/ThreadUtils.h"
#include "common/Macros.h"

#include "order_server/ClientRequest.h"

namespace Exchange
{
    /// Maximum number of unprocessed client request messages across all TCP connections in the order server / FIFO sequencer.
    constexpr size_t ME_MAX_PENDING_REQUESTS = 1024;

    class CFIFOSequencer
    {
    public:
        CFIFOSequencer(ClientRequestLFQueue* pClientRequests, CLogger* pLogger)
            : m_pIncomingRequests(pClientRequests)
            , m_pLogger(pLogger)
        {
        }

        ~CFIFOSequencer()
        {
        }

        /// Queue up a client request, not processed immediately, processed when SequenceAndPublish() is called.
        auto AddClientRequest(Nanos rx_time, const SMEClientRequest& request)
        {
            if (m_pendingSize >= m_pendingClientRequests.size())
            {
                FATAL("Too many pending requests");
            }
            m_pendingClientRequests.at(m_pendingSize++) = std::move(SRecvTimeClientRequest{rx_time, request});
        }

        /// Sort pending client requests in ascending receive time order and then write them to the lock free queue for the matching engine to consume from.
        auto SequenceAndPublish()
        {
            if (UNLIKELY(!m_pendingSize))
                return;

            m_pLogger->Log("%:% %() % Processing % requests.\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), m_pendingSize);

            std::sort(m_pendingClientRequests.begin(), m_pendingClientRequests.begin() + m_pendingSize);

            for (size_t i = 0; i < m_pendingSize; ++i)
            {
                const auto &client_request = m_pendingClientRequests.at(i);

                m_pLogger->Log("%:% %() % Writing RX:% Req:% to FIFO.\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                             client_request.recvTime, client_request.request_.ToString());

                auto next_write = m_pIncomingRequests->GetNextToWriteTo();
                *next_write = std::move(client_request.request_);
                m_pIncomingRequests->UpdateWriteIndex();
                TTT_MEASURE(T2_OrderServer_LFQueue_write, (*m_pLogger));
            }

            m_pendingSize = 0;
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CFIFOSequencer() = delete;
        CFIFOSequencer(const CFIFOSequencer &) = delete;
        CFIFOSequencer(const CFIFOSequencer &&) = delete;
        CFIFOSequencer &operator=(const CFIFOSequencer &) = delete;
        CFIFOSequencer &operator=(const CFIFOSequencer &&) = delete;

    private:
        /// Lock free queue used to publish client requests to, so that the matching engine can consume them.
        ClientRequestLFQueue *m_pIncomingRequests = nullptr;

        std::string m_timeStr;
        CLogger*    m_pLogger = nullptr;

        /// A structure that encapsulates the software receive time as well as the client request.
        struct SRecvTimeClientRequest
        {
            Nanos recvTime = 0;
            SMEClientRequest request_;

            auto operator<(const SRecvTimeClientRequest &rhs) const
            {
                return (recvTime < rhs.recvTime);
            }
        };

        /// Queue of pending client requests, not sorted.
        std::array<SRecvTimeClientRequest, ME_MAX_PENDING_REQUESTS> m_pendingClientRequests;
        size_t                                                      m_pendingSize = 0;
    };
}
