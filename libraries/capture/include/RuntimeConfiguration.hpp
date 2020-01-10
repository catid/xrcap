// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

/*
    RuntimeConfiguration module

    This provides a thread-safe way to configure the multiple subsystems
    from a single interface.
*/

#include <DepthMesh.hpp> // depth_mesh
#include <CaptureProtocol.hpp> // capture_protocol

#include <atomic>
#include <mutex>

namespace core {


//------------------------------------------------------------------------------
// Constants

// User/application requested mode
enum class CaptureMode
{
    Disabled,
    Calibration,
    CaptureLowQual,
    CaptureHighQual,

    Count
};

const char* CaptureModeToString(CaptureMode mode);


//------------------------------------------------------------------------------
// RuntimeConfiguration

// Thread-safe wrapper around ClipRegion and other things
class RuntimeConfiguration
{
public:
    // This should be set via CaptureManager::SetMode() rather than directly
    std::atomic<CaptureMode> Mode = ATOMIC_VAR_INIT(CaptureMode::Disabled);

    // These can be changed at any time
    std::atomic<bool> ImagesNeeded = ATOMIC_VAR_INIT(true);
    std::atomic<bool> VideoNeeded = ATOMIC_VAR_INIT(true);

    void SetClip(const protos::MessageSetClip& clip);
    bool ShouldClip(unsigned device_index, ClipRegion& clip) const;

    void SetExposure(const protos::MessageSetExposure& exposure);
    protos::MessageSetExposure GetExposure() const;

    void SetLighting(const protos::MessageSetLighting& lighting);
    protos::MessageSetLighting GetLighting(int camera_index) const;
    void ClearLighting();

    void SetCompression(const protos::CompressionSettings& compression);
    protos::CompressionSettings GetCompression() const;

    // This is a number that changes whenever the capture configuration changes
    std::atomic<uint32_t> CaptureConfigEpoch = ATOMIC_VAR_INIT(0);

    std::atomic<bool> NeedsKeyframe = ATOMIC_VAR_INIT(false);

    void SetExtrinsics(unsigned device_index, const protos::CameraExtrinsics& extrinsics);
    std::vector<protos::CameraExtrinsics> GetExtrinsics() const;
    void ClearExtrinsics();

    std::atomic<uint32_t> ExtrinsicsEpoch = ATOMIC_VAR_INIT(0);
    std::atomic<uint32_t> ClipEpoch = ATOMIC_VAR_INIT(0);
    std::atomic<uint32_t> ExposureEpoch = ATOMIC_VAR_INIT(0);

protected:
    mutable std::mutex Lock;
    protos::MessageSetClip Clip;
    protos::MessageSetExposure Exposure;
    std::vector<protos::MessageSetLighting> Lighting;

    std::vector<protos::CameraExtrinsics> Extrinsics;

    protos::CompressionSettings Compression{};
};


} // namespace core
