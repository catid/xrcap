// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <vector>
#include <memory>

#include "capture_client.h" // C API

#include <CaptureProtocol.hpp>
#include <DepthCalibration.hpp>

namespace core {


//------------------------------------------------------------------------------
// StreamedBuffer

struct StreamedBuffer
{
    int ExpectedBytes = 0;
    int ReceivedBytes = 0;
    std::vector<uint8_t> Data;
    bool Complete = false;

    // Reset to a new size
    void Reset(int bytes);

    // Returns true once the buffer is complete
    bool Accumulate(const uint8_t* data, int bytes);
};


//------------------------------------------------------------------------------
// FrameInfo

// This is a single frame from a single camera
struct FrameInfo
{
    std::shared_ptr<protos::MessageVideoInfo> VideoInfo;
    std::shared_ptr<protos::MessageBatchInfo> BatchInfo;
    std::shared_ptr<core::CameraCalibration> Calibration;
    std::shared_ptr<protos::CameraExtrinsics> Extrinsics;

    uint64_t Guid = 0;
    protos::Modes CaptureMode;

    protos::MessageFrameHeader FrameHeader;

    StreamedBuffer StreamedImage;
    StreamedBuffer StreamedDepth;
};


} // namespace core
