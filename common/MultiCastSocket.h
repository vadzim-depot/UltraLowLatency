#pragma once

#include <functional>

#include "SocketUtils.h"

#include "Logging.h"

namespace Common
{
    /// Size of send and receive buffers in bytes.
    constexpr size_t MultiCastBufferSize = 64 * 1024 * 1024;

    struct SMultiCastSocket
    {
        SMultiCastSocket(CLogger &logger)
            : m_logger(logger)
        {
            m_pSendBuffer = new char[MultiCastBufferSize];
            m_pRecvBuffer = new char[MultiCastBufferSize];
            m_recvCallback = [this](auto pSocket)
            {
                DefaultRecvCallback(pSocket);
            };
        }

        ~SMultiCastSocket()
        {
            Destroy();

            delete[] m_pSendBuffer;            
            m_pSendBuffer = nullptr;

            delete[] m_pRecvBuffer;
            m_pRecvBuffer = nullptr;
        }

        /// Initialize multicast socket to read from or publish to a stream.
        /// Does not Join the multicast stream yet.
        auto Init(const std::string& ip, const std::string& iface, int port, bool isListening) -> int;

        auto Destroy() -> void;

        /// Add / Join membership / subscription to a multicast stream.
        auto Join(const std::string& ip, const std::string& iface, int port) -> bool;

        /// Remove / Leave membership / subscription to a multicast stream.
        auto Leave(const std::string& ip, int port) -> void;

        /// Publish outgoing data and read incoming data.
        auto SendAndRecv() noexcept -> bool;

        /// Copy data to send buffers - does not Send them out yet.
        auto Send(const void* data, size_t len) noexcept -> void;

        void DefaultRecvCallback(SMultiCastSocket* pSocket) noexcept
        {
            m_logger.Log("%:% %() % SMultiCastSocket::DefaultRecvCallback() socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), pSocket->m_fd, pSocket->m_nextRecvValidIndex);
        }

        int  m_fd = -1;
        bool m_isSendDisconnected = false;
        bool m_isRecvDisconnected = false;

        /// Send and receive buffers, typically only one or the other is needed, not both.
        char*  m_pSendBuffer        = nullptr;
        size_t m_nextSendValidIndex = 0;
        char*  m_pRecvBuffer        = nullptr;
        size_t m_nextRecvValidIndex = 0;

        /// Function wrapper for the method to call when data is read.
        std::function<void(SMultiCastSocket*)> m_recvCallback;

        std::string m_timeStr;
        CLogger&    m_logger;
    };
}
