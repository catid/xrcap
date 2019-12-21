// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core.hpp"
#include "core_serializer.hpp"

#if !defined(_WIN32)
    #include <pthread.h>
    #include <unistd.h>
#endif // _WIN32

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif __MACH__
    #include <sys/file.h>
    #include <mach/mach_time.h>
    #include <mach/mach.h>
    #include <mach/clock.h>

    extern mach_port_t clock_port;
#else
    #include <time.h>
    #include <sys/time.h>
    #include <sys/file.h> // flock
#endif

#include <algorithm>

namespace core {


//------------------------------------------------------------------------------
// Timing

#ifdef _WIN32
// Precomputed frequency inverse
static double PerfFrequencyInverseUsec = 0.;
static double PerfFrequencyInverseMsec = 0.;

static void InitPerfFrequencyInverse()
{
    LARGE_INTEGER freq = {};
    if (!::QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) {
        return;
    }
    const double invFreq = 1. / (double)freq.QuadPart;
    PerfFrequencyInverseUsec = 1000000. * invFreq;
    PerfFrequencyInverseMsec = 1000. * invFreq;
    CORE_DEBUG_ASSERT(PerfFrequencyInverseUsec > 0.);
    CORE_DEBUG_ASSERT(PerfFrequencyInverseMsec > 0.);
}
#elif __MACH__
static bool m_clock_serv_init = false;
static clock_serv_t m_clock_serv = 0;

static void InitClockServ()
{
    m_clock_serv_init = true;
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &m_clock_serv);
}
#endif // _WIN32

uint64_t GetTimeUsec()
{
#ifdef _WIN32
    LARGE_INTEGER timeStamp = {};
    if (!::QueryPerformanceCounter(&timeStamp)) {
        return 0;
    }
    if (PerfFrequencyInverseUsec == 0.) {
        InitPerfFrequencyInverse();
    }
    return (uint64_t)(PerfFrequencyInverseUsec * timeStamp.QuadPart);
#elif __MACH__
    if (!m_clock_serv_init) {
        InitClockServ();
    }

    mach_timespec_t tv;
    clock_get_time(m_clock_serv, &tv);

    return 1000000 * tv.tv_sec + tv.tv_nsec / 1000;
#else
    // This seems to be the best clock to used based on:
    // http://btorpey.github.io/blog/2014/02/18/clock-sources-in-linux/
    // The CLOCK_MONOTONIC_RAW seems to take a long time to query,
    // and so it only seems useful for timing code that runs a small number of times.
    // The CLOCK_MONOTONIC is affected by NTP at 500ppm but doesn't make sudden jumps.
    // Applications should already be robust to clock skew so this is acceptable.
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_nsec / 1000) + static_cast<uint64_t>(ts.tv_sec) * 1000000;
#endif
}

uint64_t GetTimeMsec()
{
#ifdef _WIN32
    LARGE_INTEGER timeStamp = {};
    if (!::QueryPerformanceCounter(&timeStamp)) {
        return 0;
    }
    if (PerfFrequencyInverseMsec == 0.) {
        InitPerfFrequencyInverse();
    }
    return (uint64_t)(PerfFrequencyInverseMsec * timeStamp.QuadPart);
#else
    // TBD: Optimize this?
    return GetTimeUsec() / 1000;
#endif
}


//------------------------------------------------------------------------------
// TimeoutTimer

void TimeoutTimer::SetTimeout(uint64_t timeout_msec)
{
    TimeoutMsec = timeout_msec;
}

void TimeoutTimer::Reset()
{
    const uint64_t now_msec = GetTimeMsec();

    LastTickMsec = now_msec;
    TimeoutCount = 0;
}

bool TimeoutTimer::Timeout()
{
    // If already timed out:
    if (TimeoutCount >= 4) {
        return true;
    }

    const uint64_t now_msec = GetTimeMsec();

    // If timeout expired:
    if (now_msec - LastTickMsec > TimeoutMsec / 4) {
        // Increment timeout count
        ++TimeoutCount;
        if (TimeoutCount >= 4) {
            return true;
        }

        // Reset wait at the current time and wait to timeout again
        LastTickMsec = now_msec;
    }

    return false;
}


