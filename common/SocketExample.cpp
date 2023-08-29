#include "TimeUtils.h"
#include "Logging.h"
#include "TcpServer.h"

int main(int, char **)
{
    using namespace Common;

    std::string m_timeStr;
    CLogger m_logger("socket_example.log");

    auto tcpServerRecvCallback = [&](CTCPSocket *socket, Nanos rx_time) noexcept
    {
        m_logger.Log("CTCPServer::DefaultRecvCallback() socket:% len:% rx:%\n",
                    socket->m_fd, socket->m_nextRecvValidIndex, rx_time);

        const std::string reply = "CTCPServer received msg:" + std::string(socket->m_pRecvBuffer, socket->m_nextRecvValidIndex);
        socket->m_nextRecvValidIndex = 0;

        socket->Send(reply.data(), reply.length());
    };

    auto tcpServerRecvFinishedCallback = [&]() noexcept
    {
        m_logger.Log("CTCPServer::DefaultRecvFinishedCallback()\n");
    };

    auto tcpClientRecvCallback = [&](CTCPSocket *socket, Nanos rx_time) noexcept
    {
        const std::string recv_msg = std::string(socket->m_pRecvBuffer, socket->m_nextRecvValidIndex);
        socket->m_nextRecvValidIndex = 0;

        m_logger.Log("CTCPSocket::DefaultRecvCallback() socket:% len:% rx:% msg:%\n",
                    socket->m_fd, socket->m_nextRecvValidIndex, rx_time, recv_msg);
    };

    const std::string iface = "lo";
    const std::string ip = "127.0.0.1";
    const int port = 12345;

    m_logger.Log("Creating CTCPServer on iface:% port:%\n", iface, port);
    CTCPServer server(m_logger);
    server.m_recvCallback = tcpServerRecvCallback;
    server.m_recvFinishedCallback = tcpServerRecvFinishedCallback;
    server.Listen(iface, port);

    std::vector<CTCPSocket *> clients(5);

    for (size_t i = 0; i < clients.size(); ++i)
    {
        clients[i] = new CTCPSocket(m_logger);
        clients[i]->m_recvCallback = tcpClientRecvCallback;

        m_logger.Log("Connecting TCPClient-[%] on ip:% iface:% port:%\n", i, ip, iface, port);
        clients[i]->connect(ip, iface, port, false);
        server.Poll();
    }

    using namespace std::literals::chrono_literals;

    for (auto itr = 0; itr < 5; ++itr)
    {
        for (size_t i = 0; i < clients.size(); ++i)
        {
            const std::string client_msg = "CLIENT-[" + std::to_string(i) + "] : Sending " + std::to_string(itr * 100 + i);
            m_logger.Log("Sending TCPClient-[%] %\n", i, client_msg);
            clients[i]->Send(client_msg.data(), client_msg.length());
            clients[i]->SendAndRecv();

            std::this_thread::sleep_for(500ms);
            server.Poll();
            server.SendAndRecv();
        }
    }

    return 0;
}
