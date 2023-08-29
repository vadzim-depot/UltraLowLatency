#pragma once

#include <functional>
#include <map>

#include "common/ThreadUtils.h"
#include "common/LockFreeQueue.h"
#include "common/Macros.h"
#include "common/MultiCastSocket.h"

#include "exchange/market_data/MarketUpdate.h"

namespace Trading
{
    class CMarketDataConsumer
    {
    public:
        CMarketDataConsumer(Common::ClientId clientId, Exchange::MEMarketUpdateLFQueue* pMarketUpdates, const std::string& iface,
                            const std::string& snapshotIp, int snapshotPort,
                            const std::string& incrementalIp, int incrementalPort);

        ~CMarketDataConsumer()
        {
            Stop();

            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(5s);
        }

        /// Start and stop the market data consumer main thread.
        auto Start()
        {
            m_isRunning = true;
            ASSERT(Common::CreateAndStartThread(-1, "Trading/MarketDataConsumer", [this]()
                                                { Run(); }) != nullptr,
                   "Failed to start MarketData thread.");
        }

        auto Stop() -> void
        {
            m_isRunning = false;
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CMarketDataConsumer() = delete;
        CMarketDataConsumer(const CMarketDataConsumer &) = delete;
        CMarketDataConsumer(const CMarketDataConsumer &&) = delete;
        CMarketDataConsumer &operator=(const CMarketDataConsumer &) = delete;
        CMarketDataConsumer &operator=(const CMarketDataConsumer &&) = delete;

    private:
        /// Track the next expected sequence number on the incremental market data stream, used to detect gaps / drops.
        size_t m_nextExpIncSeqNum = 1;

        /// Lock free queue on which decoded market data updates are pushed to, to be consumed by the trade engine.
        Exchange::MEMarketUpdateLFQueue *m_pIncomingMdUpdates = nullptr;

        volatile bool m_isRunning = false;

        CLogger     m_logger;
        std::string m_timeStr;        

        /// Multicast subscriber sockets for the incremental and market data streams.
        Common::SMultiCastSocket m_incrementalMcastSocket;
        Common::SMultiCastSocket m_snapshotMcastSocket;

        /// Tracks if we are currently in the process of recovering / synchronizing with the snapshot market data stream either because we just started up or we dropped a packet.
        bool m_isInRecovery = false;

        /// Information for the snapshot multicast stream.
        const std::string m_iface;
        const std::string m_snapshotIp;
        const int         m_snapshotPort;

        /// Containers to queue up market data updates from the snapshot and incremental channels, queued up in order of increasing sequence numbers.
        typedef std::map<size_t, Exchange::SMEMarketUpdate> QueuedMarketUpdates;
        QueuedMarketUpdates m_snapshotQueuedMsgs;
        QueuedMarketUpdates m_incrementalQueuedMsgs;

    private:
        /// Main loop for this thread - reads and processes messages from the multicast sockets - the heavy lifting is in the RecvCallback() and checkSnapshotSync() methods.
        auto Run() noexcept -> void;

        /// Process a market data update, the consumer needs to use the socket parameter to figure out whether this came from the snapshot or the incremental stream.
        auto RecvCallback(SMultiCastSocket *socket) noexcept -> void;

        /// Queue up a message in the *_queued_msgs_ containers, first parameter specifies if this update came from the snapshot or the incremental streams.
        auto queueMessage(bool is_snapshot, const Exchange::MDPMarketUpdate *request);

        /// Start the process of snapshot synchronization by subscribing to the snapshot multicast stream.
        auto startSnapshotSync() -> void;

        /// Check if a recovery / synchronization is possible from the queued up market data updates from the snapshot and incremental market data streams.
        auto checkSnapshotSync() -> void;
    };
}