//------------------------------------------------------------------------------
// Process Tools

bool IsAlreadyRunning(const std::string& name)
{
#ifdef _WIN32
    std::string mutex_name = "Local\\";
    mutex_name += name;

    ::CreateMutexA(0, FALSE, mutex_name.c_str());
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    std::string filename = name + ".instlock";
    FILE* file = fopen(filename.c_str(), "w");
    if (file) {
        const int lock_result = flock(fileno(file), LOCK_EX | LOCK_NB);
        if (lock_result == 0) {
            return false;
        }
        if (errno == EWOULDBLOCK) {
            return true;
        }
    }
    CORE_DEBUG_BREAK(); // Should never happen
    return false;
#endif
}


//------------------------------------------------------------------------------
// Thread Tools

#ifdef _WIN32
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;       // Must be 0x1000.
    LPCSTR szName;      // Pointer to name (in user addr space).
    DWORD dwThreadID;   // Thread ID (-1=caller thread).
    DWORD dwFlags;      // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
void SetCurrentThreadName(const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = ::GetCurrentThreadId();
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#pragma warning(pop)
}
#elif __MACH__
void SetCurrentThreadName(const char* threadName)
{
    pthread_setname_np(threadName);
}
#else
void SetCurrentThreadName(const char* threadName)
{
    pthread_setname_np(pthread_self(), threadName);
}
#endif


//------------------------------------------------------------------------------
// SIMD-Safe Aligned Memory Allocations

static const unsigned kAlignmentBytes = 32;

static inline uint8_t* SIMDSafeAllocate(size_t size)
{
    uint8_t* data = (uint8_t*)calloc(1, kAlignmentBytes + size);
    if (!data) {
        return nullptr;
    }
    unsigned offset = (unsigned)((uintptr_t)data % kAlignmentBytes);
    data += kAlignmentBytes - offset;
    data[-1] = (uint8_t)offset;
    return data;
}

static inline void SIMDSafeFree(void* ptr)
{
    if (!ptr) {
        return;
    }
    uint8_t* data = (uint8_t*)ptr;
    unsigned offset = data[-1];
    if (offset >= kAlignmentBytes) {
        CORE_DEBUG_BREAK(); // Should never happen
        return;
    }
    data -= kAlignmentBytes - offset;
    free(data);
}


//------------------------------------------------------------------------------
// WorkerQueue

void WorkerQueue::Initialize(unsigned max_queue_size)
{
    MaxQueueSize = max_queue_size;

    Terminated = false;
    Thread = MakeSharedNoThrow<std::thread>(&WorkerQueue::Loop, this);
}

void WorkerQueue::Shutdown()
{
    Terminated = true;

    // Make sure that queue notification happens after termination flag is set
    {
        std::unique_lock<std::mutex> locker(QueueLock);
        QueueCondition.notify_all();
    }

    JoinThread(Thread);

    QueuePublic.clear();
    QueuePrivate.clear();
}

bool WorkerQueue::SubmitWork(WorkerCallback callback)
{
    std::unique_lock locker(QueueLock);

    if (QueuePublic.size() >= MaxQueueSize) {
        return false;
    }

    QueuePublic.push_back(callback);
    QueueCondition.notify_all();
    return true;
}

void WorkerQueue::Loop()
{
    SetCurrentThreadName("WorkerQueue");

    while (!Terminated)
    {
        {
            std::unique_lock<std::mutex> locker(QueueLock);

            if (QueuePublic.empty() && !Terminated) {
                QueueCondition.wait_for(locker, std::chrono::milliseconds(100));
            }

            if (QueuePublic.empty()) {
                continue;
            }

            std::swap(QueuePublic, QueuePrivate);
        }

        for (auto& callback : QueuePrivate) {
            callback();
        }
        QueuePrivate.clear();
    }
}


//------------------------------------------------------------------------------
// BackgroundWorker

void BackgroundWorker::Initialize()
{
    Terminated = false;
    Completed = false;
    Thread = MakeSharedNoThrow<std::thread>(&BackgroundWorker::Loop, this);
}

