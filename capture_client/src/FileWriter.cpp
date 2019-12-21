// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "FileWriter.hpp"

#include <iomanip>

namespace core {


//------------------------------------------------------------------------------
// Tools

void SetIntrinsics(ChunkIntrinsics& dest, const CameraIntrinsics& src)
{
    dest.Width = src.Width;
    dest.Height = src.Height;
    dest.LensModel = src.LensModel;
    dest.cx = src.cx;
    dest.cy = src.cy;
    dest.fx = src.fx;
    dest.fy = src.fy;
    for (int i = 0; i < 6; ++i) {
        dest.k[i] = src.k[i];
    }
    dest.codx = src.codx;
    dest.cody = src.cody;
    dest.p1 = src.p1;
    dest.p2 = src.p2;
}


//------------------------------------------------------------------------------
// FileWriter

bool FileWriter::Open(const char* file_path)
{
    FlushAndClose();

    File.open(file_path, std::ios::out|std::ios::binary);
    return File.good();
}

void FileWriter::WriteDecodedBatch(std::shared_ptr<DecodedBatch>& batch)
{
    if (!IsOpen()) {
        return;
    }

    int64_t interval_usec = batch->VideoBootUsec - LastVideoBootUsec;

    // If video timestamp is invalid:
    if (interval_usec <= 0 || interval_usec > 1000000) {
        interval_usec = 33333; // 30 FPS = 33.3 ms interval by default
    }

    const unsigned count = static_cast<unsigned>( batch->Frames.size() );
    WriteBatchInfo(count, VideoDurationUsec, batch->EpochUsec);

    ++VideoFrameCount;
    VideoDurationUsec += interval_usec;

    const bool force_write_info = (ParamsCounter == 0);
    if (++ParamsCounter >= kParamsInterval) {
        ParamsCounter = 0;
    }

    for (const auto& frame : batch->Frames) {
        const auto& info = frame->Info;
        const GuidCameraIndex camera_guid(info->Guid, info->FrameHeader.CameraIndex);

        if (info->VideoInfo)
        {
            bool writing = force_write_info;
            auto old_info = VideoInfo[camera_guid];
            if (!old_info) {
                VideoInfo[camera_guid] = info->VideoInfo;
                writing = true;
            } else if (old_info.get() != info->VideoInfo.get()) {
                VideoInfo[camera_guid] = info->VideoInfo;
                if (*old_info != *info->VideoInfo) {
                    writing = true;
                }
            } // TBD: We assume no changes if pointer has not changed
            if (writing) {
                WriteVideoInfo(camera_guid, *info->VideoInfo);
            }
        }

        if (info->Calibration)
        {
            bool writing = force_write_info;
            auto old_info = CalibrationInfo[camera_guid];
            if (!old_info) {
                CalibrationInfo[camera_guid] = info->Calibration;
                writing = true;
            } else if (old_info.get() != info->Calibration.get()) {
                CalibrationInfo[camera_guid] = info->Calibration;
                if (*old_info != *info->Calibration) {
                    writing = true;
                }
            } // TBD: We assume no changes if pointer has not changed
            if (writing) {
                WriteCalibration(camera_guid, *info->Calibration);
            }
        }

        if (info->Extrinsics)
        {
            bool writing = force_write_info;
            auto old_info = ExtrinsicsInfo[camera_guid];
            if (!old_info) {
                ExtrinsicsInfo[camera_guid] = info->Extrinsics;
                writing = true;
            } else if (old_info.get() != info->Extrinsics.get()) {
                ExtrinsicsInfo[camera_guid] = info->Extrinsics;
                if (*old_info != *info->Extrinsics) {
                    writing = true;
                }
            } // TBD: We assume no changes if pointer has not changed
            if (writing) {
                WriteExtrinsics(camera_guid, *info->Extrinsics);
            }
        }
    }

    for (unsigned i = 0; i < count; ++i)
    {
        const bool is_last_frame = (i == (count - 1));
        const auto& frame = batch->Frames[i];
        const auto& info = frame->Info;

        const GuidCameraIndex camera_guid(info->Guid, info->FrameHeader.CameraIndex);

        WriteFrame(
            camera_guid,
            is_last_frame,
            info->FrameHeader,
            info->StreamedImage.Data.data(),
            info->StreamedDepth.Data.data());
    }
}

void FileWriter::WriteCalibration(
    GuidCameraIndex camera_guid,
    const core::CameraCalibration& calibration)
{
    FileChunkHeader header;
    header.Length = static_cast<uint32_t>( sizeof(ChunkCalibration) );
    header.Type = FileChunk_Calibration;
    File.write((char*)&header, sizeof(header));

    ChunkCalibration output;
    output.CameraGuid = camera_guid;
    for (int i = 0; i < 3; ++i) {
        output.TranslationFromDepth[i] = calibration.TranslationFromDepth[i];
    }
    for (int i = 0; i < 9; ++i) {
        output.RotationFromDepth[i] = calibration.RotationFromDepth[i];
    }
    SetIntrinsics(output.Color, calibration.Color);
    SetIntrinsics(output.Depth, calibration.Depth);
    File.write((char*)&output, sizeof(output));
}

void FileWriter::WriteExtrinsics(
    GuidCameraIndex camera_guid,
    const protos::CameraExtrinsics& extrinsics)
{
    if (extrinsics.IsIdentity) {
        return;
    }

    FileChunkHeader header;
    header.Length = static_cast<uint32_t>( sizeof(ChunkExtrinsics) );
    header.Type = FileChunk_Extrinsics;
    File.write((char*)&header, sizeof(header));

    const float* transform = extrinsics.Transform.data();

    ChunkExtrinsics output;
    output.CameraGuid = camera_guid;
    for (int i = 0; i < 3; ++i) {
        output.Translation[i] = transform[i * 4 + 3];
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            output.Rotation[i * 3 + j] = transform[i * 4 + j];
        }
    }
    File.write((char*)&output, sizeof(output));
}

