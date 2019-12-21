// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <core_mmap.hpp>

#include <CaptureProtocol.hpp>
#include <DepthCalibration.hpp>

#include "FileFormat.hpp"
#include "CaptureDecoder.hpp"
#include "DejitterQueue.hpp"

#include <thread>
#include <atomic>
#include <memory>
#include <unordered_map>

namespace core {


//------------------------------------------------------------------------------
// FileReader

class FileReader
{
public:
    ~FileReader()
    {
        Close();
    }

    // Returns false if file cannot be opened
    bool Open(
        std::shared_ptr<DejitterQueue> playback_queue,
        const char* file_path);
    void Close();

    void Pause(bool pause);
    void SetLoopRepeat(bool loop_repeat);

    void GetPlaybackState(XrcapPlayback& playback_state);

protected:
    mutable std::mutex Lock;

    MappedReadOnlySmallFile File;
    const uint8_t* FileData = nullptr;
    unsigned FileBytes = 0;
    unsigned FileOffset = 0;

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;

    std::atomic<bool> Paused = ATOMIC_VAR_INIT(false);
    std::atomic<bool> LoopRepeat = ATOMIC_VAR_INIT(false);

    std::shared_ptr<protos::MessageBatchInfo> BatchInfo;
    uint64_t VideoEpochUsec = 0;

    std::map<GuidCameraIndex, std::shared_ptr<protos::MessageVideoInfo>> VideoInfo;
    std::map<GuidCameraIndex, std::shared_ptr<core::CameraCalibration>> CalibrationInfo;
    std::map<GuidCameraIndex, std::shared_ptr<protos::CameraExtrinsics>> ExtrinsicsInfo;

    uint64_t LastInputVideoUsec = 0;
    uint64_t LastOutputVideoUsec = 0;
    uint32_t VideoFrameNumber = 0;

    std::shared_ptr<DejitterQueue> PlaybackQueue;

    // One decoder for each camera in the received batch
    std::vector<std::shared_ptr<DecoderPipeline>> Decoders;

    void Loop();
    void OnFrame(const std::shared_ptr<FrameInfo>& frame_info);
};


} // namespace core
