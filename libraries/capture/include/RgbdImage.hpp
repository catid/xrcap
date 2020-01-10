// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Common code for RGBD capture

    This tries to not have any camera-specific includes leak into it,
    so that we can cleanly separate camera capture from processing.
*/

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <atomic>
#include <functional>

#include <core.hpp> // core
#include <Eigen/Core> // Eigen
#include <DepthCalibration.hpp> // depth_mesh
#include <DepthMesh.hpp> // depth_mesh
#include <MfxTools.hpp> // mfx
#include <CaptureProtocol.hpp> // capture_protocol

namespace core {


//------------------------------------------------------------------------------
// RgbdImage

struct RgbdImage
{
    //--------------------------------------------------------------------------
    // Set by CaptureDevice:
    //--------------------------------------------------------------------------

    // Image source
    int DeviceIndex = -1;
    int FrameNumber = 0; // Frame number for this camera
    int Framerate = 0; // FPS

    // Color image
    std::vector<uint8_t> ColorImage;
    int ColorWidth = 0, ColorHeight = 0, ColorStride = 0;

    // Does this contain a JPEG image?
    bool IsJpegBuffer = false;

    // Depth image
    std::vector<uint16_t> DepthImage;
    int DepthWidth = 0, DepthHeight = 0, DepthStride = 0;

    // Device timestamp in units specific to this device
    uint64_t DepthDeviceUsec = 0;

    // Host time when host finished receiving the depth
    uint64_t DepthSystemUsec = 0;

    // Device timestamp in units specific to this device
    uint64_t ColorDeviceUsec = 0;

    // Host time when host finished receiving the image
    uint64_t ColorSystemUsec = 0;

    // Temperature in Celsius
    float TemperatureC = 0.f;

    // Auto exposure duration for this color frame
    uint64_t ColorExposureUsec = 0;

    // Auto white balance for this color frame
    uint32_t ColorWhiteBalanceUsec = 0;

    // ISO speed for color frame
    uint32_t ColorIsoSpeed = 0;

    // Device-specific thread-safe mesher
    std::shared_ptr<DepthMesher> Mesher;

    // IMU sample for this frame
    Eigen::Vector3f AccelerationSample{};

    // Device time for sync pulse
    uint64_t SyncDeviceUsec = 0;

    // Host time for sync pulse
    uint64_t SyncSystemUsec = 0;

    //--------------------------------------------------------------------------
    // Set by CaptureManager:
    //--------------------------------------------------------------------------

    // Is this matched with a full multi-camera image set?
    std::atomic<bool> Matched = ATOMIC_VAR_INIT(false);

    //--------------------------------------------------------------------------
    // Set by BatchProcessor:
    //--------------------------------------------------------------------------

    // Batch number for all cameras
    int BatchNumber = 0;

    // This is the number of chroma components in width and height
    int ChromaWidth = 0, ChromaHeight = 0, ChromaStride = 0;

    // Some decoders produce NV12 output.
    // In this case Color[1] = interleaved U,V data, Color[2] = nullptr
    bool IsNV12 = false;

    // Decompressed YUV420 image data.
    uint8_t* Color[3];

    // Used for extrinsics calibration.
    // x,y,z,u,v coordinates of each vertex of the depth camera
    // in the frame of the color camera.
    // Not transformed to scene space, so we can use this for registration.
    std::vector<float> MeshVertices;

    // Color data copied back from GPU memory
    mfx::frameref_t CopyBack;

    // Indices for each triangle
    std::vector<uint32_t> MeshTriangles;

    // Factor applied by video processing step
    float Brightness = 0.f;
    float Saturation = 0.f;

    // Crop chosen for this video frame
    bool EnableCrop = false;
    ImageCropRegion CropRegion;

    // Compressed image and depth for streaming
    std::vector<uint8_t> CompressedImage;
    std::vector<uint8_t> CompressedDepth;
};


//------------------------------------------------------------------------------
// ImageBatch

// One image from each camera that goes through the processing pipeline
struct ImageBatch
{
    //--------------------------------------------------------------------------
    // Provided by CaptureManager:
    //--------------------------------------------------------------------------

    // Images in the batch
    std::vector< std::shared_ptr<RgbdImage> > Images;

    //--------------------------------------------------------------------------
    // Provided by BatchProcessor (Foreground):
    //--------------------------------------------------------------------------

    // Start time for processing the image set
    uint64_t BatchStartMsec = 0;

    // Incrementing number for this batch
    int BatchNumber = 0;

    //--------------------------------------------------------------------------
    // Provided by BatchProcessor (Background):
    //--------------------------------------------------------------------------

    // Time at which frame read occurred in system time since boot in usec
    //uint64_t UsbReadSystemUsec = 0;

    // End time for processing the image set
    uint64_t BatchEndMsec = 0;

    // Time at which sync pulse occurred in system time since boot in microseconds
    uint64_t SyncSystemUsec = 0;

    // Sync pulse time in microseconds since the Unix epoch
    uint64_t SyncEpochUsec = 0;

    // Video frame time since Unix epoch in microseconds
    uint64_t VideoEpochUsec = 0;

    // Is this frame following a discontinuity in the stream?
    bool Discontinuity = false;

    // Is this a keyframe?
    bool Keyframe = false;

    // Batch info for delivery
    protos::MessageBatchInfo StreamInfo{};

    // Number that increments each time the video info updates
    uint32_t VideoInfoEpoch = 0;

    // Video info for delivery
    protos::MessageVideoInfo VideoInfo{};

    // Error occurred during pipeline operations
    // e.g. JPEG decode failed or video encode failed
    std::atomic<bool> PipelineError = ATOMIC_VAR_INIT(false);

    // A stage in the pipeline failed to keep up with the input rate
    // and at least one image was dropped for this batch
    std::atomic<bool> SlowDrop = ATOMIC_VAR_INIT(false);

    // This frame was dropped due to some reason above
    std::atomic<bool> Aborted = ATOMIC_VAR_INIT(false);
};

using BatchCallback = std::function<void(std::shared_ptr<ImageBatch>& batch)>;


} // namespace core
