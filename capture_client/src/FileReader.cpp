// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "FileReader.hpp"

#include <FrameInfo.hpp>
#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// Tools

void IntrinsicsFromChunk(const ChunkIntrinsics& input, CameraIntrinsics& output)
{
    output.Width = input.Width;
    output.Height = input.Height;
    output.LensModel = input.LensModel;
    output.cx = input.cx;
    output.cy = input.cy;
    output.fx = input.fx;
    output.fy = input.fy;
    for (int i = 0; i < 6; ++i) {
        output.k[i] = input.k[i];
    }
    output.codx = input.codx;
    output.cody = input.cody;
    output.p1 = input.p1;
    output.p2 = input.p2;
}


//------------------------------------------------------------------------------
// FileReader

bool FileReader::Open(
    std::shared_ptr<DejitterQueue> playback_queue,
    const char* file_path)
{
    Close();

    std::lock_guard<std::mutex> locker(Lock);

    PlaybackQueue = playback_queue;

    if (!File.Read(file_path)) {
        return false;
    }

    FileData = File.GetData();
    FileBytes = File.GetDataBytes();
    FileOffset = 0;

    Terminated = false;
    Thread = std::make_shared<std::thread>(&FileReader::Loop, this);

    return true;
}

void FileReader::Close()
{
    Terminated = true;
    JoinThread(Thread);
    File.Close();

    spdlog::debug("Closed playback file");
}

void FileReader::Pause(bool pause)
{
    Paused = pause;
}

void FileReader::SetLoopRepeat(bool loop_repeat)
{
    LoopRepeat = loop_repeat;
}