void FileWriter::WriteVideoInfo(
    GuidCameraIndex camera_guid,
    const protos::MessageVideoInfo& info)
{
    FileChunkHeader header;
    header.Length = static_cast<uint32_t>( sizeof(ChunkVideoInfo) );
    header.Type = FileChunk_VideoInfo;
    File.write((char*)&header, sizeof(header));

    ChunkVideoInfo output;
    output.CameraGuid = camera_guid;
    output.VideoType = info.VideoType;
    output.Width = info.Width;
    output.Height = info.Height;
    output.Framerate = info.Framerate;
    output.Bitrate = info.Bitrate;
    File.write((char*)&output, sizeof(output));
}

void FileWriter::WriteBatchInfo(
    unsigned max_camera_count,
    uint64_t video_usec,
    uint64_t video_epoch_usec)
{
    FileChunkHeader header;
    header.Length = static_cast<uint32_t>( sizeof(ChunkBatchInfo) );
    header.Type = FileChunk_BatchInfo;
    File.write((char*)&header, sizeof(header));

    ChunkBatchInfo output;
    output.MaxCameraCount = static_cast<uint32_t>( max_camera_count );
    output.VideoUsec = video_usec;
    output.VideoEpochUsec = video_epoch_usec;
    File.write((char*)&output, sizeof(output));
}

void FileWriter::WriteFrame(
    GuidCameraIndex camera_guid,
    bool is_final_frame,
    const protos::MessageFrameHeader& msg,
    const void* image,
    const void* depth)
{
    FileChunkHeader header;
    header.Length = static_cast<uint32_t>( sizeof(ChunkFrameHeader) + msg.ImageBytes + msg.DepthBytes );
    header.Type = FileChunk_Frame;
    File.write((char*)&header, sizeof(header));

    ChunkFrameHeader output;
    output.IsFinalFrame = is_final_frame ? 1 : 0;
    output.CameraGuid = camera_guid;
    output.FrameNumber = msg.FrameNumber;
    output.BackReference = msg.BackReference;
    output.ImageBytes = msg.ImageBytes;
    output.DepthBytes = msg.DepthBytes;
    for (int i = 0; i < 3; ++i) {
        output.Accelerometer[i] = msg.Accelerometer[i];
    }
    output.ExposureUsec = msg.ExposureUsec;
    output.AutoWhiteBalanceUsec = msg.AutoWhiteBalanceUsec;
    output.ISOSpeed = msg.ISOSpeed;
    output.Brightness = msg.Brightness;
    output.Saturation = msg.Saturation;
    File.write((char*)&output, sizeof(output));

    File.write((char*)image, msg.ImageBytes);
    File.write((char*)depth, msg.DepthBytes);
}

void FileWriter::FlushAndClose()
{
    File.flush();
    File.close();
}


} // namespace core
