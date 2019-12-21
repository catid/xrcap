// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureDevice.hpp"

#include "CaptureSettings.hpp"

namespace core {


//------------------------------------------------------------------------------
// Constants

const char* CameraStatusToString(CameraStatus status)
{
    switch (status)
    {
    case CameraStatus::Idle: return "Idle";
    case CameraStatus::Initializing: return "Initializing";
    case CameraStatus::StartFailed: return "Start Failed";
    case CameraStatus::Capturing: return "Capturing";
    case CameraStatus::ReadFailed: return "Read Failed";
    case CameraStatus::SlowWarning: return "Slow Warning";
    default: break;
    }
    return "(Unknown)";
}

bool CameraStatusFailed(CameraStatus status)
{
    switch (status)
    {
    case CameraStatus::StartFailed: return true;
    case CameraStatus::ReadFailed: return true;
    case CameraStatus::SlowWarning: return true;
    default: break;
    }
    return false;
}


//------------------------------------------------------------------------------
// K4aDevice

bool K4aDevice::Open(
    const uint32_t index,
    const K4aDeviceSettings& settings,
    ImageCallback callback)
{
    Status = CameraStatus::Initializing;
    NeedsReset = false;

    Settings = settings;
    DeviceIndex = index;
    Info.DeviceIndex = index;
    Callback = callback;

    memset(&LastImuSample, 0, sizeof(LastImuSample));

    k4a_result_t result;

    for (int i = 0; i < 10; ++i) {
        result = k4a_device_open(index, &Device);
        if (K4A_FAILED(result)) {
            spdlog::error("[{}] k4a_device_open failed {} - Retrying {}", DeviceIndex, k4a_result_to_string(result), i);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else {
            break;
        }
    }
    if (K4A_FAILED(result)) {
        Status = CameraStatus::StartFailed;
        spdlog::error("[{}] k4a_device_open failed {} - Giving up", DeviceIndex, k4a_result_to_string(result));
        return false;
    }

    spdlog::info("[{}] Device open", DeviceIndex);

    result = k4a_device_get_version(Device, &Info.Version);
    if (K4A_FAILED(result)) {
        Status = CameraStatus::StartFailed;
        spdlog::error("[{}] k4a_device_get_version failed {}", DeviceIndex, k4a_result_to_string(result));
        return false;
    }
    Info.SerialNumber = K4aReadDeviceSerial(Device);

    spdlog::info("[{}] + Device serial = `{}` Firmware RGB={}.{}.{} depth={}.{}.{} depth.sensor={}.{}.{}",
        DeviceIndex, Info.SerialNumber,
        Info.Version.rgb.major,
        Info.Version.rgb.minor,
        Info.Version.rgb.iteration,
        Info.Version.depth.major,
        Info.Version.depth.minor,
        Info.Version.depth.iteration,
        Info.Version.depth_sensor.major,
        Info.Version.depth_sensor.minor,
        Info.Version.depth_sensor.iteration);

    result = k4a_device_get_sync_jack(
        Device,
        &Info.sync_in_jack_connected,
        &Info.sync_out_jack_connected);
    if (K4A_FAILED(result)) {
        spdlog::warn("[{}] k4a_device_get_sync_jack failed {}", DeviceIndex, k4a_result_to_string(result));
    } else {
        spdlog::info("[{}] + sync_in_jack_connected={} sync_out_jack_connected={}",
            DeviceIndex, Info.sync_in_jack_connected, Info.sync_out_jack_connected);
    }

    for (int i = 0; i < K4A_CONTROL_COUNT; ++i)
    {
        if (i == K4A_COLOR_CONTROL_AUTO_EXPOSURE_PRIORITY) {
            continue; // Skip deprecated
        }
        GetControlInfo(
            static_cast<k4a_color_control_command_t>(i),
            Controls[i]);
        PrintControlInfo(Controls[i]);
    }

    k4a_calibration_t calibration{};

    result = k4a_device_get_calibration(
        Device,
        Settings.DepthMode,
        Settings.ColorResolution,
        &calibration);
    if (K4A_FAILED(result)) {
        Status = CameraStatus::StartFailed;
        spdlog::error("[{}] k4a_device_get_calibration failed {}", DeviceIndex, k4a_result_to_string(result));
        return false;
    }
    CalibrationFromK4a(calibration, Info.Calibration);

    Mesher = std::make_shared<DepthMesher>();
    Mesher->Initialize(Info.Calibration);

    // Set default settings in case the device had some manual settings from the last run
    SetControlAuto(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE); // Auto
    //SetControlManual(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, 33000); // 33 msec
    // Hardware does not support K4A_COLOR_CONTROL_AUTO_EXPOSURE_PRIORITY
    SetControlDefault(K4A_COLOR_CONTROL_BRIGHTNESS);
    SetControlDefault(K4A_COLOR_CONTROL_CONTRAST);
    SetControlDefault(K4A_COLOR_CONTROL_SATURATION);
    SetControlDefault(K4A_COLOR_CONTROL_SHARPNESS);
    SetControlAuto(K4A_COLOR_CONTROL_WHITEBALANCE);
    //SetControlManual(K4A_COLOR_CONTROL_WHITEBALANCE, 2500);
    SetControlManual(K4A_COLOR_CONTROL_BACKLIGHT_COMPENSATION, 0); // Off
    SetControlManual(K4A_COLOR_CONTROL_GAIN, 1);
    SetControlManual(K4A_COLOR_CONTROL_POWERLINE_FREQUENCY, 2); // 60 Hz

#if 0
    SetControlManual(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, 33000);
    SetControlManual(K4A_COLOR_CONTROL_WHITEBALANCE, 2500);
    SetControlManual(K4A_COLOR_CONTROL_BRIGHTNESS, 128);
    SetControlManual(K4A_COLOR_CONTROL_GAIN, 1);
#endif

    protos::CameraExtrinsics extrinsics{};
    if (!Info.SerialNumber.empty() && LoadFromFile(GetSettingsFilePath("xrcap", FileNameFromSerial(Info.SerialNumber)), extrinsics)) {
        RuntimeConfig->SetExtrinsics(DeviceIndex, extrinsics);
        spdlog::info("[{}] Successfully restored extrinsics", DeviceIndex);
    }

    return true;
}

bool K4aDevice::SetControlAuto(k4a_color_control_command_t command)
{
    k4a_result_t result = k4a_device_set_color_control(
        Device,
        command,
        K4A_COLOR_CONTROL_MODE_AUTO,
        Controls[command].default_value);

    if (K4A_FAILED(result)) {
        spdlog::error("[{}] SetControlAuto failed: k4a_device_set_color_control({}) failed {}",
            DeviceIndex,
            k4a_color_control_command_to_string(command),
            k4a_result_to_string(result));
        return false;
    }
    return true;
}

bool K4aDevice::SetControlManual(k4a_color_control_command_t command, int32_t value)
{
    k4a_result_t result = k4a_device_set_color_control(
        Device,
        command,
        K4A_COLOR_CONTROL_MODE_MANUAL,
        value);

    if (K4A_FAILED(result)) {
        spdlog::error("[{}] SetControlManual failed: k4a_device_set_color_control({}) failed {}",
            DeviceIndex,
            k4a_color_control_command_to_string(command),
            k4a_result_to_string(result));
        return false;
    }
    return true;
}

bool K4aDevice::SetControlDefault(k4a_color_control_command_t command)
{
    k4a_result_t result = k4a_device_set_color_control(
        Device,
        command,
        Controls[command].default_mode,
        Controls[command].default_value);

    if (K4A_FAILED(result)) {
        spdlog::error("[{}] SetControlDefault failed: k4a_device_set_color_control({}) failed {}",
            DeviceIndex,
            k4a_color_control_command_to_string(command),
            k4a_result_to_string(result));
        return false;
    }
    return true;
}

bool K4aDevice::StartImageCapture(
    k4a_wired_sync_mode_t sync_mode,
    int32_t depth_delay_off_color_usec)
{
    DepthDelayOffColorUsec = depth_delay_off_color_usec;

    k4a_result_t result;

    spdlog::info("[{}] Starting to capture as {} with depth-color delay offset {} usec",
        DeviceIndex,
        k4a_sync_mode_to_string(sync_mode),
        depth_delay_off_color_usec);

    const uint64_t t0 = GetTimeUsec();

    k4a_device_configuration_t config{};
    config.color_format = Settings.ImageFormat;
    config.camera_fps = Settings.CameraFPS;
    config.color_resolution = Settings.ColorResolution;
    config.depth_delay_off_color_usec = depth_delay_off_color_usec;
    config.depth_mode = Settings.DepthMode;
    config.disable_streaming_indicator = false; // Keep it on
    config.subordinate_delay_off_master_usec = 0; // Should be the same
    config.synchronized_images_only = true; // TBD: We need both
    config.wired_sync_mode = sync_mode;

    result = k4a_device_start_cameras(Device, &config);
    if (K4A_FAILED(result)) {
        Status = CameraStatus::StartFailed;
        spdlog::error("[{}] k4a_device_start_cameras failed {}", DeviceIndex, k4a_result_to_string(result));
        return false;
    }

    ExpectedFramerate = k4a_fps_to_int(Settings.CameraFPS);
    ExpectedFrameIntervalUsec = 1000000 / ExpectedFramerate;
    spdlog::debug("Configured Framerate={} -> Expected interval={} usec",
        ExpectedFramerate, ExpectedFrameIntervalUsec);

    ClockSync.Reset();

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("[{}] Starting capture took {} msec",
        DeviceIndex, (t1 - t0) / 1000.f);

    Terminated = false;
    CameraThread = std::make_shared<std::thread>(&K4aDevice::CameraLoop, this);

    return true;
}

bool K4aDevice::StartImuCapture()
{
    k4a_result_t result;

    result = k4a_device_start_imu(Device);
    if (K4A_FAILED(result)) {
        Status = CameraStatus::StartFailed;
        spdlog::error("[{}] k4a_device_start_imu failed {}", DeviceIndex, k4a_result_to_string(result));
        return false;
    }

    ImuThread = std::make_shared<std::thread>(&K4aDevice::ImuLoop, this);

    Status = CameraStatus::Capturing;

    return true;
}

void K4aDevice::Stop()
{
    Status = CameraStatus::Idle;

    if (Terminated) {
        return;
    }

    const uint64_t t0 = GetTimeUsec();

    Terminated = true;

    JoinThread(ImuThread);
    JoinThread(CameraThread);

    if (Device != 0) {
        k4a_device_stop_cameras(Device);
        k4a_device_stop_imu(Device);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    for (int i = 0; i < kCaptureHistoryCount; ++i) {
        CaptureHistory[i].reset();
    }

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("[{}] Stop took {} msec", DeviceIndex, (t1 - t0) / 1000.f);
}

void K4aDevice::Close()
{
    Stop();

    if (Device != 0) {
        k4a_device_close(Device);
        Device = 0;
    }
}

void K4aDevice::ImuLoop()
{
    SetCurrentThreadName("K4aIMU");

    // TBD: Add watchdog here too?

    while (!Terminated)
    {
        k4a_imu_sample_t sample{};
        k4a_wait_result_t wait = k4a_device_get_imu_sample(
            Device,
            &sample,
            100); // msec

        if (wait == K4A_WAIT_RESULT_SUCCEEDED)
        {
            std::lock_guard<std::mutex> locker(ImuLock);
            LastImuSample = sample;
        }
        else if (wait == k4a_wait_result_t::K4A_WAIT_RESULT_TIMEOUT)
        {
            continue; // No data keep waiting
        }
        else // Failed:
        {
            Status = CameraStatus::ReadFailed;
            spdlog::error("[{}] k4a_device_get_imu_sample failed", DeviceIndex);
            if (Terminated) {
                break;
            }
            // Add a sleep to avoid hard spinning on errors...
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void K4aDevice::CameraLoop()
{
    SetCurrentThreadName("K4aCamera");

    uint64_t t0 = 0;
    uint64_t last_frame_msec = GetTimeMsec();

    while (!Terminated)
    {
        // Lockless read of index
        const int write_capture_index = WriteCaptureIndex;

        // Release historical capture
        CaptureHistory[write_capture_index].reset();

        k4a_capture_t capture = 0;
        k4a_wait_result_t wait = k4a_device_get_capture(
            Device,
            &capture,
            100); // msec

        const uint64_t t1 = GetTimeMsec();

        if (wait == K4A_WAIT_RESULT_SUCCEEDED && capture != 0)
        {
            last_frame_msec = t1;

            if (t1 - t0 > 3000) {
                Status = CameraStatus::Capturing;
                t0 = t1;
            }

            OnCapture(write_capture_index, capture);

            k4a_capture_release(capture);
            capture = 0;

            NeedsReset = false;

            PeriodicChecks();
        }
        else if (wait == k4a_wait_result_t::K4A_WAIT_RESULT_FAILED)
        {
            Status = CameraStatus::ReadFailed;
            spdlog::error("[{}] k4a_device_get_capture failed", DeviceIndex);
            if (Terminated) {
                break;
            }
            // Add a sleep to avoid hard spinning on errors...
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        static const unsigned kDeviceTimeoutMsec = 10000;
        if (t1 - last_frame_msec >= kDeviceTimeoutMsec) {
            NeedsReset = true;
        }
    }
}

void K4aDevice::PeriodicChecks()
{
#if 0
    k4a_color_control_mode_t mode0, mode1, mode2;
    int32_t brightness, gain, awb;
    k4a_device_get_color_control(Device, K4A_COLOR_CONTROL_BRIGHTNESS, &mode0, &brightness);
    k4a_device_get_color_control(Device, K4A_COLOR_CONTROL_GAIN, &mode1, &gain);
    k4a_device_get_color_control(Device, K4A_COLOR_CONTROL_GAIN, &mode2, &awb);
    spdlog::info("mode={} gain={} mode={} brightness={} mode={} awb={}", mode1, gain, mode0, brightness, mode2, awb);
#endif

    const uint32_t exposure_epoch = RuntimeConfig->ExposureEpoch;
    if (ExposureEpoch != exposure_epoch) {
        ExposureEpoch = exposure_epoch;
        UpdateExposure();
    }

    const uint32_t extrinsics_epoch = RuntimeConfig->ExtrinsicsEpoch;
    if (ExtrinsicsEpoch != extrinsics_epoch) {
        ExtrinsicsEpoch = extrinsics_epoch;
        WriteExtrinsics();
    }
}

void K4aDevice::UpdateExposure()
{
    const protos::MessageSetExposure exposure = RuntimeConfig->GetExposure();
    if (exposure.AutoEnabled)
    {
        spdlog::error("[{}] Setting auto exposure", DeviceIndex);
        SetControlAuto(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE); // Auto
        SetControlAuto(K4A_COLOR_CONTROL_WHITEBALANCE);
        //SetControlDefault(K4A_COLOR_CONTROL_BRIGHTNESS);
        //SetControlManual(K4A_COLOR_CONTROL_GAIN, 1);
    }
    else
    {
        spdlog::error("[{}] Setting manual exposure={} awb={}",
            DeviceIndex, exposure.ExposureUsec, exposure.AutoWhiteBalanceUsec);
        SetControlManual(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, exposure.ExposureUsec);
        SetControlManual(K4A_COLOR_CONTROL_WHITEBALANCE, exposure.AutoWhiteBalanceUsec);
        //SetControlManual(K4A_COLOR_CONTROL_BRIGHTNESS, 128);
        //SetControlManual(K4A_COLOR_CONTROL_GAIN, 1);
    }
}

void K4aDevice::WriteExtrinsics()
{
    auto all_extrinsics = RuntimeConfig->GetExtrinsics();
    if (DeviceIndex >= all_extrinsics.size()) {
        return;
    }
    const auto& extrinsics = all_extrinsics[DeviceIndex];

    // No extrinsics to write
    if (extrinsics.IsIdentity) {
        return;
    }

    const std::string serial = GetInfo().SerialNumber;
    spdlog::debug("[{}] Writing extrinsics for serial={}", DeviceIndex, serial);

    SaveToFile(extrinsics, GetSettingsFilePath("xrcap", FileNameFromSerial(serial)));
}

std::shared_ptr<RgbdImage> K4aDevice::FindCapture(uint64_t SyncSystemUsec)
{
    std::lock_guard<std::mutex> locker(CaptureHistoryLock);

    const int write_index = WriteCaptureIndex;

    for (int i = 0; i < kCaptureHistoryCount; ++i)
    {
        if (i == write_index) {
            continue;
        }

        auto& image = CaptureHistory[i];
        if (!image || image->Matched) {
            continue;
        }

        // In practice the match distance is very small, under a millisecond.
        // If one of the cameras is on an external USB hub then the frames
        // from one camera arrive 3 milliseconds later which means even through
        // a chain of 6 hubs we can correctly match frames from different cameras.
        int64_t delta_usec = SyncSystemUsec - image->SyncSystemUsec;
        if (delta_usec < 0) {
            delta_usec = -delta_usec;
        }

        // If there is a match:
        if (delta_usec < kMatchDistUsec) {
            return image;
        }
    }

    return nullptr;
}

void K4aDevice::OnCapture(int write_capture_index, k4a_capture_t capture)
{
    if (Terminated) {
        return;
    }

    k4a_image_t ColorImage = 0;
    k4a_image_t DepthImage = 0;

    std::shared_ptr<RgbdImage> image = std::make_shared<RgbdImage>();

    image->DeviceIndex = DeviceIndex;
    image->Mesher = Mesher;
    image->FrameNumber = NextFrameNumber++;
    image->Framerate = ExpectedFramerate;

    ColorImage = k4a_capture_get_color_image(capture);
    if (ColorImage == 0) {
        spdlog::error("null color image");
        return;
    }
    ScopedFunction color_scope([&]() {
        k4a_image_release(ColorImage);
        ColorImage = 0;
    });

    const k4a_image_format_t format = k4a_image_get_format(ColorImage);
    if (format != Settings.ImageFormat) {
        spdlog::error("wrong color buffer");
        return;
    }

    DepthImage = k4a_capture_get_depth_image(capture);
    if (DepthImage == 0) {
        spdlog::error("null depth image");
        return;
    }
    ScopedFunction depth_scope([&]() {
        k4a_image_release(DepthImage);
        DepthImage = 0;
    });

    // Read latest IMU sample
    {
        std::lock_guard<std::mutex> locker(ImuLock);
        image->AccelerationSample = Eigen::Vector3f(
            LastImuSample.acc_sample.xyz.x,
            LastImuSample.acc_sample.xyz.y,
            LastImuSample.acc_sample.xyz.z);
    }

    image->TemperatureC = k4a_capture_get_temperature_c(capture);

    // Copy depth
    image->DepthWidth = k4a_image_get_width_pixels(DepthImage);
    image->DepthHeight = k4a_image_get_height_pixels(DepthImage);
    image->DepthStride = k4a_image_get_stride_bytes(DepthImage);
    const unsigned depth_size = image->DepthStride * image->DepthHeight;
    image->DepthImage.resize(depth_size);
    const uint16_t* depth_image = \
        reinterpret_cast<const uint16_t*>( k4a_image_get_buffer(DepthImage) );
    memcpy(image->DepthImage.data(), depth_image, depth_size * 2);

    // Copy color
    image->ColorWidth = k4a_image_get_width_pixels(ColorImage);
    image->ColorHeight = k4a_image_get_height_pixels(ColorImage);
    image->ColorStride = k4a_image_get_stride_bytes(ColorImage);
    const uint8_t* color_image = \
        reinterpret_cast<const uint8_t*>( k4a_image_get_buffer(ColorImage) );
    const size_t color_size = k4a_image_get_size(ColorImage);
    image->ColorImage.resize(color_size);
    memcpy(image->ColorImage.data(), color_image, color_size);

    image->DepthDeviceUsec = k4a_image_get_device_timestamp_usec(DepthImage);
    image->DepthSystemUsec = k4a_image_get_system_timestamp_nsec(DepthImage) / 1000;

    const uint64_t interval_usec = image->DepthDeviceUsec - LastDepthDeviceUsec;
    if (LastDepthDeviceUsec != 0 && interval_usec > ExpectedFrameIntervalUsec * 3 / 2) {
        spdlog::warn("[{}] Slow RGBD image interval: {} msec!  CPU load may be too high.",
            DeviceIndex,
            (interval_usec / 1000.f));
        Status = CameraStatus::SlowWarning;
    }
    LastDepthDeviceUsec = image->DepthDeviceUsec;

    image->ColorDeviceUsec = k4a_image_get_device_timestamp_usec(ColorImage);
    image->ColorSystemUsec = k4a_image_get_system_timestamp_nsec(ColorImage) / 1000;
    image->ColorExposureUsec = k4a_image_get_exposure_usec(ColorImage);
    image->ColorWhiteBalanceUsec = k4a_image_get_white_balance(ColorImage);
    image->ColorIsoSpeed = k4a_image_get_iso_speed(ColorImage);

    image->IsJpegBuffer = (Settings.ImageFormat == K4A_IMAGE_FORMAT_COLOR_MJPG);

    // Depth timestamp is better to use than color due to super short exposure time,
    // but we do need to apply the depth delay offset for each camera.
    image->SyncDeviceUsec = image->DepthDeviceUsec - DepthDelayOffColorUsec;

    image->SyncSystemUsec = ClockSync.CalculateSyncSystemUsec(image->DepthSystemUsec, image->SyncDeviceUsec);

    // Offset by half of exposure time to when we actually read it off USB
    image->SyncSystemUsec += image->ColorExposureUsec / 2;

    // Store image to capture history to allow cross-camera matching
    {
        std::lock_guard<std::mutex> locker(CaptureHistoryLock);

        CaptureHistory[write_capture_index] = image;

        ++write_capture_index;
        if (write_capture_index >= kCaptureHistoryCount) {
            write_capture_index = 0;
        }
        WriteCaptureIndex = write_capture_index;
    }

    Callback(image);
}

bool K4aDevice::GetControlInfo(k4a_color_control_command_t command, ControlInfo& info)
{
    k4a_result_t result;

    info.valid = false;
    info.command = command;

    result = k4a_device_get_color_control_capabilities(
        Device,
        command,
        &info.supports_auto,
        &info.min_value,
        &info.max_value,
        &info.step_value,
        &info.default_value,
        &info.default_mode);
    if (K4A_FAILED(result)) {
        spdlog::error("[{}] k4a_device_get_color_control_capabilities({}) failed ",
            DeviceIndex, k4a_color_control_command_to_string(command));
        return false;
    }

    info.valid = true;
    return true;
}

void K4aDevice::PrintControlInfo(ControlInfo& info)
{
    if (!info.valid)
    {
        spdlog::info("[{}] {} - failed to query",
            DeviceIndex,
            k4a_color_control_command_to_string(info.command));
        return;
    }

    std::string has_auto = info.supports_auto ? "auto" : "manual";
    std::string default_auto = info.default_mode == K4A_COLOR_CONTROL_MODE_AUTO ? "auto" : "manual";

    spdlog::info("[{}] ++ {} ({}): min={} max={} step={} def.value={} def.mode={}",
        DeviceIndex,
        k4a_color_control_command_to_string(info.command),
        has_auto,
        info.min_value,
        info.max_value,
        info.step_value,
        info.default_value,
        default_auto);
}


} // namespace core