void FileReader::Loop()
{
    while (!Terminated)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::lock_guard<std::mutex> locker(Lock);

        // Queue up one second worth of video
        if (!PlaybackQueue || PlaybackQueue->GetQueueDepth() > 30) {
            continue;
        }

        if (!Paused)
        {
            const uint8_t* file_data = FileData + FileOffset;

            if (FileBytes >= FileOffset + kFileChunkHeaderBytes) {
                const unsigned remaining = FileBytes - FileOffset;
                const FileChunkHeader* header = reinterpret_cast<const FileChunkHeader*>( file_data );
                if (kFileChunkHeaderBytes + header->Length <= remaining)
                {
                    if (header->Type == FileChunk_Calibration &&
                        header->Length == sizeof(ChunkCalibration))
                    {
                        const ChunkCalibration* calibration = reinterpret_cast<const ChunkCalibration*>( file_data + kFileChunkHeaderBytes );

                        std::shared_ptr<core::CameraCalibration> stored_calibration = std::make_shared<core::CameraCalibration>();
                        for (int i = 0; i < 9; ++i) {
                            stored_calibration->RotationFromDepth[i] = calibration->RotationFromDepth[i];
                        }
                        for (int i = 0; i < 3; ++i) {
                            stored_calibration->TranslationFromDepth[i] = calibration->TranslationFromDepth[i];
                        }
                        IntrinsicsFromChunk(calibration->Color, stored_calibration->Color);
                        IntrinsicsFromChunk(calibration->Depth, stored_calibration->Depth);
                        CalibrationInfo[calibration->CameraGuid] = stored_calibration;

                        spdlog::debug("Calibration for guid={}, camera={}", calibration->CameraGuid.ServerGuid, calibration->CameraGuid.CameraIndex);
                    }
                    else if (header->Type == FileChunk_Extrinsics &&
                        header->Length == sizeof(ChunkExtrinsics))
                    {
                        const ChunkExtrinsics* extrinsics = reinterpret_cast<const ChunkExtrinsics*>( file_data + kFileChunkHeaderBytes );

                        std::shared_ptr<protos::CameraExtrinsics> stored_extrinsics = std::make_shared<protos::CameraExtrinsics>();

                        //extrinsics->Translation
                        stored_extrinsics->IsIdentity = 0;
                        stored_extrinsics->Transform[0] = extrinsics->Rotation[0];
                        stored_extrinsics->Transform[1] = extrinsics->Rotation[1];
                        stored_extrinsics->Transform[2] = extrinsics->Rotation[2];
                        stored_extrinsics->Transform[3] = extrinsics->Translation[0];
                        stored_extrinsics->Transform[4] = extrinsics->Rotation[3];
                        stored_extrinsics->Transform[5] = extrinsics->Rotation[4];
                        stored_extrinsics->Transform[6] = extrinsics->Rotation[5];
                        stored_extrinsics->Transform[7] = extrinsics->Translation[1];
                        stored_extrinsics->Transform[8] = extrinsics->Rotation[6];
                        stored_extrinsics->Transform[9] = extrinsics->Rotation[7];
                        stored_extrinsics->Transform[10] = extrinsics->Rotation[8];
                        stored_extrinsics->Transform[11] = extrinsics->Translation[2];
                        stored_extrinsics->Transform[12] = 0.f;
                        stored_extrinsics->Transform[13] = 0.f;
                        stored_extrinsics->Transform[14] = 0.f;
                        stored_extrinsics->Transform[15] = 1.f;

                        ExtrinsicsInfo[extrinsics->CameraGuid] = stored_extrinsics;

                        spdlog::debug("Extrinsics for guid={}, camera={}", extrinsics->CameraGuid.ServerGuid, extrinsics->CameraGuid.CameraIndex);
                    }
                    else if (header->Type == FileChunk_VideoInfo &&
                        header->Length == sizeof(ChunkVideoInfo))
                    {
                        const ChunkVideoInfo* video_info = reinterpret_cast<const ChunkVideoInfo*>( file_data + kFileChunkHeaderBytes );

                        auto stored_info = std::make_shared<protos::MessageVideoInfo>();
                        stored_info->VideoType = static_cast<uint8_t>( video_info->VideoType );
                        stored_info->Width = video_info->Width;
                        stored_info->Height = video_info->Height;
                        stored_info->Bitrate = video_info->Bitrate;
                        stored_info->Framerate = video_info->Framerate;

                        VideoInfo[video_info->CameraGuid] = stored_info;

                        spdlog::debug("Video info: {}x{} @ {} FPS", stored_info->Width, stored_info->Height, stored_info->Framerate);
                    }
                    else if (header->Type == FileChunk_BatchInfo &&
                        header->Length == sizeof(ChunkBatchInfo))
                    {
                        const ChunkBatchInfo* batch_info = reinterpret_cast<const ChunkBatchInfo*>( file_data + kFileChunkHeaderBytes );

                        BatchInfo = std::make_shared<protos::MessageBatchInfo>();
                        BatchInfo->CameraCount = batch_info->MaxCameraCount;
                        BatchInfo->VideoBootUsec = batch_info->VideoUsec;
                        VideoEpochUsec = batch_info->VideoEpochUsec;

                        if (LastInputVideoUsec == 0) {
                            LastOutputVideoUsec = 0;
                            BatchInfo->VideoBootUsec = 0;
                        } else {
                            int64_t diff = BatchInfo->VideoBootUsec - LastInputVideoUsec;
                            LastOutputVideoUsec += diff;
                            BatchInfo->VideoBootUsec = LastOutputVideoUsec;
                        }
                        LastInputVideoUsec = batch_info->VideoUsec;

                        ++VideoFrameNumber;
                    }
                    else if (header->Type == FileChunk_Frame &&
                        header->Length > sizeof(ChunkFrameHeader))
                    {
                        const ChunkFrameHeader* frame_header = reinterpret_cast<const ChunkFrameHeader*>( file_data + kFileChunkHeaderBytes );

                        const GuidCameraIndex camera_guid = frame_header->CameraGuid;

                        std::shared_ptr<FrameInfo> frame_info = std::make_shared<FrameInfo>();
                        frame_info->BatchInfo = BatchInfo;
                        frame_info->VideoInfo = VideoInfo[camera_guid];
                        frame_info->Calibration = CalibrationInfo[camera_guid];
                        frame_info->Extrinsics = ExtrinsicsInfo[camera_guid];
                        if (!frame_info->VideoInfo || !frame_info->BatchInfo || !frame_info->Calibration) {
                            spdlog::warn("Dropping playback frame due to missing reference info");
                        }
                        else
                        {
                            frame_info->Guid = frame_header->CameraGuid.ServerGuid;
                            frame_info->FrameHeader.CameraIndex = frame_header->CameraGuid.CameraIndex;
                            frame_info->CaptureMode = protos::Mode_CaptureHighQual; // FIXME
                            for (int i = 0; i < 3; ++i) {
                                frame_info->FrameHeader.Accelerometer[i] = frame_header->Accelerometer[i];
                            }
                            frame_info->FrameHeader.AutoWhiteBalanceUsec = frame_header->AutoWhiteBalanceUsec;
                            frame_info->FrameHeader.Brightness = frame_header->Brightness;
                            frame_info->FrameHeader.DepthBytes = frame_header->DepthBytes;
                            frame_info->FrameHeader.ImageBytes = frame_header->ImageBytes;
                            frame_info->FrameHeader.ExposureUsec = frame_header->ExposureUsec;
                            frame_info->FrameHeader.IsFinalFrame = frame_header->IsFinalFrame;
                            frame_info->FrameHeader.ISOSpeed = frame_header->ISOSpeed;
                            frame_info->FrameHeader.Saturation = frame_header->Saturation;

                            frame_info->FrameHeader.FrameNumber = frame_header->FrameNumber;
                            frame_info->FrameHeader.BackReference = frame_header->BackReference;

                            const uint8_t* image_data = file_data + kFileChunkHeaderBytes + sizeof(ChunkFrameHeader);
                            frame_info->StreamedImage.Data.resize(frame_header->ImageBytes);
                            memcpy(frame_info->StreamedImage.Data.data(), image_data, frame_header->ImageBytes);
                            frame_info->StreamedImage.Complete = true;
                            frame_info->StreamedImage.ExpectedBytes = frame_header->ImageBytes;
                            frame_info->StreamedImage.ReceivedBytes = frame_header->ImageBytes;

                            const uint8_t* depth_data = image_data + frame_header->ImageBytes;
                            frame_info->StreamedDepth.Data.resize(frame_header->DepthBytes);
                            memcpy(frame_info->StreamedDepth.Data.data(), depth_data, frame_header->DepthBytes);
                            frame_info->StreamedDepth.Complete = true;
                            frame_info->StreamedDepth.ExpectedBytes = frame_header->DepthBytes;
                            frame_info->StreamedDepth.ReceivedBytes = frame_header->DepthBytes;

                            OnFrame(frame_info);
                        }
                    }
                }

                FileOffset += kFileChunkHeaderBytes + header->Length;
            }
            else
            {
                if (LoopRepeat) {
                    FileOffset = 0;
                } else {
                    FileOffset = FileBytes;
                }
            }
        }
    }
}

