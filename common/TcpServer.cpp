#include "TcpServer.h"

namespace Common
{
    auto CTCPServer::Destroy()
    {
        close(m_efd);
        m_efd = -1;
        m_listenerSocket.Destroy();
    }

    /// Add and remove socket file descriptors to and from the EPOLL list.
    auto CTCPServer::EPollAdd(CTCPSocket* pSocket)
    {
        epoll_event ev{};
        ev.events = EPOLLET | EPOLLIN;
        ev.data.ptr = reinterpret_cast<void *>(pSocket);
        return (epoll_ctl(m_efd, EPOLL_CTL_ADD, pSocket->m_fd, &ev) != -1);
    }

    auto CTCPServer::EPollDel(CTCPSocket* pSocket)
    {
        return (epoll_ctl(m_efd, EPOLL_CTL_DEL, pSocket->m_fd, nullptr) != -1);
    }

    /// Start listening for connections on the provided interface and port.
    auto CTCPServer::Listen(const std::string& iface, int port) -> void
    {
        Destroy();
        m_efd = epoll_create(1);
        ASSERT(m_efd >= 0, "epoll_create() failed error:" + std::string(std::strerror(errno)));

        ASSERT(m_listenerSocket.connect("", iface, port, true) >= 0,
               "Listener socket failed to connect. iface:" + iface + " port:" + std::to_string(port) + " error:" + std::string(std::strerror(errno)));

        ASSERT(EPollAdd(&m_listenerSocket), "epoll_ctl() failed. error:" + std::string(std::strerror(errno)));
    }

    /// Publish outgoing data from the send buffer and read incoming data from the receive buffer.
    auto CTCPServer::SendAndRecv() noexcept -> void
    {
        bool recv = false;

        for (auto pSocket : m_receiveSockets)
        {
            if (pSocket->SendAndRecv()) // This will dispatch calls to m_recvCallback().
                recv = true;
        }
        if (recv) // There were some events and they have all been dispatched, inform listener.
            m_recvFinishedCallback();

        for (auto pSocket : m_sendSockets)
        {
            pSocket->SendAndRecv();
        }
    }

    auto CTCPServer::Del(CTCPSocket *pSocket)
    {
        EPollDel(pSocket);

        m_sockets.erase(std::remove(m_sockets.begin(), m_sockets.end(), pSocket), m_sockets.end());
        m_receiveSockets.erase(std::remove(m_receiveSockets.begin(), m_receiveSockets.end(), pSocket), m_receiveSockets.end());
        m_sendSockets.erase(std::remove(m_sendSockets.begin(), m_sendSockets.end(), pSocket), m_sendSockets.end());
    }

    /// Check for new connections or dead connections and update containers that track the sockets.
    auto CTCPServer::Poll() noexcept -> void
    {
        const int max_events = 1 + m_sockets.size();

        // Remove sockets which are no longer connected.
        for (auto pSocket : m_disconnectedSockets)
        {
            Del(pSocket);
        }

        const int n = epoll_wait(m_efd, m_events, max_events, 0);
        bool have_new_connection = false;
        for (int i = 0; i < n; ++i)
        {
            epoll_event &event = m_events[i];
            auto pSocket = reinterpret_cast<CTCPSocket *>(event.data.ptr);

            // Check for new connections.
            if (event.events & EPOLLIN)
            {
                if (pSocket == &m_listenerSocket)
                {
                    m_logger.Log("%:% %() % EPOLLIN listener_socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), pSocket->m_fd);
                    have_new_connection = true;
                    continue;
                }
                m_logger.Log("%:% %() % EPOLLIN pSocket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), pSocket->m_fd);
                if (std::find(m_receiveSockets.begin(), m_receiveSockets.end(), pSocket) == m_receiveSockets.end())
                    m_receiveSockets.push_back(pSocket);
            }

            if (event.events & EPOLLOUT)
            {
                m_logger.Log("%:% %() % EPOLLOUT pSocket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), pSocket->m_fd);
                if (std::find(m_sendSockets.begin(), m_sendSockets.end(), pSocket) == m_sendSockets.end())
                    m_sendSockets.push_back(pSocket);
            }

            if (event.events & (EPOLLERR | EPOLLHUP))
            {
                m_logger.Log("%:% %() % EPOLLERR pSocket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), pSocket->m_fd);
                if (std::find(m_receiveSockets.begin(), m_receiveSockets.end(), pSocket) == m_receiveSockets.end())
                    m_receiveSockets.push_back(pSocket);
            }
        }

        // Accept a new connection, create a CTCPSocket and add it to our containers.
        while (have_new_connection)
        {
            m_logger.Log("%:% %() % have_new_connection\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
            sockaddr_storage addr;
            socklen_t addr_len = sizeof(addr);
            int fd = accept(m_listenerSocket.m_fd, reinterpret_cast<sockaddr *>(&addr), &addr_len);
            if (fd == -1)
                break;

            ASSERT(SetNonBlocking(fd) && SetNoDelay(fd), "Failed to set non-blocking or no-delay on pSocket:" + std::to_string(fd));

            m_logger.Log("%:% %() % accepted pSocket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), fd);

            CTCPSocket *pSocket = new CTCPSocket(m_logger);
            pSocket->m_fd = fd;
            pSocket->m_recvCallback = m_recvCallback;
            ASSERT(EPollAdd(pSocket), "Unable to add socket. error:" + std::string(std::strerror(errno)));

            if (std::find(m_sockets.begin(), m_sockets.end(), pSocket) == m_sockets.end())
            {
                m_sockets.push_back(pSocket);
            }

            if (std::find(m_receiveSockets.begin(), m_receiveSockets.end(), pSocket) == m_receiveSockets.end())
            {
                m_receiveSockets.push_back(pSocket);
            }
        }
    }
}
