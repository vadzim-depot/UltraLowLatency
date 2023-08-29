#include "OrderServer.h"

namespace Exchange
{
    COrderServer::COrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, const std::string &iface, int port)
        : m_iface(iface), m_port(port), m_pOutgoingResponses(client_responses), m_logger("exchange_order_server.log"),
          m_tcpServer(m_logger), m_fifoSequencer(client_requests, &m_logger)
    {
        m_cidNextOutgoingSeqNum.fill(1);
        m_cidNextExpSeqNum.fill(1);
        m_cidTcpSocket.fill(nullptr);

        m_tcpServer.m_recvCallback = [this](auto socket, auto rx_time)
        { RecvCallback(socket, rx_time); };
        m_tcpServer.m_recvFinishedCallback = [this]()
        { RecvFinishedCallback(); };
    }

    COrderServer::~COrderServer()
    {
        Stop();

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    /// Start and stop the order server main thread.
    auto COrderServer::Start() -> void
    {
        m_isRunning = true;
        m_tcpServer.Listen(m_iface, m_port);

        ASSERT(Common::CreateAndStartThread(-1, "Exchange/OrderServer", [this]()
                                            { Run(); }) != nullptr,
               "Failed to start OrderServer thread.");
    }

    auto COrderServer::Stop() -> void
    {
        m_isRunning = false;
    }
}
