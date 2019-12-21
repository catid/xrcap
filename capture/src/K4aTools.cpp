// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "K4aTools.hpp"

#include <mimalloc.h> // Microsoft Research memory allocator

#include <sstream>

namespace core {


//------------------------------------------------------------------------------
// Tools

spdlog::level::level_enum K4ALogLevelConvert(k4a_log_level_t level)
{
    switch (level)
    {
    case k4a_log_level_t::K4A_LOG_LEVEL_CRITICAL:
        return spdlog::level::level_enum::critical;
    case k4a_log_level_t::K4A_LOG_LEVEL_ERROR:
        return spdlog::level::level_enum::err;
    case k4a_log_level_t::K4A_LOG_LEVEL_WARNING:
        return spdlog::level::level_enum::warn;
    case k4a_log_level_t::K4A_LOG_LEVEL_INFO:
        return spdlog::level::level_enum::info;
    case k4a_log_level_t::K4A_LOG_LEVEL_TRACE:
        return spdlog::level::level_enum::trace;
    case k4a_log_level_t::K4A_LOG_LEVEL_OFF:
        return spdlog::level::level_enum::off;
    default:
        break;
    }

    return spdlog::level::level_enum::debug;
}

uint8_t* k4a_alloc(int size, void** context)
{
    CORE_UNUSED(context);
    return reinterpret_cast<uint8_t*>( mi_malloc(size) );
}

void k4a_free(void* buffer, void* context)
{
    CORE_UNUSED(context);
    return mi_free(buffer);
}

const char* k4a_result_to_string(k4a_result_t result)
{
    switch (result)
    {
    case K4A_RESULT_SUCCEEDED: return "K4A_RESULT_SUCCEEDED";
    case K4A_RESULT_FAILED: return "K4A_RESULT_FAILED";
    default: break;
    }
    return "(Unknown)";
}

const char* k4a_wait_result_to_string(k4a_wait_result_t result)
{
    switch (result)
    {
    case K4A_WAIT_RESULT_SUCCEEDED: return "K4A_WAIT_RESULT_SUCCEEDED";
    case K4A_WAIT_RESULT_FAILED: return "K4A_WAIT_RESULT_FAILED";
    case K4A_WAIT_RESULT_TIMEOUT: return "K4A_WAIT_RESULT_TIMEOUT";
    default: break;
    }
    return "(Unknown)";
}

const char* k4a_buffer_result_to_string(k4a_buffer_result_t result)
{
    switch (result)
    {
    case K4A_BUFFER_RESULT_SUCCEEDED: return "K4A_BUFFER_RESULT_SUCCEEDED";
    case K4A_BUFFER_RESULT_FAILED: return "K4A_BUFFER_RESULT_FAILED";
    case K4A_BUFFER_RESULT_TOO_SMALL: return "K4A_BUFFER_RESULT_TOO_SMALL";
    default: break;
    }
    return "(Unknown)";
}

const char* k4a_color_control_command_to_string(k4a_color_control_command_t command)
{
    switch (command)
    {
    case K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE:
        return "EXPOSURE_TIME_ABSOLUTE";
    case K4A_COLOR_CONTROL_AUTO_EXPOSURE_PRIORITY:
        return "AUTO_EXPOSURE_PRIORITY";
    case K4A_COLOR_CONTROL_BRIGHTNESS:
        return "BRIGHTNESS";
    case K4A_COLOR_CONTROL_CONTRAST:
        return "CONTRAST";
    case K4A_COLOR_CONTROL_SATURATION:
        return "SATURATION";
    case K4A_COLOR_CONTROL_SHARPNESS:
        return "SHARPNESS";
    case K4A_COLOR_CONTROL_WHITEBALANCE:
        return "WHITEBALANCE";
    case K4A_COLOR_CONTROL_BACKLIGHT_COMPENSATION:
        return "BACKLIGHT_COMPENSATION";
    case K4A_COLOR_CONTROL_GAIN:
        return "GAIN";
    case K4A_COLOR_CONTROL_POWERLINE_FREQUENCY:
        return "POWERLINE_FREQUENCY";
    default: break;
    }
    return "(Unknown)";
}

const char* k4a_sync_mode_to_string(k4a_wired_sync_mode_t mode)
{
    switch (mode)
    {
    case K4A_WIRED_SYNC_MODE_STANDALONE: return "Sync Mode Standalone";
    case K4A_WIRED_SYNC_MODE_MASTER: return "Sync Mode Master";
    case K4A_WIRED_SYNC_MODE_SUBORDINATE: return "Sync Mode Subordinate";
    default: break;
    }
    return "(Unknown)";
}

unsigned k4a_fps_to_int(k4a_fps_t fps)
{
    switch (fps)
    {
    case K4A_FRAMES_PER_SECOND_5: return 5;
    case K4A_FRAMES_PER_SECOND_15: return 15;
    case K4A_FRAMES_PER_SECOND_30: return 30;
    default: break;
    }
    return 1;
}

bool operator==(const k4a_version_t& lhs, const k4a_version_t& rhs)
{
    return lhs.major == rhs.major &&
           lhs.minor == rhs.minor &&
           lhs.iteration == rhs.iteration;
}

bool operator!=(const k4a_version_t& lhs, const k4a_version_t& rhs)
{
    return !(lhs == rhs);
}

bool operator==(const k4a_hardware_version_t& lhs, const k4a_hardware_version_t& rhs)
{
    return lhs.rgb == rhs.rgb &&
           lhs.depth == rhs.depth &&
           lhs.depth_sensor == rhs.depth_sensor &&
           lhs.audio == rhs.audio;
}

bool operator!=(const k4a_hardware_version_t& lhs, const k4a_hardware_version_t& rhs)
{
    return !(lhs == rhs);
}

std::string k4a_float2_to_string(const k4a_float2_t& p)
{
    std::ostringstream oss;
    oss << "(" << p.xy.x << ", " << p.xy.y << ")";
    return oss.str();
}

std::string K4aReadDeviceSerial(k4a_device_t device)
{
    char serial_number[256] = { '\0' };
    size_t serial_number_size = sizeof(serial_number);
    k4a_buffer_result_t buffer = k4a_device_get_serialnum(
        device,
        serial_number,
        &serial_number_size);
    if (K4A_FAILED(buffer)) {
        spdlog::error("k4a_device_get_serialnum failed");
        return "unknown";
    }

    serial_number[sizeof(serial_number) - 1] = '\0';
    return serial_number;
}


//------------------------------------------------------------------------------
// Calibration

LensModels LensModelFromK4a(k4a_calibration_model_type_t type)
{
    switch (type)
    {
    case K4A_CALIBRATION_LENS_DISTORTION_MODEL_THETA: return LensModel_Theta;
    case K4A_CALIBRATION_LENS_DISTORTION_MODEL_POLYNOMIAL_3K: return LensModel_Polynomial_3K;
    case K4A_CALIBRATION_LENS_DISTORTION_MODEL_RATIONAL_6KT: return LensModel_Rational_6KT;
    case K4A_CALIBRATION_LENS_DISTORTION_MODEL_BROWN_CONRADY: return LensModel_Brown_Conrady;
    default: break;
    }
    return LensModel_Unknown;
}

static void CopyIntrinsics(
    const k4a_calibration_camera_t& from,
    CameraIntrinsics& to)
{
    to.Width = from.resolution_width;
    to.Height = from.resolution_height;

    const k4a_calibration_intrinsic_parameters_t& params = from.intrinsics.parameters;
    to.cx = params.param.cx;
    to.cy = params.param.cy;
    to.fx = params.param.fx;
    to.fy = params.param.fy;
    to.k[0] = params.param.k1;
    to.k[1] = params.param.k2;
    to.k[2] = params.param.k3;
    to.k[3] = params.param.k4;
    to.k[4] = params.param.k5;
    to.k[5] = params.param.k6;
    to.codx = params.param.codx;
    to.cody = params.param.cody;
    to.p1 = params.param.p1;
    to.p2 = params.param.p2;

    //to.MaxRadiusForProjection = from.metric_radius;

    to.LensModel = LensModelFromK4a(from.intrinsics.type);
}

void CalibrationFromK4a(
    const k4a_calibration_t& from,
    CameraCalibration& to)
{
    CopyIntrinsics(from.depth_camera_calibration, to.Depth);
    CopyIntrinsics(from.color_camera_calibration, to.Color);

    // Extrinsics from depth to color camera
    const k4a_calibration_extrinsics_t* extrinsics = &from.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];
    for (int i = 0; i < 9; ++i) {
        to.RotationFromDepth[i] = extrinsics->rotation[i]; 
    }
    for (int i = 0; i < 3; ++i) {
        to.TranslationFromDepth[i] = extrinsics->translation[i]; 
    }
}


} // namespace core
