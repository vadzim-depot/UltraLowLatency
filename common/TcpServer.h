#pragma once

#include "TcpSocket.h"

namespace Common
{
    struct CTCPServer
    {
        /// Methods to initialize member function wrappers.
        auto DefaultRecvCallback(CTCPSocket* pSocket, Nanos rx_time) noexcept
        {
            m_logger.Log("%:% %() % CTCPServer::DefaultRecvCallback() socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), pSocket->m_fd, pSocket->m_nextRecvValidIndex, rx_time);
        }

        auto DefaultRecvFinishedCallback() noexcept
        {
            m_logger.Log("%:% %() % CTCPServer::DefaultRecvFinishedCallback()\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
        }

        explicit CTCPServer(CLogger& logger)
            : m_listenerSocket(logger)
            , m_logger(logger)
        {
            m_recvCallback = [this](auto pSocket, auto rx_time)
            {
                DefaultRecvCallback(pSocket, rx_time);
            };
            
            m_recvFinishedCallback = [this]()
            {
                DefaultRecvFinishedCallback();
            };
        }

        /// Start listening for connections on the provided interface and port.
        auto Listen(const std::string &iface, int port) -> void;

        auto Destroy();

        /// Check for new connections or dead connections and update containers that track the sockets.
        auto Poll() noexcept -> void;

        /// Publish outgoing data from the send buffer and read incoming data from the receive buffer.
        auto SendAndRecv() noexcept -> void;

    private:
        /// Add and remove pSocket file descriptors to and from the EPOLL list.
        auto EPollAdd(CTCPSocket* pSocket);

        auto EPollDel(CTCPSocket* pSocket);

        auto Del(CTCPSocket *pSocket);

    public:
        /// Socket on which this server is listening for new connections on.
        int        m_efd = -1;
        CTCPSocket m_listenerSocket;

        epoll_event m_events[1024];

        /// Collection of all sockets, sockets for incoming data, sockets for outgoing data and dead connections.
        std::vector<CTCPSocket*> m_sockets;
        std::vector<CTCPSocket*> m_receiveSockets;
        std::vector<CTCPSocket*> m_sendSockets;
        std::vector<CTCPSocket*> m_disconnectedSockets;

        /// Function wrapper to call back when data is available.
        std::function<void(CTCPSocket *s, Nanos rx_time)> m_recvCallback;

        /// Function wrapper to call back when all data across all TCPSockets has been read and dispatched this round.
        std::function<void()> m_recvFinishedCallback;

        std::string m_timeStr;
        CLogger&    m_logger;
    };
}
