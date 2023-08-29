#include "TcpSocket.h"

namespace Common
{
    /// Create CTCPSocket with provided attributes to either listen-on / connect-to.
    auto CTCPSocket::connect(const std::string &ip, const std::string &iface, int port, bool is_listening) -> int
    {
        Destroy();
        // Note that needs_so_timestamp=true for CFIFOSequencer.
        m_fd = CreateSocket(m_logger, ip, iface, port, false, false, is_listening, 0, true);

        m_inInAddr.sin_addr.s_addr = INADDR_ANY;
        m_inInAddr.sin_port = htons(port);
        m_inInAddr.sin_family = AF_INET;

        return m_fd;
    }

    auto CTCPSocket::Destroy() -> void
    {
        close(m_fd);
        m_fd = -1;
    }

    /// Called to publish outgoing data from the buffers as well as check for and callback if data is available in the read buffers.
    auto CTCPSocket::SendAndRecv() noexcept -> bool
    {
        char ctrl[CMSG_SPACE(sizeof(struct timeval))];
        struct cmsghdr *cmsg = (struct cmsghdr *)&ctrl;

        struct iovec iov;
        iov.iov_base = m_pRecvBuffer + m_nextRecvValidIndex;
        iov.iov_len = TCPBufferSize - m_nextRecvValidIndex;

        msghdr msg;
        msg.msg_control = ctrl;
        msg.msg_controllen = sizeof(ctrl);
        msg.msg_name = &m_inInAddr;
        msg.msg_namelen = sizeof(m_inInAddr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        // Non-blocking call to read available data.
        const auto n_rcv = recvmsg(m_fd, &msg, MSG_DONTWAIT);
        if (n_rcv > 0)
        {
            m_nextRecvValidIndex += n_rcv;

            Nanos kernel_time = 0;
            struct timeval time_kernel;
            if (cmsg->cmsg_level == SOL_SOCKET &&
                cmsg->cmsg_type == SCM_TIMESTAMP &&
                cmsg->cmsg_len == CMSG_LEN(sizeof(time_kernel)))
            {
                memcpy(&time_kernel, CMSG_DATA(cmsg), sizeof(time_kernel));
                kernel_time = time_kernel.tv_sec * NANOS_TO_SECS + time_kernel.tv_usec * NANOS_TO_MICROS; // convert timestamp to nanoseconds.
            }

            const auto user_time = GetCurrentNanos();

            m_logger.Log("%:% %() % read socket:% len:% utime:% ktime:% diff:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::GetCurrentTimeStr(&m_timeStr), m_fd, m_nextRecvValidIndex, user_time, kernel_time, (user_time - kernel_time));
            m_recvCallback(this, kernel_time);
        }

        ssize_t n_send = std::min(TCPBufferSize, m_nextSendValidIndex);
        while (n_send > 0)
        {
            auto n_send_this_msg = std::min(static_cast<ssize_t>(m_nextSendValidIndex), n_send);
            const int flags = MSG_DONTWAIT | MSG_NOSIGNAL | (n_send_this_msg < n_send ? MSG_MORE : 0);

            // Non-blocking call to send data.
            auto n = ::send(m_fd, m_pSendBuffer, n_send_this_msg, flags);
            if (UNLIKELY(n < 0))
            {
                if (!WouldBlock())
                    m_isSendDisconnected = true;
                break;
            }

            m_logger.Log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), m_fd, n);

            n_send -= n;
            ASSERT(n == n_send_this_msg, "Don't support partial send lengths yet.");
        }
        m_nextSendValidIndex = 0;

        return (n_rcv > 0);
    }

    /// Write outgoing data to the send buffers.
    auto CTCPSocket::Send(const void *data, size_t len) noexcept -> void
    {
        if (len > 0)
        {
            memcpy(m_pSendBuffer + m_nextSendValidIndex, data, len);
            m_nextSendValidIndex += len;
        }
    }
}
