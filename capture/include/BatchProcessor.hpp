// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

/*
    BatchProcessor module

    This processes a batch of images from all the cameras, decoding JPEGs and
    culling the depth map.  If the application is rendering the images,
    we also produce renderable meshes from the depth maps.

    FIXME: Eliminate overlapping geometry that belongs to other cameras.
    FIXME: At this stage we could cull unneeded imagery.

    It then compresses the imagery and depth map for transport, passing the
    completed batch to the callback.
*/

#include "RgbdImage.hpp"
#include "RuntimeConfiguration.hpp"
#include <core_video.hpp>
#include "TimeConverter.hpp"

#include <DepthCalibration.hpp> // depth_mesh
#include <DepthMesh.hpp> // depth_mesh
#include <MfxVideoDecoder.hpp> // mfx
#include <MfxVideoEncoder.hpp> // mfx
#include <zdepth_lossless.hpp> // zdepth
#include <zdepth_lossy.hpp> // zdepth

#include <memory>
#include <atomic>
#include <thread>

namespace core {


//------------------------------------------------------------------------------
// Constants

// Interval between keyframes in milliseconds
static const int kKeyframeIntervalMsec = 1000;

// Time to hold slow warnings for
static const unsigned kWarningHoldMsec = 1000;

// Depth of any of the pipeline queues
static const int kPipelineQueueDepth = 8;

enum class ProcessorState
{
    Idle,
    Encoding,
    SlowWarning,
    Error
};


//------------------------------------------------------------------------------
// PipelineStatistics

class PipelineStatistics
{
public:
    void AddSample(std::shared_ptr<ImageBatch> batch);

protected:
    uint64_t LastReportMsec = 0;

    unsigned MinDelayMsec = 0;
    unsigned MaxDelayMsec = 0;
    uint64_t DelayAvgSumMsec = 0;
    unsigned DelayAvgCount = 0;

    unsigned ErrorCount = 0;
    unsigned SlowDropCount = 0;

    static const uint64_t kReportIntervalMsec = 2000;

    void ResetStats();
    void LogReport();
};


//------------------------------------------------------------------------------
// PipelineData

struct PipelineData
{
    // Batch for images
    std::shared_ptr<ImageBatch> Batch;

    RuntimeConfiguration* Config = nullptr;

    // Configuration for the pipeline
    protos::CompressionSettings Compression;
    bool ImagesNeeded = false;
    bool VideoNeeded = false;

    // Callback invoked on completion of entire batch
    BatchCallback Callback;

    // Number of pipelines that must retire for callback to be invoked
    std::atomic<int> ActivePipelineCount;

    void OnPipelineComplete()
    {
        const int count = --ActivePipelineCount;
        if (count == 0) {
            Callback(Batch);
        }
    }
};


//------------------------------------------------------------------------------
// BatchPipelineElement

/*
    Processing pipeline:

    (1) Mesh vertices
    (2) Mesh triangles
    (3) Mesh compression
    (4) JPEG decompression
    (5) Texture culling
    (6) Denoise and video encode

    Failures in the pipeline are flagged via the ImageBatch object, causing all
    other workers to abort remaining processing.  The object is returned early to
    the batch processor out of order, which allows us to update state and stats.
*/

class BatchPipelineElement
{
public:
    void Initialize(
        std::shared_ptr<BatchPipelineElement> next_element,
        std::string element_name,
        int camera_index);
    ~BatchPipelineElement()
    {
        Shutdown();
    }
    void Shutdown();

    // Returns false if the queue overflowed
    void Process(std::shared_ptr<PipelineData> data);

protected:
    std::shared_ptr<BatchPipelineElement> NextElement;
    std::string ElementName;
    int CameraIndex = -1;

    core::WorkerQueue Worker;

    virtual bool Run(std::shared_ptr<PipelineData> data) = 0;
};


//------------------------------------------------------------------------------
// Element State

struct VideoEncoderElement : public BatchPipelineElement
{
    ~VideoEncoderElement()
    {
        Shutdown();
    }

    mfx::EncoderParams EncoderParams{};

    std::unique_ptr<mfx::VideoEncoder> Encoder;
    std::unique_ptr<VideoParser> Parser;
    std::vector<uint8_t> VideoParameters;

    unsigned JpegWidth = 0, JpegHeight = 0;
    std::unique_ptr<mfx::VideoDecoder> JpegDecoder;

    // Allocator used when input is in raw NV12 format
    // or when we need a copy-back buffer for JPEG
    std::shared_ptr<mfx::SystemAllocator> RawAllocator;

    bool Run(std::shared_ptr<PipelineData> data) override;
};

struct MeshCompressorElement : public BatchPipelineElement
{
    ~MeshCompressorElement()
    {
        Shutdown();
    }

    TemporalDepthFilter TemporalFilter;
    DepthEdgeFilter EdgeFilter;

    std::unique_ptr<lossless::DepthCompressor> LosslessDepth;
    std::unique_ptr<lossy::DepthCompressor> LossyDepth;

    uint32_t ExtrinsicsEpoch = 0;
    uint32_t ClipEpoch = 0;

    bool EnableCrop = false;
    ImageCropRegion CropRegion;

    bool Run(std::shared_ptr<PipelineData> data) override;
};


//------------------------------------------------------------------------------
// PipelineCamera

// Processing pipeline for one camera in the batch
struct PipelineCamera
{
    void Initialize(int index);
    void Shutdown();

    inline void Process(std::shared_ptr<PipelineData> data)
    {
        MeshCompressor->Process(data);
    }

    int CameraIndex = -1;

    std::shared_ptr<VideoEncoderElement> VideoEncoder;
    std::shared_ptr<MeshCompressorElement> MeshCompressor;
};


//------------------------------------------------------------------------------
// BatchProcessor

class BatchProcessor
{
public:
    ~BatchProcessor()
    {
        Shutdown();
    }
    void Initialize(RuntimeConfiguration* config, BatchCallback callback);
    void Shutdown();

    void OnBatch(std::shared_ptr<ImageBatch> batch);

    ProcessorState GetState() const
    {
        return State;
    }

protected:
    RuntimeConfiguration* RuntimeConfig = nullptr;
    BatchCallback Callback;

    core::WorkerQueue Worker;

    std::atomic<ProcessorState> State = ATOMIC_VAR_INIT(ProcessorState::Idle);

    uint64_t LastKeyframeMsec = 0;
    int NextBatchNumber = 0;

    // Converted from boot time to Unix Epoch in microseconds
    UnixTimeConverter Epoch;

    // Timestamp cleaner for the epoch timestamps
    VideoTimestampCleaner TimeCleaner;

    static const int kMaxCameras = 8;
    PipelineCamera Cameras[kMaxCameras];

    // VideoInfo epoch starting from 1 to differentiate from 0 default values
    uint32_t VideoInfoEpoch = 1;
    protos::MessageVideoInfo VideoInfo{};

    PipelineStatistics Statistics;

    // Lock held while processing output from the pipeline, which can be received from multiple threads.
    // Aborted batches can be received out of order.
    // Completed batches are always received in order.
    std::mutex BatchHandlerLock;

    uint64_t LastWarningMsec = 0;


    void ProcessBatch(std::shared_ptr<ImageBatch> batch);
};


} // namespace core
