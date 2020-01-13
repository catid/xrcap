// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "DejitterQueue.hpp"

#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// Tools

void DecodedBatch::Insert(std::shared_ptr<DecodedFrame> frame)
{
    auto& batch_info = frame->Info->BatchInfo;

    Frames.push_back(frame);
    VideoBootUsec = batch_info->VideoBootUsec;
    FrameNumber = frame->Info->FrameHeader.FrameNumber;
    EpochUsec = 0;
    QueueStartUsec = GetTimeUsec();
}

void FrameHistory::Insert(std::shared_ptr<DecodedFrame> frame)
{
    const uint64_t video_usec = frame->Info->BatchInfo->VideoBootUsec;

    for (auto it = BatchList.begin(); it != BatchList.end(); ++it)
    {
        std::shared_ptr<DecodedBatch>& historical = *it;
        const uint64_t hist_video_usec = historical->VideoBootUsec;

        if (hist_video_usec == video_usec) {
            historical->Frames.push_back(frame);
            return;
        }

        const int64_t delta = static_cast<int64_t>( video_usec - hist_video_usec );
        if (delta < 0)
        {
            std::shared_ptr<DecodedBatch> batch = std::make_shared<DecodedBatch>();
            batch->Insert(frame);
            BatchList.insert(it, batch);
            return;
        }
    }

    std::shared_ptr<DecodedBatch> batch = std::make_shared<DecodedBatch>();
    batch->Insert(frame);
    BatchList.push_back(batch);
}

void FrameHistory::EraseBefore(uint64_t now_usec, unsigned erase_point_usec)
{
    for (auto it = BatchList.begin(); it != BatchList.end(); ++it)
    {
        const DecodedBatch* historical = it->get();

        if (static_cast<uint32_t>(now_usec - historical->QueueStartUsec) < erase_point_usec)
        {
            BatchList.erase(BatchList.begin(), it);
            return;
        }
    }

    BatchList.clear();
}


//------------------------------------------------------------------------------
// DejitterQueue

void DejitterQueue::SetQueueDepth(uint32_t msec)
{
    DejitterQueueUsec = msec * 1000;
    spdlog::info("Dejitter queue depth: {} msec", msec);
}

void DejitterQueue::Initialize(FrameDisplayCallback callback)
{
    Callback = callback;

    SetQueueDepth(500); // default

    Terminated = false;
    Thread = std::make_shared<std::thread>(&DejitterQueue::Loop, this);
}

void DejitterQueue::Shutdown()
{
    Terminated = true;
    {
        std::unique_lock<std::mutex> locker(QueueLock);
        QueueCondition.notify_all();
    }

    JoinThread(Thread);
}

void DejitterQueue::Loop()
{
    SetCurrentThreadName("DisplayQueue");

    while (!Terminated)
    {
        int sleep_msec = 0;
        std::shared_ptr<DecodedBatch> batch = DequeueNext(&sleep_msec);
        if (batch) {
            Callback(batch);
        }

        if (sleep_msec > kDejitterWakeMaxMsec) {
            sleep_msec = kDejitterWakeMaxMsec;
        }
        if (sleep_msec < kDejitterWakeMinMsec) {
            sleep_msec = kDejitterWakeMinMsec;
        }

        std::unique_lock<std::mutex> locker(QueueLock);
        QueueCondition.wait_for(locker, std::chrono::milliseconds(sleep_msec));
    }
}

void DejitterQueue::Insert(std::shared_ptr<DecodedFrame>& frame)
{
    const uint64_t now_usec = GetTimeUsec();

    std::lock_guard<std::mutex> locker(Lock);

    const uint64_t video_usec = frame->Info->BatchInfo->VideoBootUsec;

    if (LastReleasedLocalUsec != 0)
    {
        const uint64_t no_data_time_usec = static_cast<uint64_t>( now_usec - LastReleasedLocalUsec );

        if (no_data_time_usec > DejitterQueueUsec * 2)
        {
            Histories.clear();
            LastReleasedLocalUsec = 0;
            LastReleasedVideoUsec = 0;
            SyncLocalUsec = 0;
            SyncVideoUsec = 0;
        }
        else if (LastReleasedVideoUsec != 0)
        {
            // Ignore frames that are too late
            const int32_t delta = static_cast<int32_t>( video_usec - LastReleasedVideoUsec );
            if (delta <= 0) {
                return;
            }
        }
    }

    const uint64_t guid = frame->Info->Guid;
    const unsigned camera_index = frame->Info->FrameHeader.CameraIndex;

    for (auto& history : Histories) {
        if (history->Guid == guid) {
            history->Insert(frame);
            return;
        }
    }

    std::shared_ptr<DecodedBatch> batch = std::make_shared<DecodedBatch>();
    batch->Insert(frame);

    std::shared_ptr<FrameHistory> history = std::make_shared<FrameHistory>();
    history->Guid = guid;
    history->BatchList.push_back(batch);

    Histories.push_back(history);
}

