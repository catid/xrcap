// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "BatchProcessor.hpp"

#include <core_logging.hpp>
#include <core_serializer.hpp>

namespace core {


//------------------------------------------------------------------------------
// Element State

bool VideoEncoderElement::Run(std::shared_ptr<PipelineData> data)
{
    if (!data->ImagesNeeded && !data->VideoNeeded) {
        return true;
    }

    auto& batch = data->Batch;
    auto& image = batch->Images[CameraIndex];

#if 0
    spdlog::info("Delta {} Exposures: {} {} AWB {} {}",
        (int64_t)(batch->Images[0]->SyncSystemUsec - batch->Images[1]->SyncSystemUsec),
        batch->Images[0]->ColorExposureUsec, batch->Images[1]->ColorExposureUsec,
        batch->Images[0]->ColorWhiteBalanceUsec, batch->Images[1]->ColorWhiteBalanceUsec);
#endif

    if (JpegWidth != batch->VideoInfo.Width || JpegHeight != batch->VideoInfo.Height)
    {
        JpegWidth = batch->VideoInfo.Width;
        JpegHeight = batch->VideoInfo.Height;

        if (JpegDecoder) {
            spdlog::info("Video format change: Resetting video pipeline.");
        }
        JpegDecoder.reset();
        Encoder.reset();
    }

    mfx::EncoderParams encoder_params;

    const auto& compression = data->Compression;
    encoder_params.FourCC = compression.ColorVideo == protos::VideoType_H264 ? MFX_CODEC_AVC : MFX_CODEC_HEVC;
    encoder_params.Bitrate = compression.ColorBitrate;
    encoder_params.Quality = compression.ColorQuality;
    encoder_params.Framerate = image->Framerate;
    encoder_params.Height = image->ColorHeight;
    encoder_params.Width = image->ColorWidth;
    encoder_params.IntraRefreshCycleSize = image->Framerate * kKeyframeIntervalMsec / (2 * 1000);
    encoder_params.IntraRefreshQPDelta = -5;

    // Work-around for Intel Media SDK issue:
    // It does not support using D3D textures for HEVC encoding,
    // so if we are currently using D3D we need to re-initialize the JPEG decoder too.
    if (compression.ColorVideo == protos::VideoType_H265 && JpegDecoder && JpegDecoder->Allocator->IsVideoMemory) {
        spdlog::warn("Resetting video pipeline for switch to HEVC for camera={}", CameraIndex);
        JpegDecoder.reset();
        Encoder.reset();
    }

    const auto lighting = data->Config->GetLighting(CameraIndex);
    auto& procamp = encoder_params.ProcAmp;
    procamp.Enabled = true; // Always enabled
    procamp.DenoisePercentage = compression.DenoisePercent;
    procamp.Brightness = lighting.Brightness;
    procamp.Saturation = lighting.Saturation;
    image->Brightness = procamp.Brightness;
    image->Saturation = procamp.Saturation;

    if (!EncoderParams.EncoderParamsEqual(encoder_params)) {
        spdlog::warn("Resetting video encoder for new camera={} settings", CameraIndex);
        JpegDecoder.reset();
        Encoder.reset();
    }
    EncoderParams = encoder_params;

    if (!RawAllocator) {
        RawAllocator = std::make_shared<mfx::SystemAllocator>();

        const bool success = RawAllocator->InitializeNV12SystemOnly(
            batch->VideoInfo.Width,
            batch->VideoInfo.Height,
            batch->VideoInfo.Framerate);

        if (!success) {
            spdlog::error("MFX allocator failed to initialize");
            RawAllocator.reset();
            return false;
        }
    }

    if (image->IsJpegBuffer)
    {
        if (!JpegDecoder)
        {
            const uint64_t t0 = GetTimeUsec();

            JpegDecoder = std::make_unique<mfx::VideoDecoder>();

            const bool use_video_memory = (compression.ColorVideo != protos::VideoType_H265);

            bool success = JpegDecoder->Initialize(
                use_video_memory,
                MFX_CODEC_JPEG,
                image->ColorImage.data(),
                static_cast<int>( image->ColorImage.size() ));
            if (!success) {
                spdlog::error("MFX JPEG decoder failed to initialize: Please make sure the iGPU is enabled on your PC!");
                return false;
            }

            const uint64_t t1 = GetTimeUsec();
            spdlog::info("MFX JPEG decoder initialized in {} msec", (t1 - t0) / 1000.f);
        }
    }

    image->IsNV12 = true;
    image->ChromaWidth = image->ColorWidth / 2;
    image->ChromaHeight = image->ColorHeight / 2;
    image->ChromaStride = image->ChromaWidth * 2;
    image->Color[0] = nullptr;
    image->Color[1] = nullptr;
    image->Color[2] = nullptr;

    // TBD: As far as I can tell, D3D9 mode for Intel QSV only supports allocating one GPU texture at a time,
    // so we need to get rid of our frame reference as fast as we can.
    mfx::frameref_t frame;

    if (image->IsJpegBuffer)
    {
        frame = JpegDecoder->Decode(
            image->ColorImage.data(),
            static_cast<int>( image->ColorImage.size() ));
        if (!frame) {
            spdlog::error("JPEG decode failed: Resetting video pipeline.");
            JpegDecoder.reset();
            Encoder.reset();
            return false;
        }

        if (data->ImagesNeeded) {
            image->CopyBack = JpegDecoder->Allocator->CopyToSystemMemory(frame);
            if (!image->CopyBack) {
                spdlog::warn("Cannot copy frame to system memory from D3D memory");
            } else {
                image->Color[0] = image->CopyBack->Raw->Surface.Data.Y;
                image->Color[1] = image->CopyBack->Raw->Surface.Data.UV;
            }
        }
    }
    else // Raw image:
    {
        frame.reset();

        uint8_t* src = image->ColorImage.data();
        const unsigned plane_bytes = image->ColorStride * image->ColorHeight;

        if (data->ImagesNeeded) {
            image->Color[0] = src;
            image->Color[1] = src + plane_bytes;
        }

        if (data->VideoNeeded) {
            frame = RawAllocator->Allocate();
            uint8_t* dest = frame->Raw->Data.data();
            memcpy(dest, src, plane_bytes * 3 / 2);
        }
    }

    if (!data->VideoNeeded) {
        return true;
    }

    // The encoder internally checks if the settings are unchanged
    if (Encoder && !Encoder->ChangeProcAmp(procamp)) {
        spdlog::warn("Resetting video pipeline on ProcAmp change failed for camera={}", CameraIndex);
        Encoder.reset();
        JpegDecoder.reset();
    }

    if (!Encoder)
    {
        const uint64_t t0 = GetTimeUsec();

        Encoder = std::make_unique<mfx::VideoEncoder>();

        bool success = Encoder->Initialize(
            image->IsJpegBuffer ? JpegDecoder->Allocator : RawAllocator,
            EncoderParams);
        if (!success) {
            spdlog::error("MFX encoder initialization failed");
            return false;
        }

        const uint64_t t1 = GetTimeUsec();
        spdlog::info("MFX video encoder initialized in {} msec", (t1 - t0) / 1000.f);
    }

    // Note that changing this setting causes the video decoder to show some weird rescaling artifacts,
    // so it cannot be adjusted every frame.  Instead we need to set it up once and maintain the same setting.
    if (image->EnableCrop)
    {
        auto& info = frame->Raw->Surface.Info;
        info.CropX = static_cast<mfxU16>( image->CropRegion.CropX );
        info.CropY = static_cast<mfxU16>( image->CropRegion.CropY );
        info.CropW = static_cast<mfxU16>( image->CropRegion.CropW );
        info.CropH = static_cast<mfxU16>( image->CropRegion.CropH );
    }

    const bool keyframe = batch->Keyframe;
    mfx::VideoEncoderOutput video;

    for (int retries = 0; retries < 3; ++retries)
    {
        video = Encoder->Encode(frame, keyframe);
        if (video.Bytes <= 0) {
            spdlog::warn("Encoder failed {}x: Retrying...", retries + 1);
        } else {
            break;
        }
    }

    if (video.Bytes <= 0) {
        spdlog::error("Encoder failed repeatedly: Resetting video pipeline.");
        Encoder.reset();
        JpegDecoder.reset();
        return false;
    }

    if (!Parser) {
        Parser = std::make_unique<VideoParser>();
    }
    Parser->Reset();
    Parser->ParseVideo(
        compression.ColorVideo == protos::VideoType_H265,
        video.Data,
        video.Bytes);

    if (Parser->Pictures.size() != 1) {
        spdlog::error("Found {} frames in encoder output", Parser->Pictures.size());
        return false;
    }

    if (Parser->TotalParameterBytes > 0)
    {
        VideoParameters.resize(Parser->TotalParameterBytes);
        uint8_t* dest = VideoParameters.data();
        for (auto& nalu : Parser->Parameters) {
            memcpy(dest, nalu.Ptr, nalu.Bytes);
            dest += nalu.Bytes;
        }
    }

    auto& picture = Parser->Pictures[0];
    int compressed_bytes = picture.TotalBytes;
    if (keyframe) {
        compressed_bytes += static_cast<int>( VideoParameters.size() );
    }

    image->CompressedImage.resize(compressed_bytes);
    uint8_t* dest = image->CompressedImage.data();

    if (keyframe) {
        if (VideoParameters.empty()) {
            spdlog::error("Video parameters not available for keyframe");
            return false;
        }
        memcpy(dest, VideoParameters.data(), VideoParameters.size());
        dest += VideoParameters.size();
    }

    for (auto& nalu : picture.Ranges) {
        memcpy(dest, nalu.Ptr, nalu.Bytes);
        dest += nalu.Bytes;
    }

    return true;
}

bool MeshCompressorElement::Run(std::shared_ptr<PipelineData> data)
{
    auto& batch = data->Batch;
    auto& image = batch->Images[CameraIndex];

    // Do not apply extrinsics so we can use this result for registration
    uint16_t* depth = image->DepthImage.data();

    ClipRegion clip_region;
    bool clip_needed = data->Config->ShouldClip(CameraIndex, clip_region);

    if (clip_needed)
    {
        // If crop settings updated:
        if (!EnableCrop ||
            ExtrinsicsEpoch != data->Config->ExtrinsicsEpoch ||
            ClipEpoch != data->Config->ClipEpoch)
        {
            ExtrinsicsEpoch = data->Config->ExtrinsicsEpoch;
            ClipEpoch = data->Config->ClipEpoch;

            image->Mesher->CalculateCrop(clip_region, CropRegion);

            spdlog::info("Updated camera {} crop: x={} y={} w={} h={}",
                CameraIndex, CropRegion.CropX, CropRegion.CropY, CropRegion.CropW, CropRegion.CropH);
        }
    }
    image->EnableCrop = EnableCrop = clip_needed;
    image->CropRegion = CropRegion;

    const bool is_calibration = (data->Config->Mode == CaptureMode::Calibration);

    // Enable temporal filter if user wants more stablization or we are calibrating.
    if (is_calibration || data->Compression.StabilizationFilter != 0) {
        TemporalFilter.Filter(depth, image->DepthWidth, image->DepthHeight);
    }

    // If user wants to improve seams between meshes:
    if (data->Compression.EdgeFilter != 0) {
        EdgeFilter.Filter(depth, image->DepthWidth, image->DepthHeight);
    }

    const bool face_painting_fix = data->Compression.FacePaintingFix != 0;

    // This modifies the mesh so it has to be done before the other parts.
    image->Mesher->GenerateCoordinates(
        depth,
        clip_needed ? &clip_region : nullptr,
        //nullptr, // We can get image crop region each frame but cannot apply it that often
        image->MeshVertices,
        face_painting_fix,
        !is_calibration); // Cull mesh if not calibrating

    // If images are needed we assume they are used to render a mesh,
    // so we should also generate the mesh.
    if (!data->ImagesNeeded && !data->VideoNeeded) {
        return true;
    }

    image->Mesher->GenerateTriangleIndices(depth, image->MeshTriangles);

    if (!data->VideoNeeded) {
        return true;
    }

    bool lossy_depth = data->Compression.DepthVideo != protos::VideoType_Lossless;
    if (lossy_depth && !is_calibration)
    {
        if (!LossyDepth) {
            LossyDepth = std::make_unique<lossy::DepthCompressor>();
        }

        bool is_hevc = data->Compression.DepthVideo == protos::VideoType_H265;
        LossyDepth->Compress(
            image->DepthWidth,
            image->DepthHeight,
            is_hevc,
            image->Framerate,
            depth,
            image->CompressedDepth,
            batch->Keyframe);
    }
    else
    {
        if (!LosslessDepth) {
            LosslessDepth = std::make_unique<lossless::DepthCompressor>();
        }

        LosslessDepth->Compress(
            image->DepthWidth,
            image->DepthHeight,
            depth,
            image->CompressedDepth,
            batch->Keyframe);
    }

    if (image->CompressedDepth.empty()) {
        spdlog::error("LossyDepth->Compress failed");
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
// PipelineCamera

void PipelineCamera::Initialize(int index)
{
    CameraIndex = index;

    VideoEncoder = std::make_shared<VideoEncoderElement>();
    VideoEncoder->Initialize(nullptr, "Video Encoder", CameraIndex);

    MeshCompressor = std::make_shared<MeshCompressorElement>();
    MeshCompressor->Initialize(VideoEncoder, "Mesh Compressor", CameraIndex);
}

void PipelineCamera::Shutdown()
{
    MeshCompressor.reset();
    VideoEncoder.reset();
}


//------------------------------------------------------------------------------
// BatchPipelineElement

void BatchPipelineElement::Initialize(
    std::shared_ptr<BatchPipelineElement> next_element,
    std::string element_name,
    int camera_index)
{
    NextElement = next_element;
    ElementName = element_name;
    CameraIndex = camera_index;

    Worker.Initialize(kPipelineQueueDepth);
}

void BatchPipelineElement::Shutdown()
{
    Worker.Shutdown();
}

void BatchPipelineElement::Process(std::shared_ptr<PipelineData> data)
{
    // Called from the worker thread of the previous element:

    const bool pushed = Worker.SubmitWork([this, data]()
    {
        auto& batch = data->Batch;

        // If a parallel pipeline aborted this batch:
        if (batch->Aborted) {
            // Abort any remaining processing on the frame in sympathy
            data->OnPipelineComplete();
            return;
        }

        const bool success = Run(data);

        // If operation failed:
        if (!success) {
            spdlog::warn("Operation failed for stage {}: Dropped frame {} for camera {}",
                ElementName,
                batch->BatchNumber,
                CameraIndex);
            batch->PipelineError = true;
            batch->Aborted = true;
            data->OnPipelineComplete();
            return;
        }

        // If there is another element in the pipe:
        if (NextElement) {
            NextElement->Process(data);
        } else {
            // Complete with success!
            data->OnPipelineComplete();
        }
    });

    // If the queue overflowed to the next element:
    if (!pushed)
    {
        auto& batch = data->Batch;
        spdlog::warn("Computer too slow for stage {}: Dropped frame {} for camera {}",
            ElementName,
            batch->BatchNumber,
            CameraIndex);
        batch->SlowDrop = true;
        batch->Aborted = true;
        data->OnPipelineComplete();
    }
}


//------------------------------------------------------------------------------
// PipelineStatistics

void PipelineStatistics::AddSample(std::shared_ptr<ImageBatch> batch)
{
    const uint64_t start_msec = batch->BatchStartMsec;
    const uint64_t end_msec = batch->BatchEndMsec;

    if (batch->Aborted) {
        if (batch->PipelineError) {
            ErrorCount++;
        }
        if (batch->SlowDrop) {
            SlowDropCount++;
        }
        return;
    }

    const unsigned delay_msec = static_cast<unsigned>(end_msec - start_msec);

    if (DelayAvgCount == 0) {
        DelayAvgSumMsec = MinDelayMsec = MaxDelayMsec = delay_msec;
        DelayAvgCount = 1;
        return;
    }

    if (MinDelayMsec > delay_msec) {
        MinDelayMsec = delay_msec;
    }
    if (MaxDelayMsec < delay_msec) {
        MaxDelayMsec = delay_msec;
    }
    DelayAvgSumMsec += delay_msec;
    DelayAvgCount++;

    if (DelayAvgCount >= 10 && end_msec - LastReportMsec > kReportIntervalMsec)
    {
        LastReportMsec = end_msec;
        LogReport();
        ResetStats();
    }
}

void PipelineStatistics::ResetStats()
{
    DelayAvgCount = 0;
    ErrorCount = 0;
    SlowDropCount = 0;
}

void PipelineStatistics::LogReport()
{
    const unsigned avg_msec = static_cast<unsigned>( DelayAvgSumMsec / DelayAvgCount );

    spdlog::info("Video pipeline delay statistics: Min={} Avg={} Max={} (msec) Errors={} SlowDrops={}",
        MinDelayMsec,
        avg_msec,
        MaxDelayMsec,
        ErrorCount,
        SlowDropCount);
}


//------------------------------------------------------------------------------
// BatchProcessor

void BatchProcessor::Initialize(RuntimeConfiguration* config, BatchCallback callback)
{
    RuntimeConfig = config;
    Callback = callback;

    State = ProcessorState::Idle;

    LastKeyframeMsec = 0;

    Worker.Initialize(kPipelineQueueDepth);

    for (int i = 0; i < kMaxCameras; ++i) {
        Cameras[i].Initialize(i);
    }
}

void BatchProcessor::Shutdown()
{
    Worker.Shutdown();
    for (int i = 0; i < kMaxCameras; ++i) {
        Cameras[i].Shutdown();
    }
}

void BatchProcessor::OnBatch(std::shared_ptr<ImageBatch> batch)
{
    if (batch->Images.empty()) {
        spdlog::error("Empty image set");
        return;
    }

    const uint64_t now_msec = GetTimeMsec();
    batch->BatchStartMsec = now_msec;
    batch->BatchNumber = NextBatchNumber++;

    bool success = Worker.SubmitWork([this, batch]() {
        ProcessBatch(batch);
    });

    if (!success) {
        batch->SlowDrop = true;
        batch->Aborted = true;
        Statistics.AddSample(batch);
        spdlog::warn("Computer too slow to queue up new batch {}", batch->BatchNumber);
    }
}


//------------------------------------------------------------------------------
// BatchProcessor : Background Worker

void BatchProcessor::ProcessBatch(std::shared_ptr<ImageBatch> batch)
{
    if (batch->Images.empty()) {
        spdlog::error("Empty image set");
        return;
    }

    const uint64_t t0 = GetTimeUsec();

    batch->Keyframe = false;
    if (batch->BatchStartMsec - LastKeyframeMsec >= kKeyframeIntervalMsec ||
        RuntimeConfig->NeedsKeyframe.exchange(false))
    {
        LastKeyframeMsec = batch->BatchStartMsec;
        batch->Keyframe = true;
    }

    // Find the image with the earliest timestamp.
    // We do this because cameras connected through more USB hubs take about 3 msec
    // per hub longer to be received, so the earliest timestamp is the best one to
    // use for allowing the client to match times with other camera servers.

    RgbdImage* first_image = batch->Images[0].get();
    const unsigned camera_count = static_cast<unsigned>( batch->Images.size() );
    for (unsigned i = 1; i < camera_count; ++i)
    {
        RgbdImage* image = batch->Images[i].get();
        int64_t delta = image->SyncSystemUsec - first_image->SyncSystemUsec;
        if (delta > 0) {
            first_image = image;
        }
    }

    batch->SyncSystemUsec = first_image->SyncSystemUsec;

    // TBD: Currently these are unused - Perhaps remove them?
    batch->SyncEpochUsec = Epoch.Convert(batch->SyncSystemUsec);
    batch->VideoEpochUsec = TimeCleaner.Clean(
        first_image->DepthDeviceUsec,
        batch->SyncEpochUsec,
        batch->Discontinuity);
    if (batch->Discontinuity) {
        batch->Keyframe = true;
    }

    const auto compression = RuntimeConfig->GetCompression();

    std::shared_ptr<PipelineData> data = std::make_shared<PipelineData>();
    data->Batch = batch;
    data->ImagesNeeded = RuntimeConfig->ImagesNeeded.load();
    data->VideoNeeded = RuntimeConfig->VideoNeeded.load();
    data->Compression = compression;
    data->Config = RuntimeConfig;

    // Update video info we sent for each batch
    protos::MessageVideoInfo video_info;
    video_info.Bitrate = compression.ColorBitrate;
    video_info.Framerate = first_image->Framerate;
    video_info.Height = first_image->ColorHeight;
    video_info.Width = first_image->ColorWidth;
    video_info.VideoType = compression.ColorVideo;
    if (video_info != VideoInfo) {
        ++VideoInfoEpoch;
        VideoInfo = video_info;
    }
    data->Batch->VideoInfoEpoch = VideoInfoEpoch;
    data->Batch->VideoInfo = VideoInfo;

    auto& stream_info = batch->StreamInfo;
    stream_info.CameraCount = static_cast<uint32_t>( batch->Images.size() );
    stream_info.VideoBootUsec = batch->SyncSystemUsec;

    data->Callback = [this](std::shared_ptr<ImageBatch>& batch)
    {
        // Lock held while processing output from the pipeline, which can be received from multiple threads.
        // Aborted batches can be received out of order.
        // Completed batches are always received in order.
        std::lock_guard<std::mutex> locker(BatchHandlerLock);

        batch->BatchEndMsec = GetTimeMsec();
        Statistics.AddSample(batch);

        if (batch->Aborted)
        {
            if (batch->PipelineError) {
                State = ProcessorState::Error;
            } else if (batch->SlowDrop) {
                State = ProcessorState::SlowWarning;
            }
            LastWarningMsec = batch->BatchEndMsec;
            return;
        }

        // If we are encoding again (and warning is not active):
        if (LastWarningMsec == 0 || (int64_t)(GetTimeMsec() - LastWarningMsec) > kWarningHoldMsec) {
            LastWarningMsec = 0;
            State = ProcessorState::Encoding;
        }

        // Pass successful batches to callback
        Callback(batch);
    };

    // Kick off processing
    data->ActivePipelineCount = camera_count;
    for (unsigned camera_index = 0; camera_index < camera_count; ++camera_index) {
        Cameras[camera_index].Process(data);
    }
}


} // namespace core
