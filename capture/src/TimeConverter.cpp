// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "TimeConverter.hpp"

#include <core.hpp>
#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// DeviceClockSync

void DeviceClockSync::Reset()
{
    MinDeltas.Reset();
}

uint64_t DeviceClockSync::CalculateSyncSystemUsec(
    uint64_t system_clock_usec,
    uint64_t sync_device_usec)
{
    // ts_delta = (Clock offset) + (Delay between capture and reading).
    // We assume the delay between capture and reading is similar for cameras
    const int64_t ts_delta = system_clock_usec - sync_device_usec;

    // Window size = 30 seconds / 900 frames to account for clock skew/drift
    const uint64_t kWindowLengthUsec = 30 * 1000 * 1000;

    MinDeltas.Update(ts_delta, sync_device_usec, kWindowLengthUsec);

    // This is System - Device_i time
    const int64_t delta_usec = MinDeltas.GetBest();

    // Convert to system time
    return sync_device_usec + delta_usec;
}


//------------------------------------------------------------------------------
// VideoTimestampCleaner

uint64_t VideoTimestampCleaner::Clean(
    uint64_t shutter_device_usec,
    uint64_t shutter_system_usec,
    bool& discontinuity)
{
    discontinuity = true;

    // Check diff between shutter system time and the last returned system time
    const int64_t system_diff = static_cast<int64_t>(shutter_system_usec - LastReturnedSystemUsec);

    // Check diff between current shutter device time and the last device time
    const int64_t device_diff = static_cast<int64_t>(shutter_device_usec - LastDeviceUsec);

    LastReturnedSystemUsec = shutter_system_usec;
    LastDeviceUsec = shutter_device_usec;

    if (device_diff < 5000) {
        spdlog::info("Device time reset backwards: device={} (diff={}) usec",
            shutter_device_usec, device_diff);
        return shutter_system_usec;
    }
    if (device_diff > 300000) {
        spdlog::info("Device time jumped forward: device={} (diff={}) usec",
            shutter_device_usec, device_diff);
        return shutter_system_usec;
    }

    if (system_diff < 5000) {
        spdlog::info("System time reset backwards: system={} (diff={}) usec",
            shutter_system_usec, system_diff);
        return shutter_system_usec;
    }

    if (system_diff > device_diff * 2) {
        spdlog::debug("System time jumped forward: system={} (diff={}) usec",
            shutter_system_usec, system_diff);
        return shutter_system_usec;
    }

    // Expected interval - actual interval
    int64_t mismatch_usec = device_diff - system_diff;

    // Bound the mismatch that we apply so it slowly smooths over issues
    if (mismatch_usec > kMaxMismatchUsec) {
        mismatch_usec = kMaxMismatchUsec;
    }
    else if (mismatch_usec < -kMaxMismatchUsec) {
        mismatch_usec = -kMaxMismatchUsec;
    }

    // Apply correction
    shutter_system_usec += mismatch_usec;

    LastReturnedSystemUsec = shutter_system_usec;
    discontinuity = false;
    return shutter_system_usec;
}


} // namespace core
