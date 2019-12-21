// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureManager.hpp"

#include <future>
#include <core_string.hpp>

namespace core {


//------------------------------------------------------------------------------
// Constants

const char* CaptureStatusToString(CaptureStatus status)
{
    switch (status)
    {
    case CaptureStatus::Idle: return "Idle";
    case CaptureStatus::Initializing: return "Initializing";
    case CaptureStatus::Capturing: return "Capturing";
    case CaptureStatus::NoCameras: return "No Cameras";
    case CaptureStatus::BadUsbConnection: return "Bad USB Connection";
    case CaptureStatus::FirmwareVersionMismatch: return "Firmware Version Mismatch";
    case CaptureStatus::SyncCableMisconfigured: return "Sync Cable Misconfigured";
    default: break;
    }
    return "(Unknown)";
}

bool CaptureStatusFailed(CaptureStatus status)
{
    switch (status)
    {
    case CaptureStatus::Idle: return false;
    case CaptureStatus::Initializing: return false;
    case CaptureStatus::Capturing: return false;
    default: break;
    }
    return true;
}


//------------------------------------------------------------------------------
// Tools

unsigned GetAttachedK4CameraCount()
{
    return k4a_device_get_installed_count();
}


//------------------------------------------------------------------------------
// CaptureManager : API

void CaptureManager::Initialize(RuntimeConfiguration* config, BatchCallback callback)
{
    RuntimeConfig = config;
    Callback = callback;

    Status = CaptureStatus::Idle;

    Processor.Initialize(config, Callback);

    Terminated = false;
    Thread = std::make_shared<std::thread>(&CaptureManager::Loop, this);
}

void CaptureManager::Shutdown()
{
    Terminated = true;

    {
        std::unique_lock<std::mutex> qlocker(StartLock);
        StartCondition.notify_all();
    }

    spdlog::info("Capture manager thread stopping...");

    JoinThread(Thread);

    spdlog::info("Capture manager encoder stopping...");

    Processor.Shutdown();

    spdlog::info("...Capture manager shutdown complete");
}

void CaptureManager::SetMode(CaptureMode mode)
{
    RuntimeConfig->Mode = mode;

    std::unique_lock<std::mutex> qlocker(StartLock);
    StartCondition.notify_all();
}

void CaptureManager::EnableTmdaMode(bool enabled)
{
    TdmaModeEnabled = enabled;
}

void CaptureManager::SetTdmaSlots(const std::vector<int>& tdma_slots)
{
    std::unique_lock<std::mutex> locker(TdmaLock);
    TdmaSlots = tdma_slots;
}

unsigned CaptureManager::GetTdmaSlotCount()
{
    std::unique_lock<std::mutex> locker(TdmaLock);
    return static_cast<unsigned>( TdmaSlots.size() );
}

std::vector<CameraStatus> CaptureManager::GetCameraStatus() const
{
    std::vector<CameraStatus> status;
    for (const auto& device : Devices) {
        status.push_back(device->GetStatus());
    }
    return status;
}

std::vector<CameraCalibration> CaptureManager::GetCameraCalibration() const
{
    std::vector<CameraCalibration> calibration;
    for (const auto& device : Devices) {
        calibration.push_back(device->GetCalibration());
    }
    return calibration;
}

void CaptureManager::Loop()
{
    SetCurrentThreadName("StartLoop");

    Status = CaptureStatus::Idle;

    CaptureMode prev_mode = CaptureMode::Disabled;

    int delay_counter = 0;
    const int wake_interval_msec = 100;
    const int retry_delay_ticks = 5000 / wake_interval_msec;

    while (!Terminated)
    {
        {
            // unique_lock used since StartCondition.wait_for requires it
            std::unique_lock<std::mutex> locker(StartLock);

            if (!Terminated) {
                StartCondition.wait_for(locker, std::chrono::milliseconds(wake_interval_msec));
            }
        }
        if (Terminated) {
            break;
        }

        if (DeviceCount > 0)
        {
            const unsigned detected_camera_count = GetAttachedK4CameraCount();

            if (DeviceCount != detected_camera_count) {
                spdlog::warn("Detected camera count changed from {} -> {}: Stopping capture...", DeviceCount, detected_camera_count);
                BackgroundStop();
                prev_mode = CaptureMode::Disabled;
                continue;
            }

            if (TdmaModeEnabled)
            {
                bool should_stop = false;
                {
                    std::unique_lock<std::mutex> locker(TdmaLock);
                    if (detected_camera_count != TdmaSlots.size()) {
                        spdlog::warn("Registered TDMA slots {} no longer matches detected camera count {}: Stopping capture...", TdmaSlots.size(), detected_camera_count);
                        should_stop = true;
                    }
                }
                if (should_stop) {
                    BackgroundStop();
                    prev_mode = CaptureMode::Disabled;
                    continue;
                }
            }
        }

        const CaptureMode next_mode = RuntimeConfig->Mode;

        if (CheckDeviceFailure())
        {
            spdlog::warn("Device failure detected!  Stopping capture...");
            Status = CaptureStatus::BadUsbConnection;
            BackgroundStop();
            spdlog::warn("Device failure detected!  Restarting capture...");
            Status = BackgroundStart(next_mode);
            continue;
        }

        if (next_mode == prev_mode) {
            continue;
        }

        // If capture is currently enabled:
        if (prev_mode != CaptureMode::Disabled) {
            spdlog::debug("Background: Stopping capture");
            BackgroundStop();
            Status = CaptureStatus::Idle;
            spdlog::debug("Background: Capture stopped");
        }

        prev_mode = CaptureMode::Disabled;

        // If a retry delay is in progress:
        if (delay_counter > 0) {
            --delay_counter;
            continue;
        }

        if (next_mode != CaptureMode::Disabled) {
            spdlog::debug("Background: Starting capture");
            Status = CaptureStatus::Initializing;
            Status = BackgroundStart(next_mode);
            GetConfiguration()->CaptureConfigEpoch++;
            if (Status == CaptureStatus::Capturing) {
                spdlog::debug("Background: Capture started");
                prev_mode = next_mode;
            } else {
                spdlog::debug("Background: Capture start failed");
                delay_counter = retry_delay_ticks;
            }
        }
    }

    spdlog::debug("Background: Stopping capture on shutdown");
    BackgroundStop();

    spdlog::debug("CaptureManager background thread terminated");
}


//------------------------------------------------------------------------------
// CaptureManager : Background

struct {
    bool operator()(std::shared_ptr<K4aDevice> a, std::shared_ptr<K4aDevice> b) const {
        return core::StrCaseCompare(
            a->GetInfo().SerialNumber.c_str(),
            b->GetInfo().SerialNumber.c_str()) < 0;
    }
} DevicePtrSort;

CaptureStatus CaptureManager::BackgroundStart(CaptureMode mode)
{
    BackgroundStop();

    K4aDeviceSettings settings;

    if (mode == CaptureMode::CaptureLowQual) {
        // Normal video capture mode: Well matched depth-color full motion video
        settings.CameraFPS = K4A_FRAMES_PER_SECOND_30;
        settings.DepthMode = K4A_DEPTH_MODE_NFOV_2X2BINNED;
        settings.ColorResolution = K4A_COLOR_RESOLUTION_720P;
        settings.ImageFormat = K4A_IMAGE_FORMAT_COLOR_NV12;
    } else if (mode == CaptureMode::CaptureHighQual) {
        // Normal video capture mode: Well matched depth-color full motion video
        settings.CameraFPS = K4A_FRAMES_PER_SECOND_30;
        settings.DepthMode = K4A_DEPTH_MODE_NFOV_2X2BINNED;
        settings.ColorResolution = K4A_COLOR_RESOLUTION_1536P;
        settings.ImageFormat = K4A_IMAGE_FORMAT_COLOR_MJPG;
    } else if (mode == CaptureMode::Calibration) {
        // Extrinsics calibration capture mode: Wide angle, low rate
        settings.CameraFPS = K4A_FRAMES_PER_SECOND_5;
        settings.DepthMode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
        settings.ColorResolution = K4A_COLOR_RESOLUTION_1536P;
        settings.ImageFormat = K4A_IMAGE_FORMAT_COLOR_MJPG;
    }

    Devices.clear();

    const k4a_log_level_t min_log_level = k4a_log_level_t::K4A_LOG_LEVEL_WARNING;

    // If this is called before a device is opened, then the stdout will be
    // disabled for the k4a library.
    k4a_result_t result = k4a_set_debug_message_handler([](
        void *context,
        k4a_log_level_t level,
        const char *file,
        const int line,
        const char *message)
    {
        CORE_UNUSED(context);
        auto lvl = K4ALogLevelConvert(level);
        if (lvl > spdlog::level::level_enum::info) {
            spdlog::log(lvl, "k4a: [{}:{}] {}", file, line, message);
        } else {
            spdlog::log(lvl, "k4a: {}", message);
        }
    },
        nullptr,
        min_log_level);
    if (K4A_FAILED(result)) {
        spdlog::warn("Failed to hook Kinect log callback");
    }

    result = k4a_set_allocator(k4a_alloc, k4a_free);
    if (K4A_FAILED(result)) {
        spdlog::warn("Failed to hook Kinect allocator");
    }

    const uint32_t count = k4a_device_get_installed_count();
    if (count == 0) {
        spdlog::warn("No cameras detected");
        return CaptureStatus::NoCameras;
    }

    spdlog::info("Number of cameras = {}", count);
    Devices.reserve(count);

    const bool multi_server = TdmaModeEnabled;
    std::vector<int> tdma_slots;
    if (multi_server) {
        std::unique_lock<std::mutex> locker(TdmaLock);
        tdma_slots = TdmaSlots;
    } else {
        tdma_slots.resize(count);
        int32_t depth_delay_off_color = count / 2;
        for (unsigned camera_index = 0; camera_index < count; ++camera_index) {
            tdma_slots[camera_index] = depth_delay_off_color;
            depth_delay_off_color--;
        }
    }
    if (tdma_slots.size() != count) {
        spdlog::error("Mismatch between TDMA slot count {} and detected camera count {}", tdma_slots.size(), count);
        return CaptureStatus::Initializing;
    }

    const uint64_t t0 = GetTimeUsec();

    // We cannot open the cameras in parallel because it is not thread safe
    for (uint32_t camera_index = 0; camera_index < count; ++camera_index)
    {
        std::shared_ptr<K4aDevice> device = std::make_shared<K4aDevice>(RuntimeConfig);

        const bool success = device->Open(
            camera_index,
            settings,
            [this](std::shared_ptr<RgbdImage>& image)
        {
            OnImage(image);
        });
        if (!success) {
            spdlog::error("Failed to open camera {}: Make sure USB bandwidth is available", camera_index);
            return CaptureStatus::BadUsbConnection;
        }
        Devices.push_back(device);
    }

    // This breaks the capture code
    //std::sort(Devices.begin(), Devices.end(), DevicePtrSort);

    // If multi-camera mode:
    if (count > 1 || multi_server)
    {
        const auto version0 = Devices[0]->GetInfo().Version;
        int master_count = 0;

        // Verify all hardware versions match
        for (uint32_t i = 0; i < count; ++i)
        {
            const K4aDeviceInfo info = Devices[i]->GetInfo();

            if (i > 0 && version0 != info.Version)
            {
                spdlog::error("Multiple camera setup problem: Firmware version does not match on all cameras.  Please upgrade the firmware");
                return CaptureStatus::FirmwareVersionMismatch;
            }
            if (!info.sync_out_jack_connected && !info.sync_in_jack_connected)
            {
                spdlog::error("One of the cameras has no sync cables plugged in");
                return CaptureStatus::SyncCableMisconfigured;
            }
            if (info.sync_out_jack_connected && info.sync_in_jack_connected)
            {
                spdlog::error("One of the cameras has sync in and sync out both plugged in");
                return CaptureStatus::SyncCableMisconfigured;
            }
            if (info.sync_out_jack_connected)
            {
                ++master_count;
            }
        }
    
        if (!multi_server && master_count <= 0) {
            spdlog::error("No master detected: Multiple cameras but none of them have sync out plugged in properly");
            return CaptureStatus::SyncCableMisconfigured;
        }
        if (master_count > 1) {
            spdlog::error("Multiple cameras are master: Make sure the sync ports are set up right");
            return CaptureStatus::SyncCableMisconfigured;
        }
    }

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("Took {} msec to open cameras", (t1 - t0) / 1000.f);

#if 0 // FIXME: Still does not work
    // Start all the cameras in parallel
    std::vector<std::future<CaptureStatus>> start_futures;
    start_futures.reserve(count);
    for (int camera_index = 0; camera_index < count; ++camera_index)
    {
        auto& device = Devices[camera_index];

        auto fut = std::async([device, count, depth_delay_off_color_usec, this]() -> CaptureStatus {
            // Configure sync
            k4a_wired_sync_mode_t sync_mode = K4A_WIRED_SYNC_MODE_STANDALONE;
            if (count > 1) {
                if (device->GetInfo().sync_out_jack_connected) {
                    sync_mode = K4A_WIRED_SYNC_MODE_MASTER;
                } else {
                    sync_mode = K4A_WIRED_SYNC_MODE_SUBORDINATE;
                }
            }

            if (!device->StartImageCapture(sync_mode, depth_delay_off_color_usec)) {
                spdlog::error("Camera failed to start capturing");
                return CaptureStatus::BadUsbConnection;
            }
            return CaptureStatus::Capturing;
        });
        start_futures.push_back(std::move(fut));

        depth_delay_off_color_usec += kDepthOffsetUsec;
    }
    CaptureStatus start_result = CaptureStatus::Capturing;
    std::for_each(start_futures.begin(), start_futures.end(), [&](std::future<CaptureStatus>& fut) {
        fut.wait();
        const CaptureStatus fut_result = fut.get();
        if (fut_result != CaptureStatus::Capturing) {
            start_result = fut_result;
        }
    });
    if (start_result != CaptureStatus::Capturing) {
        return start_result;
    }
#else
    // Camera starting in parallel is not thread-safe so do it serially
    for (uint32_t camera_index = 0; camera_index < count; ++camera_index)
    {
        auto& device = Devices[camera_index];

        // Configure sync
        k4a_wired_sync_mode_t sync_mode = K4A_WIRED_SYNC_MODE_STANDALONE;
        if (count > 1 || multi_server) {
            if (device->GetInfo().sync_out_jack_connected) {
                sync_mode = K4A_WIRED_SYNC_MODE_MASTER;
            } else {
                sync_mode = K4A_WIRED_SYNC_MODE_SUBORDINATE;
            }
        }

        const int32_t depth_delay_off_color_usec = tdma_slots[camera_index] * kDepthOffsetUsec;
        if (!device->StartImageCapture(sync_mode, depth_delay_off_color_usec)) {
            spdlog::error("Camera failed to start capturing");
            return CaptureStatus::BadUsbConnection;
        }
    }
#endif

    if (mode == CaptureMode::Calibration)
    {
        // Also start IMU capture to aid in extrinsics calibration
        for (auto& device : Devices) {
            device->StartImuCapture();
        }
    }

    const uint64_t t2 = GetTimeUsec();
    spdlog::info("Took {} msec to start cameras", (t2 - t1) / 1000.f);

    DeviceCount = count;
    return CaptureStatus::Capturing;
}

bool CaptureManager::CheckDeviceFailure()
{
    for (auto& device : Devices) {
        if (device->DeviceFailed()) {
            return true;
        }
    }
    return false;
}

void CaptureManager::BackgroundStop()
{
    if (DeviceCount <= 0) {
        return;
    }

    const uint64_t t0 = GetTimeUsec();

    spdlog::info("Starting shutdown");

    // Disable log callback before exiting
    ScopedFunction log_disable([]() {
        k4a_set_debug_message_handler(nullptr, nullptr, k4a_log_level_t::K4A_LOG_LEVEL_OFF);
    });

    // Must be performed in this order to avoid crashes on shutdown:
    StopAll();

    CloseAll();
    Devices.clear();
    DeviceCount = 0;

    // Clear extrinsics on stop to avoid poisoning the next run if a new camera is attached.
    RuntimeConfig->ClearExtrinsics();

    // Lighting is invalidated each time the cameras are re-opened
    RuntimeConfig->ClearLighting();

    // Wait for 100 msec after stopping to avoid triggering firmware bugs on the device or whatever.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("Full shutdown in {} msec", (t1 - t0) / 1000.f);
}

void CaptureManager::StopAll()
{
    // Stop all the cameras in parallel because each one takes half a second.
    std::vector<std::future<void>> futures;
    futures.reserve(Devices.size());
    for (auto& device : Devices) {
        auto fut = std::async([&] {
            device->Stop();
        });
        futures.push_back(std::move(fut));
    }
    std::for_each(futures.begin(), futures.end(), [](std::future<void> & fut) {
        fut.wait();
    });
}

void CaptureManager::CloseAll()
{
    // Stop all the cameras in parallel because each one takes half a second.
    std::vector<std::future<void>> futures;
    futures.reserve(Devices.size());
    for (auto& device : Devices) {
        auto fut = std::async([&] {
            device->Close();
        });
        futures.push_back(std::move(fut));
    }
    std::for_each(futures.begin(), futures.end(), [](std::future<void> & fut) {
        fut.wait();
    });
}

void CaptureManager::OnImage(std::shared_ptr<RgbdImage>& image)
{
    if (RuntimeConfig->Mode == CaptureMode::Disabled) {
        return;
    }

    // If already matched:
    if (image->Matched) {
        return; // Do not match twice
    }

    //spdlog::info("[{}] Image t={}", image->DeviceIndex, image->ColorDeviceUsec);

    const unsigned device_index = image->DeviceIndex;
    const unsigned count = DeviceCount;

    std::lock_guard<std::mutex> locker(BatchLock);

    if (!LatestBatch) {
        LatestBatch = std::make_shared<ImageBatch>();
    }

    auto& images = LatestBatch->Images;

    images.clear();
    images.resize(count);
    images[device_index] = image;
    ScopedFunction set_scope([&images]() {
        images.clear();
    });

    for (unsigned i = 0; i < count; ++i)
    {
        if (i == device_index) {
            continue;
        }

        std::shared_ptr<RgbdImage> image_i = Devices[i]->FindCapture(image->SyncSystemUsec);
        if (!image_i) {
            return; // No match found
        }
        images[i] = image_i;
    }

    // Mark as matched
    for (unsigned i = 0; i < count; ++i) {
        images[i]->Matched = true;
    }

    Processor.OnBatch(LatestBatch); 

    set_scope.Cancel();
    LatestBatch.reset();
}


} // namespace core
