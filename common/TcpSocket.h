#pragma once

#include <functional>

#include "SocketUtils.h"
#include "Logging.h"

namespace Common
{
    /// Size of our send and receive buffers in bytes.
    constexpr size_t TCPBufferSize = 64 * 1024 * 1024;

    struct CTCPSocket
    {
        /// Default callback to be used to receive and process data.
        auto DefaultRecvCallback(CTCPSocket *socket, Nanos rx_time) noexcept
        {
            m_logger.Log("%:% %() % CTCPSocket::DefaultRecvCallback() socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::GetCurrentTimeStr(&m_timeStr), socket->m_fd, socket->m_nextRecvValidIndex, rx_time);
        }

        explicit CTCPSocket(CLogger& logger)
            : m_logger(logger)
        {
            m_pSendBuffer = new char[TCPBufferSize];
            m_pRecvBuffer = new char[TCPBufferSize];
            m_recvCallback = [this](auto socket, auto rx_time)
            {
                DefaultRecvCallback(socket, rx_time);
            };
        }

        auto Destroy() -> void;

        ~CTCPSocket()
        {
            Destroy();

            delete[] m_pSendBuffer;
            m_pSendBuffer = nullptr;
            
            delete[] m_pRecvBuffer;
            m_pRecvBuffer = nullptr;
        }

        /// Create CTCPSocket with provided attributes to either listen-on / connect-to.
        auto connect(const std::string &ip, const std::string &iface, int port, bool is_listening) -> int;

        /// Called to publish outgoing data from the buffers as well as check for and callback if data is available in the read buffers.
        auto SendAndRecv() noexcept -> bool;

        /// Write outgoing data to the send buffers.
        auto Send(const void *data, size_t len) noexcept -> void;

        /// Deleted default, copy & move constructors and assignment-operators.
        CTCPSocket() = delete;
        CTCPSocket(const CTCPSocket &) = delete;
        CTCPSocket(const CTCPSocket &&) = delete;
        CTCPSocket &operator=(const CTCPSocket &) = delete;
        CTCPSocket &operator=(const CTCPSocket &&) = delete;

    public:
        int m_fd = -1;

        /// Send and receive buffers and trackers for read/write indices.
        char*  m_pSendBuffer        = nullptr;
        size_t m_nextSendValidIndex = 0;
        char*  m_pRecvBuffer        = nullptr;
        size_t m_nextRecvValidIndex = 0;

        /// To track the state of outgoing or incoming connections.
        bool   m_isSendDisconnected = false;
        bool   m_isRecvDisconnected = false;

        /// Socket attributes.
        struct sockaddr_in m_inInAddr;

        /// Function wrapper to callback when there is data to be processed.
        std::function<void(CTCPSocket *s, Nanos rx_time)> m_recvCallback;

        std::string m_timeStr;
        CLogger&    m_logger;
    };
}
