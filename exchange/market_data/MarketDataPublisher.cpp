#include "MarketDataPublisher.h"

namespace Exchange
{
    CMarketDataPublisher::CMarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const std::string &iface,
                                             const std::string &snapshot_ip, int snapshot_port,
                                             const std::string &incremental_ip, int incremental_port)
        : m_pOutgoingMdUpdates(market_updates), m_snapshotMdUpdates(ME_MAX_MARKET_UPDATES),
          m_isRunning(false), m_logger("exchange_market_data_publisher.log"), m_incrementalSocket(m_logger)
    {
        ASSERT(m_incrementalSocket.Init(incremental_ip, iface, incremental_port, /*is_listening*/ false) >= 0,
               "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));
        m_pSnapshotSynthesizer = new CSnapshotSynthesizer(&m_snapshotMdUpdates, iface, snapshot_ip, snapshot_port);
    }

    /// Main run loop for this thread - consumes market updates from the lock free queue from the matching engine, publishes them on the incremental multicast stream and forwards them to the snapshot synthesizer.
    auto CMarketDataPublisher::Run() noexcept -> void
    {
        m_logger.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr));
        while (m_isRunning)
        {
            for (auto market_update = m_pOutgoingMdUpdates->GetNextToRead();
                 m_pOutgoingMdUpdates->size() && market_update; market_update = m_pOutgoingMdUpdates->GetNextToRead())
            {
                TTT_MEASURE(T5_MarketDataPublisher_LFQueue_read, m_logger);

                m_logger.Log("%:% %() % Sending seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::GetCurrentTimeStr(&m_timeStr), m_nextIncSeqNum,
                            market_update->ToString().c_str());

                START_MEASURE(Exchange_McastSocket_send);
                m_incrementalSocket.Send(&m_nextIncSeqNum, sizeof(m_nextIncSeqNum));
                m_incrementalSocket.Send(market_update, sizeof(SMEMarketUpdate));
                END_MEASURE(Exchange_McastSocket_send, m_logger);

                m_pOutgoingMdUpdates->UpdateReadIndex();
                TTT_MEASURE(T6_MarketDataPublisher_UDP_write, m_logger);

                // Forward this incremental market data update the snapshot synthesizer.
                auto next_write = m_snapshotMdUpdates.GetNextToWriteTo();
                next_write->seqNum = m_nextIncSeqNum;
                next_write->me_market_update_ = *market_update;
                m_snapshotMdUpdates.UpdateWriteIndex();

                ++m_nextIncSeqNum;
            }

            // Publish to the multicast stream.
            m_incrementalSocket.SendAndRecv();
        }
    }
}
