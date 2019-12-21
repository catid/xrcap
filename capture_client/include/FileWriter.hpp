// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <fstream>

#include <CaptureProtocol.hpp>
#include <DepthCalibration.hpp>

#include "DejitterQueue.hpp" // DecodedBatch
#include "FileFormat.hpp"

namespace core {


//------------------------------------------------------------------------------
// Tools

void SetIntrinsics(ChunkIntrinsics& dest, const CameraIntrinsics& src);


//------------------------------------------------------------------------------
// FileWriter

class FileWriter
{
public:
    ~FileWriter()
    {
        FlushAndClose();
    }

    // Returns false if file cannot be opened
    bool Open(const char* file_path);
    bool IsOpen() const
    {
        return File.is_open();
    }
    uint64_t GetFileBytes()
    {
        return static_cast<uint64_t>( File.tellp() );
    }

    uint32_t GetFrameCount() const
    {
        return VideoFrameCount;
    }

    uint64_t GetDurationUsec() const
    {
        return VideoDurationUsec;
    }

    // This handles calling all the other functions below
    void WriteDecodedBatch(std::shared_ptr<DecodedBatch>& batch);

    void FlushAndClose();

protected:
    std::ofstream File;

    uint32_t VideoFrameCount = 0;
    uint64_t VideoDurationUsec = 0;

    // Last raw input timestamp
    uint64_t LastVideoBootUsec = 0;

    static const uint32_t kParamsInterval = 30; // Frames
    uint32_t ParamsCounter = 0;

    // Last info written
    std::map<GuidCameraIndex, std::shared_ptr<protos::MessageVideoInfo>> VideoInfo;
    std::map<GuidCameraIndex, std::shared_ptr<core::CameraCalibration>> CalibrationInfo;
    std::map<GuidCameraIndex, std::shared_ptr<protos::CameraExtrinsics>> ExtrinsicsInfo;

    void WriteCalibration(
        GuidCameraIndex camera_guid,
        const core::CameraCalibration& calibration);

    void WriteExtrinsics(
        GuidCameraIndex camera_guid,
        const protos::CameraExtrinsics& extrinsics); // Assumes no skew in matrix

    void WriteVideoInfo(
        GuidCameraIndex camera_guid,
        const protos::MessageVideoInfo& info);

    void WriteBatchInfo(
        unsigned max_camera_count,
        uint64_t video_usec,
        uint64_t video_epoch_usec);

    void WriteFrame(
        GuidCameraIndex camera_guid,
        bool is_final_frame,
        const protos::MessageFrameHeader& header,
        const void* image,
        const void* depth);
};


} // namespace core