void FileReader::OnFrame(const std::shared_ptr<FrameInfo>& frame)
{
    const unsigned camera_count = frame->BatchInfo->CameraCount;
    if (Decoders.size() != camera_count)
    {
        Decoders.clear();
        Decoders.resize(camera_count);

        for (unsigned i = 0; i < camera_count; ++i) {
            Decoders[i] = std::make_shared<DecoderPipeline>();
        }
    }

    //spdlog::info("{} Test: camera={} frame={} boot={}", "Reader", frame->FrameHeader.CameraIndex, frame->BatchInfo->FrameNumber, frame->BatchInfo->VideoBootUsec);

    std::shared_ptr<DecodePipelineData> data = std::make_shared<DecodePipelineData>();
    data->Input = frame;
    data->Callback = [this](std::shared_ptr<DecodedFrame> decoded) {
        PlaybackQueue->Insert(decoded);
    };

    const unsigned camera_index = frame->FrameHeader.CameraIndex;
    Decoders[camera_index]->Process(data);
}

void FileReader::GetPlaybackState(XrcapPlayback& playback_state)
{
    std::lock_guard<std::mutex> locker(Lock);

    if (PlaybackQueue) {
        playback_state.DejitterQueueMsec = PlaybackQueue->GetQueueDepth() * 33; // FIXME
    } else {
        playback_state.DejitterQueueMsec = 0;
    }
    playback_state.VideoTimeUsec = LastOutputVideoUsec;
    playback_state.VideoFrame = VideoFrameNumber;

    // FIXME: XrcapPlaybackState_LiveStream

    playback_state.State = this->Paused ? XrcapPlaybackState_Paused : XrcapPlaybackState_Playing;
    playback_state.VideoFrameCount = VideoFrameNumber; // FIXME
    playback_state.VideoDurationUsec = LastOutputVideoUsec; // FIXME
}


} // namespace core
