#pragma once

#include <functional>

#include "common/ThreadUtils.h"
#include "common/Macros.h"
#include "common/TcpServer.h"

#include "exchange/order_server/ClientRequest.h"
#include "exchange/order_server/ClientResponse.h"

namespace Trading
{
    class COrderGateway
    {
    public:
        COrderGateway(ClientId client_id,
                     Exchange::ClientRequestLFQueue *client_requests,
                     Exchange::ClientResponseLFQueue *client_responses,
                     std::string ip, const std::string &iface, int port);

        ~COrderGateway()
        {
            Stop();

            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(5s);
        }

        /// Start and stop the order gateway main thread.
        auto Start()
        {
            m_isRunning = true;
            ASSERT(m_tcpSocket.connect(m_ip, m_iface, m_port, false) >= 0,
                   "Unable to connect to ip:" + m_ip + " port:" + std::to_string(m_port) + " on iface:" + m_iface + " error:" + std::string(std::strerror(errno)));
            ASSERT(Common::CreateAndStartThread(-1, "Trading/OrderGateway", [this]()
                                                { Run(); }) != nullptr,
                   "Failed to start OrderGateway thread.");
        }

        auto Stop() -> void
        {
            m_isRunning = false;
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        COrderGateway() = delete;
        COrderGateway(const COrderGateway &) = delete;
        COrderGateway(const COrderGateway &&) = delete;
        COrderGateway &operator=(const COrderGateway &) = delete;
        COrderGateway &operator=(const COrderGateway &&) = delete;

    private:
        const ClientId clientId;

        /// Exchange's order server's TCP server address.
        std::string m_ip;
        const std::string m_iface;
        const int m_port = 0;

        /// Lock free queue on which we consume client requests from the trade engine and forward them to the exchange's order server.
        Exchange::ClientRequestLFQueue* m_pOutgoingRequests = nullptr;

        /// Lock free queue on which we write client responses which we read and processed from the exchange, to be consumed by the trade engine.
        Exchange::ClientResponseLFQueue* m_pIncomingResponses = nullptr;

        volatile bool m_isRunning = false;

        CLogger     m_logger;
        std::string m_timeStr;        

        /// Sequence numbers to track the sequence number to set on outgoing client requests and expected on incoming client responses.
        size_t m_nextOutgoingSeqNum = 1;
        size_t m_nextExpSeqNum = 1;

        /// TCP connection to the exchange's order server.
        Common::CTCPSocket m_tcpSocket;

    private:
        /// Main thread loop - sends out client requests to the exchange and reads and dispatches incoming client responses.
        auto Run() noexcept -> void;

        /// Callback when an incoming client response is read, we perform some checks and forward it to the lock free queue connected to the trade engine.
        auto RecvCallback(CTCPSocket* pSocket, Nanos rxTime) noexcept -> void;
    };
}
