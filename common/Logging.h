#pragma once

#include <string>
#include <fstream>
#include <cstdio>

#include "Macros.h"
#include "LockFreeQueue.h"
#include "ThreadUtils.h"
#include "TimeUtils.h"

namespace Common
{
    /// Maximum size of the lock free queue of data to be logged.
    constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;

    /// Type of SLogElement message.
    enum class ELogType : int8_t
    {
        CHAR = 0,
        INTEGER = 1,
        LONG_INTEGER = 2,
        LONG_LONG_INTEGER = 3,
        UNSIGNED_INTEGER = 4,
        UNSIGNED_LONG_INTEGER = 5,
        UNSIGNED_LONG_LONG_INTEGER = 6,
        FLOAT = 7,
        DOUBLE = 8
    };

    /// Represents a single and primitive log entry.
    struct SLogElement
    {
        ELogType type = ELogType::CHAR;
        union
        {
            char c;
            int i;
            long l;
            long long ll;
            unsigned u;
            unsigned long ul;
            unsigned long long ull;
            float f;
            double d;
        } u_;
    };

    class CLogger final
    {
    public:
        /// Consumes from the lock free queue of log entries and writes to the output log file.
        auto FlushQueue() noexcept
        {
            while (m_isRrunning)
            {
                for (auto next = m_queue.GetNextToRead(); m_queue.size() && next; next = m_queue.GetNextToRead())
                {
                    switch (next->type)
                    {
                        case ELogType::CHAR:
                            m_file << next->u_.c;
                            break;
                        case ELogType::INTEGER:
                            m_file << next->u_.i;
                            break;
                        case ELogType::LONG_INTEGER:
                            m_file << next->u_.l;
                            break;
                        case ELogType::LONG_LONG_INTEGER:
                            m_file << next->u_.ll;
                            break;
                        case ELogType::UNSIGNED_INTEGER:
                            m_file << next->u_.u;
                            break;
                        case ELogType::UNSIGNED_LONG_INTEGER:
                            m_file << next->u_.ul;
                            break;
                        case ELogType::UNSIGNED_LONG_LONG_INTEGER:
                            m_file << next->u_.ull;
                            break;
                        case ELogType::FLOAT:
                            m_file << next->u_.f;
                            break;
                        case ELogType::DOUBLE:
                            m_file << next->u_.d;
                            break;
                    }
                    m_queue.UpdateReadIndex();
                }
                m_file.flush();

                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(10ms);
            }
        }

        explicit CLogger(const std::string &fileName)
            : m_fileName(fileName), m_queue(LOG_QUEUE_SIZE)
        {
            m_file.open(fileName);
            ASSERT(m_file.is_open(), "Could not open log file:" + fileName);

            m_pLoggerThread = CreateAndStartThread(-1, "Common/CLogger " + m_fileName, [this]()
            { 
                FlushQueue();
            });

            ASSERT(m_pLoggerThread != nullptr, "Failed to start CLogger thread.");
        }

        ~CLogger()
        {
            std::string time_str;
            std::cerr << Common::GetCurrentTimeStr(&time_str) << " Flushing and closing CLogger for " << m_fileName << std::endl;

            while (m_queue.size())
            {
                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(1s);
            }
            m_isRrunning = false;
            m_pLoggerThread->join();

            m_file.close();
            std::cerr << Common::GetCurrentTimeStr(&time_str) << " CLogger for " << m_fileName << " exiting." << std::endl;
        }

        /// Overloaded methods to write different log entry types to the lock free queue.
        /// Creates a SLogElement of the correct type and writes it to the lock free queue.
        auto PushValue(const SLogElement &logElement) noexcept
        {
            *(m_queue.GetNextToWriteTo()) = logElement;
            m_queue.UpdateWriteIndex();
        }

        auto PushValue(const char value) noexcept
        {
            PushValue(SLogElement{ELogType::CHAR, {.c = value}});
        }

        auto PushValue(const int value) noexcept
        {
            PushValue(SLogElement{ELogType::INTEGER, {.i = value}});
        }

        auto PushValue(const long value) noexcept
        {
            PushValue(SLogElement{ELogType::LONG_INTEGER, {.l = value}});
        }

        auto PushValue(const long long value) noexcept
        {
            PushValue(SLogElement{ELogType::LONG_LONG_INTEGER, {.ll = value}});
        }

        auto PushValue(const unsigned value) noexcept
        {
            PushValue(SLogElement{ELogType::UNSIGNED_INTEGER, {.u = value}});
        }

        auto PushValue(const unsigned long value) noexcept
        {
            PushValue(SLogElement{ELogType::UNSIGNED_LONG_INTEGER, {.ul = value}});
        }

        auto PushValue(const unsigned long long value) noexcept
        {
            PushValue(SLogElement{ELogType::UNSIGNED_LONG_LONG_INTEGER, {.ull = value}});
        }

        auto PushValue(const float value) noexcept
        {
            PushValue(SLogElement{ELogType::FLOAT, {.f = value}});
        }

        auto PushValue(const double value) noexcept
        {
            PushValue(SLogElement{ELogType::DOUBLE, {.d = value}});
        }

        auto PushValue(const char* pValue) noexcept
        {
            while (*pValue)
            {
                PushValue(*pValue);
                ++pValue;
            }
        }

        auto PushValue(const std::string &value) noexcept
        {
            PushValue(value.c_str());
        }

        /// Parse the format string, substitute % with the variable number of arguments passed and write the string to the lock free queue.
        template <typename T, typename... A>
        auto Log(const char* szBuffer, const T &value, A... args) noexcept
        {
            while (*szBuffer)
            {
                if (*szBuffer == '%')
                {
                    if (UNLIKELY(*(szBuffer + 1) == '%'))
                    { // to allow %% -> % escape character.
                        ++szBuffer;
                    }
                    else
                    {
                        PushValue(value);    // substitute % with the value specified in the arguments.
                        Log(szBuffer + 1, args...); // pop an argument and call self recursively.
                        return;
                    }
                }
                PushValue(*szBuffer++);
            }
            FATAL("extra arguments provided to log()");
        }

        /// Overload for case where no substitution in the string is necessary.
        /// Note that this is overloading not specialization. gcc does not allow inline specializations.
        auto Log(const char *szBuffer) noexcept
        {
            while (*szBuffer)
            {
                if (*szBuffer == '%')
                {
                    if (UNLIKELY(*(szBuffer + 1) == '%'))
                    { // to allow %% -> % escape character.
                        ++szBuffer;
                    }
                    else
                    {
                        FATAL("missing arguments to log()");
                    }
                }
                PushValue(*szBuffer++);
            }
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        CLogger() = delete;

        CLogger(const CLogger &) = delete;

        CLogger(const CLogger &&) = delete;

        CLogger &operator=(const CLogger &) = delete;

        CLogger &operator=(const CLogger &&) = delete;

    private:
        /// File to which the log entries will be written.
        const std::string m_fileName;
        std::ofstream     m_file;

        /// Lock free queue of log elements from main logging thread to background formatting and disk writer thread.
        CLockFreeQueue<SLogElement> m_queue;
        std::atomic<bool>           m_isRrunning = {true};

        /// Background logging thread.
        std::thread* m_pLoggerThread = nullptr;
    };
}
