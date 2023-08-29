#include "OrderGateway.h"

namespace Trading
{
    COrderGateway::COrderGateway(ClientId clientId,
                               Exchange::ClientRequestLFQueue* pClientRequests,
                               Exchange::ClientResponseLFQueue* pClientResponses,
                               std::string ip, const std::string& iface, int port)
        : clientId(clientId)
        , m_ip(ip)
        , m_iface(iface)
        , m_port(port)
        , m_pOutgoingRequests(pClientRequests)
        , m_pIncomingResponses(pClientResponses)
        , m_logger("trading_order_gateway_" + std::to_string(clientId) + ".log")
        , m_tcpSocket(m_logger)
    {
        m_tcpSocket.m_recvCallback = [this](auto socket, auto rx_time)
        { 
            RecvCallback(socket, rx_time); 
        };
    }

    /// Main thread loop - sends out client requests to the exchange and reads and dispatches incoming client responses.
    auto COrderGateway::Run() noexcept -> void
    {
        m_logger.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
        while (m_isRunning)
        {
            m_tcpSocket.SendAndRecv();

            for (auto clientRequest = m_pOutgoingRequests->GetNextToRead(); clientRequest; clientRequest = m_pOutgoingRequests->GetNextToRead())
            {
                TTT_MEASURE(T11_OrderGateway_LFQueue_read, m_logger);

                m_logger.Log("%:% %() % Sending cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), clientId, m_nextOutgoingSeqNum, clientRequest->ToString());
                START_MEASURE(Trading_TCPSocket_send);
                m_tcpSocket.Send(&m_nextOutgoingSeqNum, sizeof(m_nextOutgoingSeqNum));
                m_tcpSocket.Send(clientRequest, sizeof(Exchange::SMEClientRequest));
                END_MEASURE(Trading_TCPSocket_send, m_logger);
                m_pOutgoingRequests->UpdateReadIndex();
                TTT_MEASURE(T12_OrderGateway_TCP_write, m_logger);

                m_nextOutgoingSeqNum++;
            }
        }
    }

    /// Callback when an incoming client response is read, we perform some checks and forward it to the lock free queue connected to the trade engine.
    auto COrderGateway::RecvCallback(CTCPSocket *socket, Nanos rx_time) noexcept -> void
    {
        TTT_MEASURE(T7t_OrderGateway_TCP_read, m_logger);

        START_MEASURE(Trading_OrderGateway_recvCallback);
        m_logger.Log("%:% %() % Received socket:% len:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), socket->m_fd, socket->m_nextRecvValidIndex, rx_time);

        if (socket->m_nextRecvValidIndex >= sizeof(Exchange::SOMClientResponse))
        {
            size_t i = 0;
            for (; i + sizeof(Exchange::SOMClientResponse) <= socket->m_nextRecvValidIndex; i += sizeof(Exchange::SOMClientResponse))
            {
                auto response = reinterpret_cast<const Exchange::SOMClientResponse *>(socket->m_pRecvBuffer + i);
                m_logger.Log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), response->ToString());

                if (response->meClientResponse.clientId != clientId)
                { // this should never happen unless there is a bug at the exchange.
                    m_logger.Log("%:% %() % ERROR Incorrect client id. ClientId expected:% received:%.\n", __FILE__, __LINE__, __FUNCTION__,
                                Common::GetCurrentTimeStr(&m_timeStr), clientId, response->meClientResponse.clientId);
                    continue;
                }
                if (response->seqNum != m_nextExpSeqNum)
                { // this should never happen since we use a reliable TCP protocol, unless there is a bug at the exchange.
                    m_logger.Log("%:% %() % ERROR Incorrect sequence number. ClientId:%. SeqNum expected:% received:%.\n", __FILE__, __LINE__, __FUNCTION__,
                                Common::GetCurrentTimeStr(&m_timeStr), clientId, m_nextExpSeqNum, response->seqNum);
                    continue;
                }

                ++m_nextExpSeqNum;

                auto next_write = m_pIncomingResponses->GetNextToWriteTo();
                *next_write = std::move(response->meClientResponse);
                m_pIncomingResponses->UpdateWriteIndex();
                TTT_MEASURE(T8t_OrderGateway_LFQueue_write, m_logger);
            }
            memcpy(socket->m_pRecvBuffer, socket->m_pRecvBuffer + i, socket->m_nextRecvValidIndex - i);
            socket->m_nextRecvValidIndex -= i;
        }
        END_MEASURE(Trading_OrderGateway_recvCallback, m_logger);
    }
}
