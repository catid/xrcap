// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Azure Kinect DK

    Each DK includes:

    + Microphones
    + IMU
    + Depth sensor
    + Color sensor
    + IR sensor
*/

#pragma once

#include <DepthMesh.hpp> // depth_mesh

#include "K4aTools.hpp"
#include "RgbdImage.hpp"
#include "TimeConverter.hpp"
#include "RuntimeConfiguration.hpp"

namespace core {


//------------------------------------------------------------------------------
// Constants

// Remember the last few frames from each camera for multicamera matching
static const int kCaptureHistoryCount = 8;

// Maximum match distance between frames in microseconds 
static const int64_t kMatchDistUsec = 20000;

enum class CameraStatus
{
    Idle,
    Initializing,
    StartFailed,
    Capturing,
    ReadFailed,
    SlowWarning,

    Count
};

const char* CameraStatusToString(CameraStatus status);
bool CameraStatusFailed(CameraStatus status);


//------------------------------------------------------------------------------
// K4aDevice

// Trade-offs for the different modes are described here:
// https://docs.microsoft.com/en-us/azure/Kinect-dk/hardware-specification
struct K4aDeviceSettings
{
    // Frames per second
    k4a_fps_t CameraFPS = k4a_fps_t::K4A_FRAMES_PER_SECOND_30;

    // This has the best overlap between depth image and color image
    k4a_color_resolution_t ColorResolution = K4A_COLOR_RESOLUTION_1536P;
    // K4A_COLOR_RESOLUTION_720P
    // K4A_COLOR_RESOLUTION_1536P

    // MJPG is required for this resolution
    k4a_image_format_t ImageFormat = K4A_IMAGE_FORMAT_COLOR_MJPG;
    // K4A_IMAGE_FORMAT_COLOR_MJPG
    // K4A_IMAGE_FORMAT_COLOR_NV12

    // Since we are meshing, we throw away a lot of the depth information anyway,
    // so binning seems worthwhile to decimate it further.
    k4a_depth_mode_t DepthMode = K4A_DEPTH_MODE_NFOV_2X2BINNED;
    // K4A_DEPTH_MODE_NFOV_2X2BINNED
    // K4A_DEPTH_MODE_NFOV_UNBINNED
};

struct K4aDeviceInfo
{
    uint32_t DeviceIndex = 0;
    std::string SerialNumber;
    k4a_hardware_version_t Version{};
    bool sync_in_jack_connected = false;
    bool sync_out_jack_connected = false;
    CameraCalibration Calibration{};
};

using ImageCallback = std::function<void(std::shared_ptr<RgbdImage>&)>;

class K4aDevice
{
public:
    K4aDevice(RuntimeConfiguration* config)
    {
        RuntimeConfig = config;
    }
    virtual inline ~K4aDevice()
    {
        Close();
    }

    CameraStatus GetStatus() const
    {
        return Status;
    }
    CameraCalibration GetCalibration() const
    {
        return Info.Calibration;
    }

    bool Open(
        const uint32_t index,
        const K4aDeviceSettings& settings,
        ImageCallback callback);
    bool StartImageCapture(
        k4a_wired_sync_mode_t sync_mode,
        int32_t depth_delay_off_color_usec);

    // Must be called after StartImageCapture()
    bool StartImuCapture();

    void Stop();
    void Close();

    // Get info about a control
    bool GetControlInfo(k4a_color_control_command_t command, ControlInfo& info);

    // Set color control
    bool SetControlAuto(k4a_color_control_command_t command);
    bool SetControlManual(k4a_color_control_command_t command, int32_t value);
    bool SetControlDefault(k4a_color_control_command_t command);

    const K4aDeviceInfo& GetInfo() const
    {
        return Info;
    }

    // Find capture for the given timestamp
    std::shared_ptr<RgbdImage> FindCapture(uint64_t SyncSystemUsec);

    bool DeviceFailed() const {
        return NeedsReset;
    }

protected:
    RuntimeConfiguration* RuntimeConfig = nullptr;
    K4aDeviceSettings Settings;
    uint32_t DeviceIndex = 0;
    ImageCallback Callback;

    std::atomic<CameraStatus> Status = ATOMIC_VAR_INIT(CameraStatus::Idle);

    std::atomic<bool> NeedsReset = ATOMIC_VAR_INIT(false);

    k4a_device_t Device = 0;
    K4aDeviceInfo Info;
    int NextFrameNumber = 0;

    // WhiteBalance:
    // The unit is degrees Kelvin. The setting must be set to a value evenly divisible by 10 degrees.
    // AutoExposurePriority: DEPRECATED DO NOT USE
    // BacklightCompenstation: 0=off, 1=on
    // PowerlineFrequency: 0=off,1=50Hz,2=60Hz
    ControlInfo Controls[K4A_CONTROL_COUNT];

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> ImuThread;
    std::shared_ptr<std::thread> CameraThread;

    // We assume that the camera is not moving and do not try to time sync
    // the IMU samples.  This should be improved for inside-out tracking
    // where the camera is moving.
    mutable std::mutex ImuLock;
    k4a_imu_sample_t LastImuSample{};

    // Capture history
    mutable std::mutex CaptureHistoryLock;
    std::shared_ptr<RgbdImage> CaptureHistory[kCaptureHistoryCount];
    std::atomic<int> WriteCaptureIndex = ATOMIC_VAR_INIT(0);

    uint64_t LastDepthDeviceUsec = 0;
    int ExpectedFramerate = 0;
    unsigned ExpectedFrameIntervalUsec = 0;
    int DepthDelayOffColorUsec = 0;

    // Mesher object for this device
    std::shared_ptr<DepthMesher> Mesher;

    DeviceClockSync ClockSync;

    uint32_t ExposureEpoch = 0;
    uint32_t ExtrinsicsEpoch = 0;


    void PrintControlInfo(ControlInfo& info);

    void ImuLoop();
    void CameraLoop();

    void OnCapture(int write_capture_index, k4a_capture_t capture);

    void PeriodicChecks();
    void UpdateExposure();
    void WriteExtrinsics();
};


} // namespace core
