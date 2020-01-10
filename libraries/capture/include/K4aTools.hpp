// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Azure Kinect DK:
    + 5x Microphones
    + IMU
    + Depth sensor
    + Color camera
    + IR camera
*/

#pragma once

#include <k4a/k4a.h> // Azure Kinect SDK (C API)

#include <core_logging.hpp>
#include <DepthCalibration.hpp> // depth_mesh

#include <cstdint>
#include <string>
#include <vector>

namespace core {


//------------------------------------------------------------------------------
// Constants

/*
    Configuring camera captures:

    From https://docs.microsoft.com/en-us/azure/Kinect-dk/hardware-specification
    "Depth camera captures should be offset from one another by 160us or more
    to avoid interference."

    We assume the sync pulse is pretty close (within a microsecond) for all cameras.
    So all color captures should happen simultaenously.

    We need to offset the depth captures at least 160 usec from eachother,
    and we want them to be as close as possible so all depth captures are
    seeing about the same instant in time.

    We want the color images to be at the center of the depth captures,
    so we put half of the cameras behind and half ahead of the color time.
*/

static const int32_t kDepthOffsetUsec = 160;


//------------------------------------------------------------------------------
// Tools

struct ControlInfo
{
    bool valid = false;
    k4a_color_control_command_t command = K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE;
    bool supports_auto = false;
    int32_t min_value = 0, max_value = 0, step_value = 0, default_value = 0;
    k4a_color_control_mode_t default_mode = K4A_COLOR_CONTROL_MODE_AUTO;
};

// Convert some enums to strings
const char* k4a_result_to_string(k4a_result_t result);
const char* k4a_wait_result_to_string(k4a_wait_result_t result);
const char* k4a_buffer_result_to_string(k4a_buffer_result_t result);
const char* k4a_color_control_command_to_string(k4a_color_control_command_t command);
const char* k4a_sync_mode_to_string(k4a_wired_sync_mode_t mode);

// Compare hardware versions
bool operator==(const k4a_version_t& lhs, const k4a_version_t& rhs);
bool operator!=(const k4a_version_t& lhs, const k4a_version_t& rhs);
bool operator==(const k4a_hardware_version_t& lhs, const k4a_hardware_version_t& rhs);
bool operator!=(const k4a_hardware_version_t& lhs, const k4a_hardware_version_t& rhs);

// More string conversion
std::string k4a_float2_to_string(const k4a_float2_t& p);

// Number of controls offered
#define K4A_CONTROL_COUNT (K4A_COLOR_CONTROL_POWERLINE_FREQUENCY + 1)

std::string K4aReadDeviceSerial(k4a_device_t device);

spdlog::level::level_enum K4ALogLevelConvert(k4a_log_level_t level);

unsigned k4a_fps_to_int(k4a_fps_t fps);

// Faster custom allocator for k4a
uint8_t* k4a_alloc(int size, void** context);
void k4a_free(void* buffer, void* context);


//------------------------------------------------------------------------------
// Calibration

// Convert from k4a API to common API
core::LensModels LensModelFromK4a(k4a_calibration_model_type_t type);

void CalibrationFromK4a(
    const k4a_calibration_t& from,
    core::CameraCalibration& to);


} // namespace core
