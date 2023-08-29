#include "MultiCastSocket.h"

namespace Common
{
    /// Initialize multicast socket to read from or publish to a stream.
    /// Does not Join the multicast stream yet.
    auto SMultiCastSocket::Init(const std::string &ip, const std::string &iface, int port, bool is_listening) -> int
    {
        Destroy();
        m_fd = CreateSocket(m_logger, ip, iface, port, true, false, is_listening, 32, false);
        return m_fd;
    }

    auto SMultiCastSocket::Destroy() -> void
    {
        close(m_fd);
        m_fd = -1;
    }

    /// Add / Join membership / subscription to a multicast stream.
    bool SMultiCastSocket::Join(const std::string &ip, const std::string &iface, int port)
    {
        // TODO: After IGMP-Join finishes need to update poll-fd list.
        return Common::Join(m_fd, ip, iface, port);
    }

    /// Remove / Leave membership / subscription to a multicast stream.
    auto SMultiCastSocket::Leave(const std::string &, int) -> void
    {
        // TODO: Remove from poll-fd list.
        Destroy();
    }

    /// Publish outgoing data and read incoming data.
    auto SMultiCastSocket::SendAndRecv() noexcept -> bool
    {
        // Read data and dispatch callbacks if data is available - non blocking.
        const ssize_t n_rcv = recv(m_fd, m_pRecvBuffer + m_nextRecvValidIndex, MultiCastBufferSize - m_nextRecvValidIndex, MSG_DONTWAIT);
        if (n_rcv > 0)
        {
            m_nextRecvValidIndex += n_rcv;
            m_logger.Log("%:% %() % read socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), m_fd,
                        m_nextRecvValidIndex);
            m_recvCallback(this);
        }

        // Publish market data in the send buffer to the multicast stream.
        ssize_t n_send = std::min(MultiCastBufferSize, m_nextSendValidIndex);
        while (n_send > 0)
        {
            ssize_t n_send_this_msg = std::min(static_cast<ssize_t>(m_nextSendValidIndex), n_send);
            const int flags = MSG_DONTWAIT | MSG_NOSIGNAL | (n_send_this_msg < n_send ? MSG_MORE : 0);
            ssize_t n = ::send(m_fd, m_pSendBuffer, n_send_this_msg, flags);
            if (n < 0)
            {
                if (!WouldBlock())
                {
                    m_isSendDisconnected = true;
                }

                break;
            }

            m_logger.Log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), m_fd, n);

            n_send -= n;
            ASSERT(n == n_send_this_msg, "Don't support partial send lengths yet.");
        }
        m_nextSendValidIndex = 0;

        return (n_rcv > 0);
    }

    /// Copy data to send buffers - does not Send them out yet.
    auto SMultiCastSocket::Send(const void* pData, size_t len) noexcept -> void
    {
        if (len > 0)
        {
            memcpy(m_pSendBuffer + m_nextSendValidIndex, pData, len);
            m_nextSendValidIndex += len;
            ASSERT(m_nextSendValidIndex < MultiCastBufferSize, "Mcast socket buffer filled up and SendAndRecv() not called.");
        }
    }
}
