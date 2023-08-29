#include "MarketDataConsumer.h"

namespace Trading
{
    CMarketDataConsumer::CMarketDataConsumer(Common::ClientId client_id, Exchange::MEMarketUpdateLFQueue *market_updates,
                                           const std::string &iface,
                                           const std::string &snapshot_ip, int snapshot_port,
                                           const std::string &incremental_ip, int incremental_port)
        : m_pIncomingMdUpdates(market_updates), m_isRunning(false),
          m_logger("trading_market_data_consumer_" + std::to_string(client_id) + ".log"),
          m_incrementalMcastSocket(m_logger), m_snapshotMcastSocket(m_logger),
          m_iface(iface), m_snapshotIp(snapshot_ip), m_snapshotPort(snapshot_port)
    {
        auto recv_callback = [this](auto socket)
        {
            RecvCallback(socket);
        };

        m_incrementalMcastSocket.m_recvCallback = recv_callback;
        ASSERT(m_incrementalMcastSocket.Init(incremental_ip, iface, incremental_port, /*is_listening*/ true) >= 0,
               "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));

        ASSERT(m_incrementalMcastSocket.Join(incremental_ip, iface, incremental_port),
               "Join failed on:" + std::to_string(m_incrementalMcastSocket.m_fd) + " error:" + std::string(std::strerror(errno)));

        m_snapshotMcastSocket.m_recvCallback = recv_callback;
    }

    /// Main loop for this thread - reads and processes messages from the multicast sockets - the heavy lifting is in the RecvCallback() and checkSnapshotSync() methods.
    auto CMarketDataConsumer::Run() noexcept -> void
    {
        m_logger.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
        while (m_isRunning)
        {
            m_incrementalMcastSocket.SendAndRecv();
            m_snapshotMcastSocket.SendAndRecv();
        }
    }

    /// Start the process of snapshot synchronization by subscribing to the snapshot multicast stream.
    auto CMarketDataConsumer::startSnapshotSync() -> void
    {
        m_snapshotQueuedMsgs.clear();
        m_incrementalQueuedMsgs.clear();

        ASSERT(m_snapshotMcastSocket.Init(m_snapshotIp, m_iface, m_snapshotPort, /*is_listening*/ true) >= 0,
               "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
        ASSERT(m_snapshotMcastSocket.Join(m_snapshotIp, m_iface, m_snapshotPort), // IGMP multicast subscription.
               "Join failed on:" + std::to_string(m_snapshotMcastSocket.m_fd) + " error:" + std::string(std::strerror(errno)));
    }

    /// Check if a recovery / synchronization is possible from the queued up market data updates from the snapshot and incremental market data streams.
    auto CMarketDataConsumer::checkSnapshotSync() -> void
    {
        if (m_snapshotQueuedMsgs.empty())
        {
            return;
        }

        const auto &first_snapshot_msg = m_snapshotQueuedMsgs.begin()->second;
        if (first_snapshot_msg.type != Exchange::EMarketUpdateType::SNAPSHOT_START)
        {
            m_logger.Log("%:% %() % Returning because have not seen a SNAPSHOT_START yet.\n",
                        __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
            m_snapshotQueuedMsgs.clear();
            return;
        }

        std::vector<Exchange::SMEMarketUpdate> final_events;

        auto have_complete_snapshot = true;
        size_t next_snapshot_seq = 0;
        for (auto &snapshot_itr : m_snapshotQueuedMsgs)
        {
            m_logger.Log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::GetCurrentTimeStr(&m_timeStr), snapshot_itr.first, snapshot_itr.second.ToString());
            if (snapshot_itr.first != next_snapshot_seq)
            {
                have_complete_snapshot = false;
                m_logger.Log("%:% %() % Detected gap in snapshot stream expected:% found:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                            Common::GetCurrentTimeStr(&m_timeStr), next_snapshot_seq, snapshot_itr.first, snapshot_itr.second.ToString());
                break;
            }

            if (snapshot_itr.second.type != Exchange::EMarketUpdateType::SNAPSHOT_START &&
                snapshot_itr.second.type != Exchange::EMarketUpdateType::SNAPSHOT_END)
                final_events.push_back(snapshot_itr.second);

            ++next_snapshot_seq;
        }

        if (!have_complete_snapshot)
        {
            m_logger.Log("%:% %() % Returning because found gaps in snapshot stream.\n",
                        __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
            m_snapshotQueuedMsgs.clear();
            return;
        }

        const auto &last_snapshot_msg = m_snapshotQueuedMsgs.rbegin()->second;
        if (last_snapshot_msg.type != Exchange::EMarketUpdateType::SNAPSHOT_END)
        {
            m_logger.Log("%:% %() % Returning because have not seen a SNAPSHOT_END yet.\n",
                        __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
            return;
        }

        auto have_complete_incremental = true;
        size_t num_incrementals = 0;
        m_nextExpIncSeqNum = last_snapshot_msg.orderId + 1;
        for (auto inc_itr = m_incrementalQueuedMsgs.begin(); inc_itr != m_incrementalQueuedMsgs.end(); ++inc_itr)
        {
            m_logger.Log("%:% %() % Checking next_exp:% vs. seq:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::GetCurrentTimeStr(&m_timeStr), m_nextExpIncSeqNum, inc_itr->first, inc_itr->second.ToString());

            if (inc_itr->first < m_nextExpIncSeqNum)
                continue;

            if (inc_itr->first != m_nextExpIncSeqNum)
            {
                m_logger.Log("%:% %() % Detected gap in incremental stream expected:% found:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                            Common::GetCurrentTimeStr(&m_timeStr), m_nextExpIncSeqNum, inc_itr->first, inc_itr->second.ToString());
                have_complete_incremental = false;
                break;
            }

            m_logger.Log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::GetCurrentTimeStr(&m_timeStr), inc_itr->first, inc_itr->second.ToString());

            if (inc_itr->second.type != Exchange::EMarketUpdateType::SNAPSHOT_START &&
                inc_itr->second.type != Exchange::EMarketUpdateType::SNAPSHOT_END)
                final_events.push_back(inc_itr->second);

            ++m_nextExpIncSeqNum;
            ++num_incrementals;
        }

        if (!have_complete_incremental)
        {
            m_logger.Log("%:% %() % Returning because have gaps in queued incrementals.\n",
                        __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
            m_snapshotQueuedMsgs.clear();
            return;
        }

        for (const auto &itr : final_events)
        {
            auto next_write = m_pIncomingMdUpdates->GetNextToWriteTo();
            *next_write = itr;
            m_pIncomingMdUpdates->UpdateWriteIndex();
        }

        m_logger.Log("%:% %() % Recovered % snapshot and % incremental orders.\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::GetCurrentTimeStr(&m_timeStr), m_snapshotQueuedMsgs.size() - 2, num_incrementals);

        m_snapshotQueuedMsgs.clear();
        m_incrementalQueuedMsgs.clear();
        m_isInRecovery = false;

        m_snapshotMcastSocket.Leave(m_snapshotIp, m_snapshotPort);
        ;
    }

    /// Queue up a message in the *_queued_msgs_ containers, first parameter specifies if this update came from the snapshot or the incremental streams.
    auto CMarketDataConsumer::queueMessage(bool is_snapshot, const Exchange::MDPMarketUpdate *request)
    {
        if (is_snapshot)
        {
            if (m_snapshotQueuedMsgs.find(request->seqNum) != m_snapshotQueuedMsgs.end())
            {
                m_logger.Log("%:% %() % Packet drops on snapshot socket. Received for a 2nd time:%\n", __FILE__, __LINE__, __FUNCTION__,
                            Common::GetCurrentTimeStr(&m_timeStr), request->ToString());
                m_snapshotQueuedMsgs.clear();
            }
            m_snapshotQueuedMsgs[request->seqNum] = request->me_market_update_;
        }
        else
        {
            m_incrementalQueuedMsgs[request->seqNum] = request->me_market_update_;
        }

        m_logger.Log("%:% %() % size snapshot:% incremental:% % => %\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::GetCurrentTimeStr(&m_timeStr), m_snapshotQueuedMsgs.size(), m_incrementalQueuedMsgs.size(), request->seqNum, request->ToString());

        checkSnapshotSync();
    }

    /// Process a market data update, the consumer needs to use the socket parameter to figure out whether this came from the snapshot or the incremental stream.
    auto CMarketDataConsumer::RecvCallback(SMultiCastSocket *socket) noexcept -> void
    {
        TTT_MEASURE(T7_MarketDataConsumer_UDP_read, m_logger);

        START_MEASURE(Trading_MarketDataConsumer_recvCallback);
        const auto is_snapshot = (socket->m_fd == m_snapshotMcastSocket.m_fd);
        if (UNLIKELY(is_snapshot && !m_isInRecovery))
        { // market update was read from the snapshot market data stream and we are not in recovery, so we dont need it and discard it.
            socket->m_nextRecvValidIndex = 0;

            m_logger.Log("%:% %() % WARN Not expecting snapshot messages.\n",
                        __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));

            return;
        }

        if (socket->m_nextRecvValidIndex >= sizeof(Exchange::MDPMarketUpdate))
        {
            size_t i = 0;
            for (; i + sizeof(Exchange::MDPMarketUpdate) <= socket->m_nextRecvValidIndex; i += sizeof(Exchange::MDPMarketUpdate))
            {
                auto request = reinterpret_cast<const Exchange::MDPMarketUpdate *>(socket->m_pRecvBuffer + i);
                m_logger.Log("%:% %() % Received % socket len:% %\n", __FILE__, __LINE__, __FUNCTION__,
                            Common::GetCurrentTimeStr(&m_timeStr),
                            (is_snapshot ? "snapshot" : "incremental"), sizeof(Exchange::MDPMarketUpdate), request->ToString());

                const bool already_in_recovery = m_isInRecovery;
                m_isInRecovery = (already_in_recovery || request->seqNum != m_nextExpIncSeqNum);

                if (UNLIKELY(m_isInRecovery))
                {
                    if (UNLIKELY(!already_in_recovery))
                    { // if we just entered recovery, start the snapshot synchonization process by subscribing to the snapshot multicast stream.
                        m_logger.Log("%:% %() % Packet drops on % socket. SeqNum expected:% received:%\n", __FILE__, __LINE__, __FUNCTION__,
                                    Common::GetCurrentTimeStr(&m_timeStr), (is_snapshot ? "snapshot" : "incremental"), m_nextExpIncSeqNum, request->seqNum);
                        startSnapshotSync();
                    }

                    queueMessage(is_snapshot, request); // queue up the market data update message and check if snapshot recovery / synchronization can be completed successfully.
                }
                else if (!is_snapshot)
                { // not in recovery and received a packet in the correct order and without gaps, process it.
                    m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                                Common::GetCurrentTimeStr(&m_timeStr), request->ToString());

                    ++m_nextExpIncSeqNum;

                    auto next_write = m_pIncomingMdUpdates->GetNextToWriteTo();
                    *next_write = std::move(request->me_market_update_);
                    m_pIncomingMdUpdates->UpdateWriteIndex();
                    TTT_MEASURE(T8_MarketDataConsumer_LFQueue_write, m_logger);
                }
            }
            memcpy(socket->m_pRecvBuffer, socket->m_pRecvBuffer + i, socket->m_nextRecvValidIndex - i);
            socket->m_nextRecvValidIndex -= i;
        }
        END_MEASURE(Trading_MarketDataConsumer_recvCallback, m_logger);
    }
}