std::shared_ptr<DecodedBatch> DejitterQueue::DequeueNext(int* sleep_msec)
{
    *sleep_msec = kDejitterWakeMaxMsec;

    const uint64_t now_usec = GetTimeUsec();
    const uint32_t dejitter_queue_usec = DejitterQueueUsec;

    std::lock_guard<std::mutex> locker(Lock);

    std::shared_ptr<DecodedBatch> earliest;
    std::shared_ptr<FrameHistory> earliest_history;
    uint64_t earliest_video_usec = 0;

    size_t smallest_count = 0;

    for (auto& history : Histories)
    {
        if (history->BatchList.empty()) {
            continue;
        }
        auto& batch = history->BatchList.front();
        const uint64_t video_usec = batch->VideoBootUsec;
        if (!earliest || static_cast<int64_t>( video_usec - earliest_video_usec ) < 0) {
            earliest_video_usec = video_usec;
            earliest = batch;
            earliest_history = history;
        }
        if (smallest_count == 0) {
            smallest_count = history->BatchList.size();
        }
        else if (smallest_count > history->BatchList.size()) {
            smallest_count = history->BatchList.size();
        }
    }

    if (!earliest) {
        Reset();
        return nullptr;
    }
    if (LastReleasedLocalUsec != 0 && (uint64_t)(now_usec - LastReleasedLocalUsec) > dejitter_queue_usec * 2) {
        //spdlog::warn("DejitterQueue: Reset on long release time");
        Reset();
    }
    int queued_time_usec = (int)static_cast<int64_t>( now_usec - earliest->QueueStartUsec );
    if (queued_time_usec < 0) {
        Histories.clear();
        Reset();
        spdlog::warn("DejitterQueue: Clear because queue time went negative");
        return nullptr;
    }

    // Make sure each stream we are listening to has at least 2 frames queued or halt
    if (smallest_count <= 2) {
        //spdlog::warn("DejitterQueue: Halt wait for 2");
        return nullptr;
    }

    if (SyncVideoUsec != 0)
    {
        uint32_t erase_point_usec = dejitter_queue_usec * 3 / 2;

        if ((unsigned)queued_time_usec > erase_point_usec)
        {
            Reset();

            for (auto& history : Histories) {
                history->EraseBefore(now_usec, erase_point_usec);
            }
            spdlog::warn("DejitterQueue: Erased extra long queue backlog");
            return nullptr;
        }

        // If we are playing back too slow:
        float playback_speed = 1.0f;
        if ((unsigned)queued_time_usec > dejitter_queue_usec) {
            // Increase playback speed to keep the queue full.
            playback_speed = queued_time_usec / (float)dejitter_queue_usec;
        }
        // We do not decrease playback speed if the queue depth is reducing as this is almost
        // always caused by network latency spikes instead of clock skew.

        {
            const int32_t release_delay_usec = static_cast<int32_t>(now_usec - SyncLocalUsec);
            const int32_t video_delay_usec = static_cast<int32_t>(earliest_video_usec - SyncVideoUsec);
            const int32_t remaining_usec = video_delay_usec - static_cast<int32_t>(release_delay_usec * playback_speed);

            if (remaining_usec > kDejitterFuzzUsec) {
                *sleep_msec = static_cast<int>(remaining_usec / 1000);
                //spdlog::warn("DejitterQueue: Pacing delay");
                return nullptr;
            }
        }
    }
    else
    {
        // We have no started playing yet, so make sure we queue up to the target delay
        if ((unsigned)queued_time_usec < dejitter_queue_usec) {
            //spdlog::warn("DejitterQueue: Waiting to build up queue");
            return nullptr;
        }
    }

    const uint64_t rep_age_usec = now_usec - SyncLocalUsec;
    if (rep_age_usec > kSyncIntervalUsec) {
        SyncLocalUsec = now_usec;
        SyncVideoUsec = earliest_video_usec;
    }
    LastReleasedLocalUsec = now_usec;
    LastReleasedVideoUsec = earliest_video_usec;

    std::shared_ptr<DecodedBatch> output = std::make_shared<DecodedBatch>();
    output->VideoBootUsec = 0;

    const unsigned hist_count = static_cast<unsigned>( Histories.size() );
    for (unsigned i = 0; i < hist_count; ++i)
    {
        if (Histories[i]->BatchList.empty()) {
            continue;
        }
        auto& first = Histories[i]->BatchList.front();
        int64_t delta = static_cast<int64_t>( first->VideoBootUsec - earliest_video_usec );
        if (delta < 0) {
            delta = 0;
        }
        //spdlog::info("hist={} delta={} first={}", i, delta, first->VideoBootUsec);
        if (delta < 30000) {
            if (output->VideoBootUsec == 0) {
                output->VideoBootUsec = first->VideoBootUsec;
            }
            for (auto& frame : first->Frames) {
                output->Frames.push_back(frame);
            }
            Histories[i]->BatchList.pop_front();
        }
    }

    //spdlog::warn("Playing: frame={} time={}", output->FrameNumber, output->VideoBootUsec / 1000.f);

    return output;
}


} // namespace core
