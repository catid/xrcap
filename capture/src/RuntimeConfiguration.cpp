// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "RuntimeConfiguration.hpp"

#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// Constants

const char* CaptureModeToString(CaptureMode mode)
{
    switch (mode)
    {
    case CaptureMode::Disabled: return "Disabled";
    case CaptureMode::Calibration: return "Calibration";
    case CaptureMode::CaptureLowQual: return "CaptureLowQual";
    case CaptureMode::CaptureHighQual: return "CaptureHighQual";
    default: break;
    }
    return "(Unknown)";
}


//------------------------------------------------------------------------------
// RuntimeConfiguration

void RuntimeConfiguration::SetLighting(const protos::MessageSetLighting& lighting)
{
    std::lock_guard<std::mutex> locker(Lock);
    if (lighting.CameraIndex >= Lighting.size()) {
        Lighting.resize(lighting.CameraIndex + 1);
    }
    Lighting[lighting.CameraIndex] = lighting;
}

protos::MessageSetLighting RuntimeConfiguration::GetLighting(int camera_index) const
{
    std::lock_guard<std::mutex> locker(Lock);
    if (camera_index >= (int)Lighting.size()) {
        return protos::MessageSetLighting();
    }
    return Lighting[camera_index];
}

void RuntimeConfiguration::ClearLighting()
{
    std::lock_guard<std::mutex> locker(Lock);
    Lighting.clear();
}

void RuntimeConfiguration::SetExposure(const protos::MessageSetExposure& exposure)
{
    std::lock_guard<std::mutex> locker(Lock);
    Exposure = exposure;
    ++ExposureEpoch;
}

protos::MessageSetExposure RuntimeConfiguration::GetExposure() const
{
    std::lock_guard<std::mutex> locker(Lock);
    return Exposure;
}

void RuntimeConfiguration::SetClip(const protos::MessageSetClip& clip)
{
    std::lock_guard<std::mutex> locker(Lock);
    Clip = clip;
    ++ClipEpoch;
}

bool RuntimeConfiguration::ShouldClip(unsigned device_index, ClipRegion& clip) const
{
    // If in calibration mode:
    if (Mode == CaptureMode::Calibration) {
        return false;
    }

    std::lock_guard<std::mutex> locker(Lock);

    // If clip disabled:
    if (!Clip.Enabled) {
        return false;
    }

    // If no extrinsics calibration yet:
    if (device_index >= Extrinsics.size()) {
        return false;
    }

    if (Extrinsics[device_index].IsIdentity) {
        return false;
    }

    auto& transform = Extrinsics[device_index].Transform;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            clip.Extrinsics(row, col) = transform[row * 4 + col];
        }
    }

    clip.Radius = Clip.ClipRadiusMeters;
    clip.Floor = Clip.ClipFloorMeters;
    clip.Ceiling = Clip.ClipCeilingMeters;
    return (clip.Radius > 0.f);
}

void RuntimeConfiguration::SetCompression(const protos::CompressionSettings& compression)
{
    std::lock_guard<std::mutex> locker(Lock);
    Compression = compression;
}

protos::CompressionSettings RuntimeConfiguration::GetCompression() const
{
    std::lock_guard<std::mutex> locker(Lock);
    return Compression;
}

void RuntimeConfiguration::SetExtrinsics(unsigned device_index, const protos::CameraExtrinsics& extrinsics)
{
    spdlog::info("Updating extinstrics for camera {}: identity={}",
        device_index, extrinsics.IsIdentity);

    std::lock_guard<std::mutex> locker(Lock);

    if (device_index >= Extrinsics.size()) {
        Extrinsics.resize(device_index + 1);
    }
    Extrinsics[device_index] = extrinsics;
    ++ExtrinsicsEpoch;
}

std::vector<protos::CameraExtrinsics> RuntimeConfiguration::GetExtrinsics() const
{
    std::lock_guard<std::mutex> locker(Lock);
    return Extrinsics;
}

void RuntimeConfiguration::ClearExtrinsics()
{
    std::lock_guard<std::mutex> locker(Lock);
    Extrinsics.clear();
    ++ExtrinsicsEpoch;
}


} // namespace core
