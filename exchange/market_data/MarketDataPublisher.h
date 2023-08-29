#pragma once

#include <functional>

#include "market_data/SnapshotSynthesizer.h"

namespace Exchange
{
    class CMarketDataPublisher
    {
    public:
        CMarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const std::string &iface,
                            const std::string &snapshot_ip, int snapshot_port,
                            const std::string &incremental_ip, int incremental_port);

        ~CMarketDataPublisher()
        {
            Stop();

            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(5s);

            delete m_pSnapshotSynthesizer;
            m_pSnapshotSynthesizer = nullptr;
        }

        /// Start and stop the market data publisher main thread, as well as the internal snapshot synthesizer thread.
        auto Start()
        {
            m_isRunning = true;

            ASSERT(Common::CreateAndStartThread(-1, "Exchange/MarketDataPublisher", [this]()
                                                { Run(); }) != nullptr,
                   "Failed to start MarketData thread.");

            m_pSnapshotSynthesizer->Start();
        }

        auto Stop() -> void
        {
            m_isRunning = false;

            m_pSnapshotSynthesizer->Stop();
        }

        /// Main run loop for this thread - consumes market updates from the lock free queue from the matching engine, publishes them on the incremental multicast stream and forwards them to the snapshot synthesizer.
        auto Run() noexcept -> void;

        // Deleted default, copy & move constructors and assignment-operators.
        CMarketDataPublisher() = delete;
        CMarketDataPublisher(const CMarketDataPublisher &) = delete;
        CMarketDataPublisher(const CMarketDataPublisher &&) = delete;
        CMarketDataPublisher &operator=(const CMarketDataPublisher &) = delete;
        CMarketDataPublisher &operator=(const CMarketDataPublisher &&) = delete;

    private:
        /// Sequencer number tracker on the incremental market data stream.
        size_t m_nextIncSeqNum = 1;

        /// Lock free queue from which we consume market data updates sent by the matching engine.
        MEMarketUpdateLFQueue* m_pOutgoingMdUpdates = nullptr;

        /// Lock free queue on which we forward the incremental market data updates to send to the snapshot synthesizer.
        MDPMarketUpdateLFQueue m_snapshotMdUpdates;

        volatile bool m_isRunning = false;

        std::string m_timeStr;
        CLogger     m_logger;

        /// Multicast socket to represent the incremental market data stream.
        Common::SMultiCastSocket m_incrementalSocket;

        /// Snapshot synthesizer which synthesizes and publishes limit order book snapshots on the snapshot multicast stream.
        CSnapshotSynthesizer* m_pSnapshotSynthesizer = nullptr;
    };
}
