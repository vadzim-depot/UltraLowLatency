#include "SnapshotSynthesizer.h"

namespace Exchange
{
    CSnapshotSynthesizer::CSnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const std::string &iface,
                                             const std::string &snapshot_ip, int snapshot_port)
        : m_snapshotMdUpdates(market_updates), m_logger("exchange_snapshot_synthesizer.log"), m_snapshotSocket(m_logger), m_orderPool(ME_MAX_ORDER_IDS)
    {
        ASSERT(m_snapshotSocket.Init(snapshot_ip, iface, snapshot_port, /*is_listening*/ false) >= 0,
               "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
    }

    CSnapshotSynthesizer::~CSnapshotSynthesizer()
    {
        Stop();
    }

    /// Start and stop the snapshot synthesizer thread.
    void CSnapshotSynthesizer::Start()
    {
        m_isRunning = true;
        ASSERT(Common::CreateAndStartThread(-1, "Exchange/SnapshotSynthesizer", [this]()
                                            { Run(); }) != nullptr,
               "Failed to start SnapshotSynthesizer thread.");
    }

    void CSnapshotSynthesizer::Stop()
    {
        m_isRunning = false;
    }

    /// Process an incremental market update and update the limit order book snapshot.
    auto CSnapshotSynthesizer::AddToSnapshot(const MDPMarketUpdate *market_update)
    {
        const auto &me_market_update = market_update->me_market_update_;
        auto *orders = &m_tickerOrders.at(me_market_update.tickerId);
        switch (me_market_update.type)
        {
            case EMarketUpdateType::ADD:
            {
                auto order = orders->at(me_market_update.orderId);
                ASSERT(order == nullptr, "Received:" + me_market_update.ToString() + " but order already exists:" + (order ? order->ToString() : ""));
                orders->at(me_market_update.orderId) = m_orderPool.Allocate(me_market_update);
            }
            break;
            case EMarketUpdateType::MODIFY:
            {
                auto order = orders->at(me_market_update.orderId);
                ASSERT(order != nullptr, "Received:" + me_market_update.ToString() + " but order does not exist.");
                ASSERT(order->orderId == me_market_update.orderId, "Expecting existing order to match new one.");
                ASSERT(order->side == me_market_update.side, "Expecting existing order to match new one.");

                order->qty = me_market_update.qty;
                order->price = me_market_update.price;
            }
            break;
            case EMarketUpdateType::CANCEL:
            {
                auto order = orders->at(me_market_update.orderId);
                ASSERT(order != nullptr, "Received:" + me_market_update.ToString() + " but order does not exist.");
                ASSERT(order->orderId == me_market_update.orderId, "Expecting existing order to match new one.");
                ASSERT(order->side == me_market_update.side, "Expecting existing order to match new one.");

                m_orderPool.Deallocate(order);
                orders->at(me_market_update.orderId) = nullptr;
            }
            break;
            case EMarketUpdateType::SNAPSHOT_START:
            case EMarketUpdateType::CLEAR:
            case EMarketUpdateType::SNAPSHOT_END:
            case EMarketUpdateType::TRADE:
            case EMarketUpdateType::INVALID:
                break;
        }

        ASSERT(market_update->seqNum == m_lastIncSeqNum + 1, "Expected incremental seq_nums to increase.");
        m_lastIncSeqNum = market_update->seqNum;
    }

    /// Publish a full snapshot cycle on the snapshot multicast stream.
    auto CSnapshotSynthesizer::PublishSnapshot()
    {
        size_t snapshot_size = 0;

        // The snapshot cycle starts with a SNAPSHOT_START message and orderId contains the last sequence number from the incremental market data stream used to build this snapshot.
        const MDPMarketUpdate start_market_update{snapshot_size++, {EMarketUpdateType::SNAPSHOT_START, m_lastIncSeqNum}};
        m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&m_timeStr), start_market_update.ToString());
        m_snapshotSocket.Send(&start_market_update, sizeof(MDPMarketUpdate));

        // Publish order information for each order in the limit order book for each instrument.
        for (size_t ticker_id = 0; ticker_id < m_tickerOrders.size(); ++ticker_id)
        {
            const auto &orders = m_tickerOrders.at(ticker_id);

            SMEMarketUpdate me_market_update;
            me_market_update.type = EMarketUpdateType::CLEAR;
            me_market_update.tickerId = ticker_id;

            // We start order information for each instrument by first publishing a CLEAR message so the downstream consumer can clear the order book.
            const MDPMarketUpdate clear_market_update{snapshot_size++, me_market_update};
            m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&m_timeStr), clear_market_update.ToString());
            m_snapshotSocket.Send(&clear_market_update, sizeof(MDPMarketUpdate));

            // Publish each order.
            for (const auto order : orders)
            {
                if (order)
                {
                    const MDPMarketUpdate market_update{snapshot_size++, *order};
                    m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&m_timeStr), market_update.ToString());
                    m_snapshotSocket.Send(&market_update, sizeof(MDPMarketUpdate));
                    m_snapshotSocket.SendAndRecv();
                }
            }
        }

        // The snapshot cycle ends with a SNAPSHOT_END message and orderId contains the last sequence number from the incremental market data stream used to build this snapshot.
        const MDPMarketUpdate end_market_update{snapshot_size++, {EMarketUpdateType::SNAPSHOT_END, m_lastIncSeqNum}};
        m_logger.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&m_timeStr), end_market_update.ToString());
        m_snapshotSocket.Send(&end_market_update, sizeof(MDPMarketUpdate));
        m_snapshotSocket.SendAndRecv();

        m_logger.Log("%:% %() % Published snapshot of % orders.\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&m_timeStr), snapshot_size - 1);
    }

    /// Main method for this thread - processes incremental updates from the market data publisher, updates the snapshot and publishes the snapshot periodically.
    void CSnapshotSynthesizer::Run()
    {
        m_logger.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&m_timeStr));
        while (m_isRunning)
        {
            for (auto market_update = m_snapshotMdUpdates->GetNextToRead(); m_snapshotMdUpdates->size() && market_update; market_update = m_snapshotMdUpdates->GetNextToRead())
            {
                m_logger.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&m_timeStr),
                            market_update->ToString().c_str());

                AddToSnapshot(market_update);

                m_snapshotMdUpdates->UpdateReadIndex();
            }

            if (GetCurrentNanos() - m_lastSnapshotTime > 60 * NANOS_TO_SECS)
            {
                m_lastSnapshotTime = GetCurrentNanos();
                PublishSnapshot();
            }
        }
    }
}
