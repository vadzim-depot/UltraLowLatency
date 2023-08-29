#include "ThreadUtils.h"
#include "LockFreeQueue.h"

struct MyStruct
{
    int data[3];
};

using namespace Common;

auto ConsumeFunction(CLockFreeQueue<MyStruct> *lfq)
{
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(5s);

    while (lfq->size())
    {
        const auto d = lfq->GetNextToRead();
        lfq->UpdateReadIndex();

        std::cout << "ConsumeFunction read elem:" << d->data[0] << "," << d->data[1] << "," << d->data[2] << " lfq-size:" << lfq->size() << std::endl;

        std::this_thread::sleep_for(1s);
    }

    std::cout << "ConsumeFunction exiting." << std::endl;
}

int main(int, char **)
{
    CLockFreeQueue<MyStruct> lfq(20);

    auto ct = CreateAndStartThread(-1, "", ConsumeFunction, &lfq);

    for (auto i = 0; i < 50; ++i)
    {
        const MyStruct d{i, i * 10, i * 100};
        *(lfq.GetNextToWriteTo()) = d;
        lfq.UpdateWriteIndex();

        std::cout << "main constructed elem:" << d.data[0] << "," << d.data[1] << "," << d.data[2] << " lfq-size:" << lfq.size() << std::endl;

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    ct->join();

    std::cout << "main exiting." << std::endl;

    return 0;
}
