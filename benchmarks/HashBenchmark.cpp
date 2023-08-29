#include "matcher/MatchingEngine.h"
#include "matcher/UnorderedMapMatchingEngineOrderBook.h"

static constexpr size_t loop_count = 100000;

template <typename T>
size_t benchmarkHashMap(T *order_book, const std::vector<Exchange::SMEClientRequest> &client_requests)
{
    size_t total_rdtsc = 0;

    for (size_t i = 0; i < loop_count; ++i)
    {
        const auto &client_request = client_requests[i];
        switch (client_request.type)
        {
        case Exchange::EClientRequestType::NEW:
        {
            const auto start = Common::rdtsc();
            order_book->AddOrder(client_request.clientId, client_request.orderId, client_request.tickerId,
                            client_request.side, client_request.price, client_request.qty);
            total_rdtsc += (Common::rdtsc() - start);
        }
        break;

        case Exchange::EClientRequestType::CANCEL:
        {
            const auto start = Common::rdtsc();
            order_book->CancelOrder(client_request.clientId, client_request.orderId, client_request.tickerId);
            total_rdtsc += (Common::rdtsc() - start);
        }
        break;

        default:
            break;
        }
    }

    return (total_rdtsc / (loop_count * 2));
}

int main(int, char **)
{
    srand(0);

    Common::CLogger logger("hash_benchmark.log");
    Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);
    auto matching_engine = new Exchange::CMatchingEngine(&client_requests, &client_responses, &market_updates);

    Common::OrderId order_id = 1000;
    std::vector<Exchange::SMEClientRequest> client_requests_vec;
    Price base_price = (rand() % 100) + 100;
    while (client_requests_vec.size() < loop_count)
    {
        const Price price = base_price + (rand() % 10) + 1;
        const Qty qty = 1 + (rand() % 100) + 1;
        const ESide side = (rand() % 2 ? Common::ESide::BUY : Common::ESide::SELL);

        Exchange::SMEClientRequest new_request{Exchange::EClientRequestType::NEW, 0, 0, order_id++, side, price, qty};
        client_requests_vec.push_back(new_request);

        const auto cxl_index = rand() % client_requests_vec.size();
        auto cxl_request = client_requests_vec[cxl_index];
        cxl_request.type = Exchange::EClientRequestType::CANCEL;

        client_requests_vec.push_back(cxl_request);
    }

    {
        auto me_order_book = new Exchange::CMEOrderBook(0, &logger, matching_engine);
        const auto cycles = benchmarkHashMap(me_order_book, client_requests_vec);
        std::cout << "ARRAY HASHMAP " << cycles << " CLOCK CYCLES PER OPERATION." << std::endl;
    }

    {
        auto me_order_book = new Exchange::CUnorderedMapMEOrderBook(0, &logger, matching_engine);
        const auto cycles = benchmarkHashMap(me_order_book, client_requests_vec);
        std::cout << "UNORDERED-MAP HASHMAP " << cycles << " CLOCK CYCLES PER OPERATION." << std::endl;
    }

    exit(EXIT_SUCCESS);
}
