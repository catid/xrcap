// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    CaptureManager

    This module manages the CaptureDevice sessions for each camera and matches
    frames together into a set from the same instant in time.

    It uses the codecs library to decode a batch of JPEGs in parallel for
    each set of images.

    If the JPEG images are needed for the capture GUI then these are copied to
    the CPU for rendering.

    X,Y,Z coordinates are recovered for the depth data.
    If capture mode is enabled, then a clip region is applied and the depth
    data is culled of elements not needed for render.
    Otherwise the depth data is not culled because it is needed for calibration.
*/

#pragma once

#include "RuntimeConfiguration.hpp"
#include "CaptureDevice.hpp"
#include "CaptureProtocol.hpp"
#include "BatchProcessor.hpp"

#include <MfxVideoDecoder.hpp> // mfx

#include <vector>
#include <memory>
#include <atomic>
#include <condition_variable>

namespace core {


//------------------------------------------------------------------------------
// Constants

// Maximum number of frames to queue up for decoding
static const int kDecodeQueueDepth = 3;

// Overall capture status
enum class CaptureStatus
{
    Idle,
    Initializing,
    Capturing,
    NoCameras,
    BadUsbConnection,
    FirmwareVersionMismatch,
    SyncCableMisconfigured,

    Count
};

const char* CaptureStatusToString(CaptureStatus status);
bool CaptureStatusFailed(CaptureStatus status);


//------------------------------------------------------------------------------
// Tools

unsigned GetAttachedK4CameraCount();


//------------------------------------------------------------------------------
// CaptureManager

class CaptureManager
{
public:
    void Initialize(RuntimeConfiguration* config, BatchCallback callback);
    void Shutdown();

    void SetMode(CaptureMode mode);

    void EnableTmdaMode(bool enabled);
    void SetTdmaSlots(const std::vector<int>& tdma_slots = std::vector<int>());
    unsigned GetTdmaSlotCount();

    CaptureStatus GetStatus() const
    {
        return Status;
    }
    unsigned GetDeviceCount() const
    {
        return DeviceCount;
    }
    std::vector<CameraStatus> GetCameraStatus() const;
    std::vector<CameraCalibration> GetCameraCalibration() const;

    RuntimeConfiguration* GetConfiguration()
    {
        return RuntimeConfig;
    }

protected:
    RuntimeConfiguration* RuntimeConfig = nullptr;
    BatchCallback Callback;

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;
    std::atomic<CaptureStatus> Status = ATOMIC_VAR_INIT(CaptureStatus::Idle);

    /// Connected devices
    std::atomic<uint32_t> DeviceCount = 0;
    std::vector< std::shared_ptr<K4aDevice> > Devices;

    /// Latest image batch received from all connected cameras
    mutable std::mutex BatchLock;
    std::shared_ptr<ImageBatch> LatestBatch;

    /// Lock protecting StartCondition
    mutable std::mutex StartLock;

    /// Condition that indicates the thread should wake up
    std::condition_variable StartCondition;

    /// Batch processor
    BatchProcessor Processor;

    std::atomic<bool> TdmaModeEnabled = ATOMIC_VAR_INIT(false);
    std::mutex TdmaLock;
    std::vector<int> TdmaSlots;


    void Loop();

    CaptureStatus BackgroundStart(CaptureMode mode);
    void BackgroundStop();

    // Returns true if a device has failed
    bool CheckDeviceFailure();

    void StopAll();
    void CloseAll();

    void OnImage(std::shared_ptr<RgbdImage>& image);

    bool ShouldClip(ClipRegion& clip) const;
};


} // namespace core
