#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "Macros.h"

namespace Common
{
    template <typename T>
    class CMemoryPool final
    {
    public:
        explicit CMemoryPool(std::size_t numElems) : m_store(numElems, {T(), true}) /* pre-allocation of vector storage. */
        {
            ASSERT(reinterpret_cast<const SObjectBlock *>(&(m_store[0].object)) == &(m_store[0]), "T object should be first member of SObjectBlock.");
        }

        /// Allocate a new object of type T, use placement new to initialize the object, mark the block as in-use and return the object.
        template <typename... Args>
        T* Allocate(Args... args) noexcept
        {
            auto pObjBlock = &(m_store[m_nextFreeIndex]);
            ASSERT(pObjBlock->isFree, "Expected free SObjectBlock at index:" + std::to_string(m_nextFreeIndex));
            T *pRet = &(pObjBlock->object);
            pRet = new (pRet) T(args...); // placement new.
            pObjBlock->isFree = false;

            UpdateNextFreeIndex();
            return pRet;
        }

        /// Return the object back to the pool by marking the block as free again.
        /// Destructor is not called for the object.
        auto Deallocate(const T *elem) noexcept
        {
            const auto elemIndex = (reinterpret_cast<const SObjectBlock *>(elem) - &m_store[0]);

            ASSERT(elemIndex >= 0 && static_cast<size_t>(elemIndex) < m_store.size(), "Element being deallocated does not belong to this Memory pool.");
            ASSERT(!m_store[elemIndex].isFree, "Expected in-use SObjectBlock at index:" + std::to_string(elemIndex));

            m_store[elemIndex].isFree = true;
        }

        // Deleted default, copy & move constructors and assignment-operators.
        CMemoryPool() = delete;

        CMemoryPool(const CMemoryPool &) = delete;

        CMemoryPool(const CMemoryPool &&) = delete;

        CMemoryPool &operator=(const CMemoryPool &) = delete;

        CMemoryPool &operator=(const CMemoryPool &&) = delete;

    private:
        /// Find the next available free block to be used for the next allocation.
        auto UpdateNextFreeIndex() noexcept
        {
            const auto initialFreeIndex = m_nextFreeIndex;
            while (!m_store[m_nextFreeIndex].isFree)
            {
                ++m_nextFreeIndex;
                if (UNLIKELY(m_nextFreeIndex == m_store.size()))
                { // hardware branch predictor should almost always predict this to be false any ways.
                    m_nextFreeIndex = 0;
                }
                if (UNLIKELY(initialFreeIndex == m_nextFreeIndex))
                {
                    ASSERT(initialFreeIndex != m_nextFreeIndex, "Memory Pool out of space.");
                }
            }
        }

        /// It is better to have one vector of structs with two objects than two vectors of one object.
        /// Consider how these are accessed and cache performance.
        struct SObjectBlock
        {
            T object;
            bool isFree = true;
        };

        /// We could've chosen to use a std::array that would Allocate the memory on the stack instead of the heap.
        /// We would have to measure to see which one yields better performance.
        /// It is good to have objects on the stack but performance starts getting worse as the size of the pool increases.
        std::vector<SObjectBlock> m_store;

        size_t m_nextFreeIndex = 0;
    };
}
