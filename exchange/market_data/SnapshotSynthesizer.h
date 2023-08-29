#pragma once

#include "common/Types.h"
#include "common/ThreadUtils.h"
#include "common/LockFreeQueue.h"
#include "common/Macros.h"
#include "common/MultiCastSocket.h"
#include "common/MemoryPool.h"
#include "common/Logging.h"

#include "market_data/MarketUpdate.h"
#include "matcher/MatchingEngineOrder.h"

using namespace Common;

namespace Exchange
{
    class CSnapshotSynthesizer
    {
    public:
        CSnapshotSynthesizer(MDPMarketUpdateLFQueue* market_updates, const std::string &iface,
                            const std::string& snapshot_ip, int snapshot_port);

        ~CSnapshotSynthesizer();

        /// Start and stop the snapshot synthesizer thread.
        auto Start() -> void;

        auto Stop() -> void;

        /// Process an incremental market update and update the limit order book snapshot.
        auto AddToSnapshot(const MDPMarketUpdate* pMarketUpdate);

        /// Publish a full snapshot cycle on the snapshot multicast stream.
        auto PublishSnapshot();

        /// Main method for this thread - processes incremental updates from the market data publisher, updates the snapshot and publishes the snapshot periodically.
        auto Run() -> void;

        /// Deleted default, copy & move constructors and assignment-operators.
        CSnapshotSynthesizer() = delete;
        CSnapshotSynthesizer(const CSnapshotSynthesizer &) = delete;
        CSnapshotSynthesizer(const CSnapshotSynthesizer &&) = delete;
        CSnapshotSynthesizer &operator=(const CSnapshotSynthesizer &) = delete;
        CSnapshotSynthesizer &operator=(const CSnapshotSynthesizer &&) = delete;

    private:
        /// Lock free queue containing incremental market data updates coming in from the market data publisher.
        MDPMarketUpdateLFQueue *m_snapshotMdUpdates = nullptr;

        CLogger m_logger;

        volatile bool m_isRunning = false;

        std::string m_timeStr;

        /// Multicast socket for the snapshot multicast stream.
        SMultiCastSocket m_snapshotSocket;

        /// Hash map from TickerId -> Full limit order book snapshot containing information for every live order.
        std::array<std::array<SMEMarketUpdate *, ME_MAX_ORDER_IDS>, ME_MAX_TICKERS> m_tickerOrders;

        size_t m_lastIncSeqNum = 0;
        Nanos  m_lastSnapshotTime = 0;

        /// Memory pool to manage SMEMarketUpdate messages for the orders in the snapshot limit order books.
        CMemoryPool<SMEMarketUpdate> m_orderPool;
    };
}
