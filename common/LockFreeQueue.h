#pragma once

#include <iostream>
#include <vector>
#include <atomic>

#include "Macros.h"

namespace Common
{
    template <typename T>
    class CLockFreeQueue final
    {
    public:
        explicit CLockFreeQueue(std::size_t numElements)
            : m_store(numElements, T()) /* pre-allocation of vector storage. */
        {
        }

        auto GetNextToWriteTo() noexcept
        {
            return &m_store[m_nextWriteIndex];
        }

        auto UpdateWriteIndex() noexcept
        {
            m_nextWriteIndex = (m_nextWriteIndex + 1) % m_store.size();
            m_numElements++;
        }

        auto GetNextToRead() const noexcept -> const T *
        {
            return (size() ? &m_store[m_nextReadIndex] : nullptr);
        }

        auto UpdateReadIndex() noexcept
        {
            m_nextReadIndex = (m_nextReadIndex + 1) % m_store.size(); // wrap around at the end of container size.
            ASSERT(m_numElements != 0, "Read an invalid element in:" + std::to_string(pthread_self()));
            m_numElements--;
        }

        auto size() const noexcept
        {
            return m_numElements.load();
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CLockFreeQueue() = delete;

        CLockFreeQueue(const CLockFreeQueue &) = delete;

        CLockFreeQueue(const CLockFreeQueue &&) = delete;

        CLockFreeQueue &operator=(const CLockFreeQueue &) = delete;

        CLockFreeQueue &operator=(const CLockFreeQueue &&) = delete;

    private:
        /// Underlying container of data accessed in FIFO order.
        std::vector<T> m_store;

        /// Atomic trackers for next index to write new data to and read new data from.
        std::atomic<size_t> m_nextWriteIndex = {0};
        std::atomic<size_t> m_nextReadIndex = {0};

        std::atomic<size_t> m_numElements = {0};
    };
}
