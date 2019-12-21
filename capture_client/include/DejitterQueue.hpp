// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    SteadyDisplayQueue

    Accepts frames in bursts or perhaps out of order.
    Releases frames in a steady framerate flow suitable for rendering,
    based on the relative timestamps in the video stream.
    If the video playback is too slow or fast, it adjusts playback speed
    to compensate.
*/

#pragma once

#include "CaptureDecoder.hpp"

#include <thread>
#include <memory>
#include <condition_variable>
#include <atomic>

namespace core {


//------------------------------------------------------------------------------
// Constants

// Release early if this close to the display time.
static const int kDejitterFuzzUsec = 1000;

// Wake time limits
static const int kDejitterWakeMinMsec = 2;
static const int kDejitterWakeMaxMsec = 10;

// Interval between re-syncing to the stream timestamps
static const uint64_t kSyncIntervalUsec = 500 * 1000;


//------------------------------------------------------------------------------
// Tools

struct DecodedBatch
{
    std::vector<std::shared_ptr<DecodedFrame>> Frames;

    uint32_t FrameNumber = 0;
    uint64_t VideoBootUsec = 0;
    uint64_t EpochUsec = 0;

    // Time when the first frame for this batch was enqueued.
    uint64_t QueueStartUsec = 0;

    void Insert(std::shared_ptr<DecodedFrame> frame);
};

struct FrameHistory
{
    uint64_t Guid = 0;

    std::list<std::shared_ptr<DecodedBatch>> BatchList;

    void Insert(std::shared_ptr<DecodedFrame> frame);
    void EraseBefore(uint64_t now_usec, unsigned erase_point_usec);
};


//------------------------------------------------------------------------------
// DejitterQueue

using FrameDisplayCallback = std::function<void(std::shared_ptr<DecodedBatch>&)>;

class DejitterQueue
{
public:
    ~DejitterQueue()
    {
        Shutdown();
    }
    void Initialize(FrameDisplayCallback callback);
    void Shutdown();

    void SetQueueDepth(uint32_t msec);
    int GetQueueDepth() const
    {
        std::lock_guard<std::mutex> locker(Lock);
        return (int)Histories.size();
    }

    void Insert(std::shared_ptr<DecodedFrame>& frame);

protected:
    FrameDisplayCallback Callback;

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;

    mutable std::mutex QueueLock;
    std::condition_variable QueueCondition;

    mutable std::mutex Lock;
    uint64_t LastReleasedLocalUsec = 0;
    uint64_t LastReleasedVideoUsec = 0;
    uint64_t SyncLocalUsec = 0;
    uint64_t SyncVideoUsec = 0;

    std::vector<std::shared_ptr<FrameHistory>> Histories;

    // This is how much latency to add in order to avoid stalls due to network lag.
    std::atomic<uint32_t> DejitterQueueUsec = ATOMIC_VAR_INIT(500 * 1000);


    std::shared_ptr<DecodedBatch> DequeueNext(int* sleep_msec);

    void Reset()
    {
        SyncLocalUsec = 0;
        SyncVideoUsec = 0;
    }

    void Loop();
};


} // namespace core
