#pragma once

#include <functional>

#include "common/ThreadUtils.h"
#include "common/Macros.h"
#include "common/TcpServer.h"

#include "order_server/ClientRequest.h"
#include "order_server/ClientResponse.h"
#include "order_server/FifoSequencer.h"

namespace Exchange
{
    class COrderServer
    {
    public:
        COrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, const std::string &iface, int port);
        ~COrderServer();

        /// Start and stop the order server main thread.
        auto Start() -> void;
        auto Stop() -> void;

        /// Main run loop for this thread - accepts new client connections, receives client requests from them and sends client responses to them.
        auto Run() noexcept
        {
            m_logger.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
            while (m_isRunning)
            {
                m_tcpServer.Poll();

                m_tcpServer.SendAndRecv();

                for (auto client_response = m_pOutgoingResponses->GetNextToRead(); m_pOutgoingResponses->size() && client_response; client_response = m_pOutgoingResponses->GetNextToRead())
                {
                    TTT_MEASURE(T5t_OrderServer_LFQueue_read, m_logger);

                    auto &next_outgoing_seq_num = m_cidNextOutgoingSeqNum[client_response->clientId];
                    m_logger.Log("%:% %() % Processing cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                                client_response->clientId, next_outgoing_seq_num, client_response->ToString());

                    ASSERT(m_cidTcpSocket[client_response->clientId] != nullptr,
                           "Dont have a CTCPSocket for ClientId:" + std::to_string(client_response->clientId));
                    START_MEASURE(Exchange_TCPSocket_send);
                    m_cidTcpSocket[client_response->clientId]->Send(&next_outgoing_seq_num, sizeof(next_outgoing_seq_num));
                    m_cidTcpSocket[client_response->clientId]->Send(client_response, sizeof(SMEClientResponse));
                    END_MEASURE(Exchange_TCPSocket_send, m_logger);

                    m_pOutgoingResponses->UpdateReadIndex();
                    TTT_MEASURE(T6t_OrderServer_TCP_write, m_logger);

                    ++next_outgoing_seq_num;
                }
            }
        }

        /// Read client request from the TCP receive buffer, check for sequence gaps and forward it to the FIFO sequencer.
        auto RecvCallback(CTCPSocket *socket, Nanos rx_time) noexcept
        {
            TTT_MEASURE(T1_OrderServer_TCP_read, m_logger);
            m_logger.Log("%:% %() % Received socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr),
                        socket->m_fd, socket->m_nextRecvValidIndex, rx_time);

            if (socket->m_nextRecvValidIndex >= sizeof(SOMClientRequest))
            {
                size_t i = 0;
                for (; i + sizeof(SOMClientRequest) <= socket->m_nextRecvValidIndex; i += sizeof(SOMClientRequest))
                {
                    auto request = reinterpret_cast<const SOMClientRequest *>(socket->m_pRecvBuffer + i);
                    m_logger.Log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), request->ToString());

                    if (UNLIKELY(m_cidTcpSocket[request->meClientRequest.clientId] == nullptr))
                    { // first message from this ClientId.
                        m_cidTcpSocket[request->meClientRequest.clientId] = socket;
                    }

                    if (m_cidTcpSocket[request->meClientRequest.clientId] != socket)
                    { // TODO - change this to send a reject back to the client.
                        m_logger.Log("%:% %() % Received ClientRequest from ClientId:% on different socket:% expected:%\n", __FILE__, __LINE__, __FUNCTION__,
                                    Common::GetCurrentTimeStr(&m_timeStr), request->meClientRequest.clientId, socket->m_fd,
                                    m_cidTcpSocket[request->meClientRequest.clientId]->m_fd);
                        continue;
                    }

                    auto &next_exp_seq_num = m_cidNextExpSeqNum[request->meClientRequest.clientId];
                    if (request->seqNum != next_exp_seq_num)
                    { // TODO - change this to send a reject back to the client.
                        m_logger.Log("%:% %() % Incorrect sequence number. ClientId:% SeqNum expected:% received:%\n", __FILE__, __LINE__, __FUNCTION__,
                                    Common::GetCurrentTimeStr(&m_timeStr), request->meClientRequest.clientId, next_exp_seq_num, request->seqNum);
                        continue;
                    }

                    ++next_exp_seq_num;

                    START_MEASURE(Exchange_FIFOSequencer_addClientRequest);
                    m_fifoSequencer.AddClientRequest(rx_time, request->meClientRequest);
                    END_MEASURE(Exchange_FIFOSequencer_addClientRequest, m_logger);
                }
                memcpy(socket->m_pRecvBuffer, socket->m_pRecvBuffer + i, socket->m_nextRecvValidIndex - i);
                socket->m_nextRecvValidIndex -= i;
            }
        }

        /// End of reading incoming messages across all the TCP connections, sequence and publish the client requests to the matching engine.
        auto RecvFinishedCallback() noexcept
        {
            START_MEASURE(Exchange_FIFOSequencer_sequenceAndPublish);
            m_fifoSequencer.SequenceAndPublish();
            END_MEASURE(Exchange_FIFOSequencer_sequenceAndPublish, m_logger);
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        COrderServer() = delete;
        COrderServer(const COrderServer &) = delete;
        COrderServer(const COrderServer &&) = delete;
        COrderServer &operator=(const COrderServer &) = delete;
        COrderServer &operator=(const COrderServer &&) = delete;

    private:
        const std::string m_iface;
        const int         m_port = 0;

        /// Lock free queue of outgoing client responses to be sent out to connected clients.
        ClientResponseLFQueue* m_pOutgoingResponses = nullptr;

        volatile bool m_isRunning = false;

        std::string m_timeStr;
        CLogger     m_logger;

        /// Hash map from ClientId -> the next sequence number to be sent on outgoing client responses.
        std::array<size_t, ME_MAX_NUM_CLIENTS> m_cidNextOutgoingSeqNum;

        /// Hash map from ClientId -> the next sequence number expected on incoming client requests.
        std::array<size_t, ME_MAX_NUM_CLIENTS> m_cidNextExpSeqNum;

        /// Hash map from ClientId -> TCP socket / client connection.
        std::array<Common::CTCPSocket*, ME_MAX_NUM_CLIENTS> m_cidTcpSocket;

        /// TCP server instance listening for new client connections.
        Common::CTCPServer m_tcpServer;

        /// FIFO sequencer responsible for making sure incoming client requests are processed in the order in which they were received.
        CFIFOSequencer m_fifoSequencer;
    };
}
