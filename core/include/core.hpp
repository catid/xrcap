// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <list>
#include <memory>


//------------------------------------------------------------------------------
// Boolean Constants

#define CORE_TRUE  1
#define CORE_FALSE 0


//------------------------------------------------------------------------------
// Application Return Values

#define CORE_APP_SUCCESS 0
#define CORE_APP_FAILURE -1


//------------------------------------------------------------------------------
// Tool Macros

#define CORE_ARRAY_COUNT(array) \
    static_cast<int>(sizeof(array) / sizeof(array[0]))


//------------------------------------------------------------------------------
// Portability Macros

// Specify an intentionally unused variable (often a function parameter)
#define CORE_UNUSED(x) (void)(x);
#define CORE_UNUSED2(x, y) (void)(x); (void)(y);
#define CORE_UNUSED3(x, y, z) (void)(x); (void)(y); (void)(z);

// Compiler-specific debug break
#if defined(_DEBUG) || defined(DEBUG) || defined(CORE_DEBUG_IN_RELEASE)
    #define CORE_DEBUG
    #if defined(_WIN32)
        #define CORE_DEBUG_BREAK() __debugbreak()
    #else // _WIN32
        #define CORE_DEBUG_BREAK() __builtin_trap()
    #endif // _WIN32
    #define CORE_DEBUG_ASSERT(cond) { if (!(cond)) { CORE_DEBUG_BREAK(); } }
    #define CORE_IF_DEBUG(x) x;
#else // _DEBUG
    #define CORE_DEBUG_BREAK() ;
    #define CORE_DEBUG_ASSERT(cond) ;
    #define CORE_IF_DEBUG(x) ;
#endif // _DEBUG

// Compiler-specific force inline keyword
#if defined(_MSC_VER)
    #define CORE_INLINE inline __forceinline
#else // _MSC_VER
    #define CORE_INLINE inline __attribute__((always_inline))
#endif // _MSC_VER

// Compiler-specific align keyword
#if defined(_MSC_VER)
    #define CORE_ALIGNED(x) __declspec(align(x))
#else // _MSC_VER
    #define CORE_ALIGNED(x) __attribute__ ((aligned(x)))
#endif // _MSC_VER

// Architecture check
#if defined(ANDROID) || defined(IOS) || defined(LINUX_ARM)
    // Use aligned accesses on ARM
    #define CORE_ALIGNED_ACCESSES
#endif // ANDROID


#include <cstdint>      // uint8_t etc types
#include <memory>       // std::unique_ptr, std::shared_ptr
#include <new>          // std::nothrow
#include <mutex>        // std::mutex
#include <vector>       // std::vector
#include <functional>   // std::function
#include <thread>       // std::thread
#include <chrono>       // std::chrono

namespace core {

//------------------------------------------------------------------------------
// C Platform Consistency Checks

static_assert(CORE_TRUE == (int)true, "CORE_TRUE is not true");
static_assert(CORE_FALSE == (int)false, "CORE_FALSE is not false");


//------------------------------------------------------------------------------
// C++ Convenience Classes

/// Derive from NoCopy to disallow copies of the derived class
struct NoCopy
{
    CORE_INLINE NoCopy() {}
    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;
};

/// Calls the provided (lambda) function at the end of the current scope
class ScopedFunction
{
public:
    ScopedFunction(std::function<void()> func) {
        Func = func;
    }
    ~ScopedFunction() {
        if (Func) {
            Func();
        }
    }
    void Cancel() {
        Func = std::function<void()>();
    }
    std::function<void()> Func;
};

/// Join a std::shared_ptr<std::thread>
inline void JoinThread(std::shared_ptr<std::thread>& th)
{
    if (th) {
        try {
            if (th->joinable()) {
                th->join();
            }
        } catch (std::system_error& /*err*/) {}
        th = nullptr;
    }
}


//------------------------------------------------------------------------------
// High-resolution timers

/// Get time in microseconds
uint64_t GetTimeUsec();

/// Get time in milliseconds
uint64_t GetTimeMsec();


//------------------------------------------------------------------------------
// TimeoutTimer

class TimeoutTimer
{
public:
    void SetTimeout(uint64_t timeout_msec);
    void Reset();
    bool Timeout();

protected:
    uint64_t TimeoutMsec = 0;
    uint64_t LastTickMsec = 0;
    int TimeoutCount = 0;
};


//------------------------------------------------------------------------------
// Process Tools

/// Returns true if the given process name is already running.
/// This is useful to avoid running the same application twice.
bool IsAlreadyRunning(const std::string& name);


//------------------------------------------------------------------------------
// Thread Tools

/// Set the current thread name
void SetCurrentThreadName(const char* name);


//------------------------------------------------------------------------------
// Shared Pointers

template<typename T, typename... Args>
inline std::unique_ptr<T> MakeUniqueNoThrow(Args&&... args) {
    return std::unique_ptr<T>(new(std::nothrow) T(std::forward<Args>(args)...));
}

template<typename T, typename... Args>
inline std::shared_ptr<T> MakeSharedNoThrow(Args&&... args) {
    return std::shared_ptr<T>(new(std::nothrow) T(std::forward<Args>(args)...));
}


//------------------------------------------------------------------------------
// WorkerQueue

using WorkerCallback = std::function<void()>;

// Queue up a maximum number of work items
class WorkerQueue
{
public:
    void Initialize(unsigned max_queue_size);
    void Shutdown();

    // Returns false if queue overflowed
    bool SubmitWork(WorkerCallback callback);

    bool IsTerminated() const
    {
        return Terminated;
    }

protected:
    unsigned MaxQueueSize = 2;

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;

    mutable std::mutex QueueLock;
    std::condition_variable QueueCondition;
    std::list<WorkerCallback> QueuePublic;
    std::list<WorkerCallback> QueuePrivate;

    void Loop();
};


//------------------------------------------------------------------------------
// BackgroundWorker

// Runs a single task off the main thread
class BackgroundWorker
{
public:
    ~BackgroundWorker()
    {
        Shutdown();
    }
    void Initialize();
    void Shutdown();

    void Fork(WorkerCallback callback);
    void Join();

    bool IsTerminated() const {
        return Terminated;
    }

protected:
    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;

    mutable std::mutex StartLock, EndLock;
    std::condition_variable StartCondition, EndCondition;
    WorkerCallback Callback;
    std::atomic<bool> Completed = ATOMIC_VAR_INIT(false);

    void Loop();
};


//------------------------------------------------------------------------------
// UnixTimeConverter

// Convert from microseconds since boot to microseconds since Unix epoch.
class UnixTimeConverter
{
public:
    uint64_t Convert(uint64_t boot_usec);

protected:
    using wallclock_t = std::chrono::time_point<std::chrono::system_clock>;

    static const int kHistoryCount = 10;
    static const uint64_t kUpdateIntervalUsec = 1000 * 1000;

    uint64_t LastUpdateUsec = 0;

    wallclock_t BootUnixTime{};

    wallclock_t History[kHistoryCount];
    int HistoryCount = 0;
    int HistoryWriteIndex = 0;

    std::vector<int64_t> MedianWork;

    void Update();
};


} // namespace core


namespace std {


//------------------------------------------------------------------------------
// Android Portability

#if defined(ANDROID) && !defined(DEFINED_TO_STRING)

#define DEFINED_TO_STRING
#include <string>
#include <sstream>

/// Polyfill for to_string() on Android
template<typename T> string to_string(T value)
{
    ostringstream os;
    os << value;
    return os.str();
}
#endif // DEFINED_TO_STRING


} // namespace std