void BackgroundWorker::Shutdown()
{
    Terminated = true;
    {
        std::unique_lock<std::mutex> locker(StartLock);
        StartCondition.notify_all();
    }
    {
        std::unique_lock<std::mutex> locker(EndLock);
        EndCondition.notify_all();
    }

    JoinThread(Thread);
}

void BackgroundWorker::Fork(WorkerCallback callback)
{
    std::unique_lock locker(StartLock);

    if (Callback) {
        CORE_DEBUG_BREAK(); // Invalid state
        return;
    }

    Completed = false;
    Callback = callback;
    StartCondition.notify_all();
}

void BackgroundWorker::Join()
{
    while (!Terminated && !Completed)
    {
        std::unique_lock<std::mutex> locker(EndLock);
        if (Terminated || Completed) {
            break;
        }
        EndCondition.wait_for(locker, std::chrono::milliseconds(100));
    }
}

void BackgroundWorker::Loop()
{
    SetCurrentThreadName("BackgroundWorker");

    while (!Terminated)
    {
        WorkerCallback local_callback;
        {
            std::unique_lock<std::mutex> locker(StartLock);
            if (!Terminated) {
                StartCondition.wait_for(locker, std::chrono::milliseconds(100));
            }
            local_callback = Callback;
            Callback = WorkerCallback();
        }

        if (!local_callback) {
            continue;
        }
        local_callback();

        Completed = true;
        {
            std::unique_lock<std::mutex> locker(EndLock);
            EndCondition.notify_all();
        }
    }
}


//------------------------------------------------------------------------------
// Percentile

// Note: This partially sorts and modifies the provided data
template<typename T>
static inline T GetPercentile(std::vector<T>& data, float percentile)
{
    if (data.empty()) {
        const T empty{};
        return empty;
    }
    if (data.size() == 1) {
        return data[0];
    }

    using offset_t = typename std::vector<T>::size_type;

    const size_t count = data.size();
    const offset_t goalOffset = (offset_t)(count * percentile);

    std::nth_element(data.begin(), data.begin() + goalOffset, data.end());

    return data[goalOffset];
}


//------------------------------------------------------------------------------
// UnixTimeConverter

void UnixTimeConverter::Update()
{
    const uint64_t now_usec = GetTimeUsec();

    if ((now_usec - LastUpdateUsec) < kUpdateIntervalUsec) {
        return;
    }
    LastUpdateUsec = now_usec;

    const wallclock_t now = std::chrono::system_clock::now();
    const wallclock_t system_boot_time = now - std::chrono::microseconds(now_usec);

    // Write history
    History[HistoryWriteIndex] = system_boot_time;
    HistoryWriteIndex++;
    if (HistoryCount < HistoryWriteIndex) {
        HistoryCount = HistoryWriteIndex;
    }
    if (HistoryWriteIndex >= kHistoryCount) {
        HistoryWriteIndex = 0;
    }

    const wallclock_t last_time = BootUnixTime;

    MedianWork.resize(HistoryCount);
    for (int i = 0; i < HistoryCount; ++i) {
        MedianWork[i] = History[i].time_since_epoch().count();
    }
    const int64_t median_count = GetPercentile(MedianWork, 0.5f);

    BootUnixTime = History[0];
    for (int i = 0; i < HistoryCount; ++i) {
        if (History[i].time_since_epoch().count() == median_count) {
            BootUnixTime = History[i];
            break;
        }
    }

    if (HistoryCount > 1) {
        int64_t delta_usec = (int64_t)(last_time.time_since_epoch().count() - BootUnixTime.time_since_epoch().count());
        if (delta_usec < 0) {
            delta_usec = -delta_usec;
        }
    }
}

uint64_t UnixTimeConverter::Convert(uint64_t boot_usec)
{
    // Update time offset between boot and local time.
    Update();

    wallclock_t point = BootUnixTime + std::chrono::microseconds(boot_usec);
    return std::chrono::duration_cast<std::chrono::microseconds>(point.time_since_epoch()).count();
}


} // namespace core
