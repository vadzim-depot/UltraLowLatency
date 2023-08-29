#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "Macros.h"

namespace OptCommon
{
    template <typename T>
    class COptMemPool final
    {
    public:
        explicit COptMemPool(std::size_t numElems)
            : m_store(numElems, {T(), true}) /* pre-allocation of vector storage. */
        {
            ASSERT(reinterpret_cast<const SObjectBlock *>(&(m_store[0].object)) == &(m_store[0]), "T object should be first member of SObjectBlock.");
        }

        /// Allocate a new object of type T, use placement new to initialize the object, mark the block as in-use and return the object.
        template <typename... Args>
        T *Allocate(Args... args) noexcept
        {
            auto obj_block = &(m_store[m_nextFreeIndex]);
#if !defined(NDEBUG)
            ASSERT(obj_block->isFree, "Expected free SObjectBlock at index:" + std::to_string(m_nextFreeIndex));
#endif
            T *ret = &(obj_block->object);
            ret = new (ret) T(args...); // placement new.
            obj_block->isFree = false;

            UpdateNextFreeIndex();

            return ret;
        }

        /// Return the object back to the pool by marking the block as free again.
        /// Destructor is not called for the object.
        auto Deallocate(const T *elem) noexcept
        {
            const auto elem_index = (reinterpret_cast<const SObjectBlock *>(elem) - &m_store[0]);
#if !defined(NDEBUG)
            ASSERT(elem_index >= 0 && static_cast<size_t>(elem_index) < m_store.size(), "Element being deallocated does not belong to this Memory pool.");
            ASSERT(!m_store[elem_index].isFree, "Expected in-use SObjectBlock at index:" + std::to_string(elem_index));
#endif
            m_store[elem_index].isFree = true;
        }

        // Deleted default, copy & move constructors and assignment-operators.
        COptMemPool() = delete;

        COptMemPool(const COptMemPool &) = delete;

        COptMemPool(const COptMemPool &&) = delete;

        COptMemPool &operator=(const COptMemPool &) = delete;

        COptMemPool &operator=(const COptMemPool &&) = delete;

    private:
        /// Find the next available free block to be used for the next allocation.
        auto UpdateNextFreeIndex() noexcept
        {
            const auto initial_free_index = m_nextFreeIndex;
            while (!m_store[m_nextFreeIndex].isFree)
            {
                ++m_nextFreeIndex;
                if (UNLIKELY(m_nextFreeIndex == m_store.size()))
                { // hardware branch predictor should almost always predict this to be false any ways.
                    m_nextFreeIndex = 0;
                }
                if (UNLIKELY(initial_free_index == m_nextFreeIndex))
                {
#if !defined(NDEBUG)
                    ASSERT(initial_free_index != m_nextFreeIndex, "Memory Pool out of space.");
#endif
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
