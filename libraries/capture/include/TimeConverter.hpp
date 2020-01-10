// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <cstdint>
#include <vector>

namespace core {


//------------------------------------------------------------------------------
// WindowedMinMax

struct WindowedMinCompareS64
{
    inline bool operator()(const int64_t x, const int64_t y) const
    {
        return (x - y) <= 0;
    }
};

struct WindowedMaxCompareS64
{
    inline bool operator()(const int64_t x, const int64_t y) const
    {
        return (x - y) >= 0;
    }
};

/// Templated class that calculates a running windowed minimum or maximum with
/// a fixed time and resource cost.
template<class CompareT> class WindowedMinMaxS64
{
public:
    typedef uint64_t TimeT;
    CompareT Compare;

    struct Sample
    {
        /// Sample value
        int64_t Value;

        /// Timestamp of data collection
        TimeT Timestamp;


        /// Default values and initializing constructor
        explicit Sample(int64_t value = 0, TimeT timestamp = 0)
            : Value(value)
            , Timestamp(timestamp)
        {
        }

        /// Check if a timeout expired
        inline bool TimeoutExpired(TimeT now, TimeT timeout)
        {
            return (TimeT)(now - Timestamp) > timeout;
        }
    };


    static const unsigned kSampleCount = 3;

    Sample Samples[kSampleCount];


    bool IsValid() const
    {
        return Samples[0].Value != 0; ///< ish
    }

    int64_t GetBest() const
    {
        return Samples[0].Value;
    }

    void Reset(const Sample sample = Sample())
    {
        Samples[0] = Samples[1] = Samples[2] = sample;
    }

    void Update(int64_t value, TimeT timestamp, const TimeT windowLengthTime)
    {
        const Sample sample(value, timestamp);

        // On the first sample, new best sample, or if window length has expired:
        if (!IsValid() ||
            Compare(value, Samples[0].Value) ||
            Samples[2].TimeoutExpired(sample.Timestamp, windowLengthTime))
        {
            Reset(sample);
            return;
        }

        // Insert the new value into the sorted array
        if (Compare(value, Samples[1].Value))
            Samples[2] = Samples[1] = sample;
        else if (Compare(value, Samples[2].Value))
            Samples[2] = sample;

        // Expire best if it has been the best for a long time
        if (Samples[0].TimeoutExpired(sample.Timestamp, windowLengthTime))
        {
            // Also expire the next best if needed
            if (Samples[1].TimeoutExpired(sample.Timestamp, windowLengthTime))
            {
                Samples[0] = Samples[2];
                Samples[1] = sample;
            }
            else
            {
                Samples[0] = Samples[1];
                Samples[1] = Samples[2];
            }
            Samples[2] = sample;
            return;
        }

        // Quarter of window has gone by without a better value - Use the second-best
        if (Samples[1].Value == Samples[0].Value &&
            Samples[1].TimeoutExpired(sample.Timestamp, windowLengthTime / 4))
        {
            Samples[2] = Samples[1] = sample;
            return;
        }

        // Half the window has gone by without a better value - Use the third-best one
        if (Samples[2].Value == Samples[1].Value &&
            Samples[2].TimeoutExpired(sample.Timestamp, windowLengthTime / 2))
        {
            Samples[2] = sample;
        }
    }
};

using WindowMinS64 = WindowedMinMaxS64<WindowedMinCompareS64>;
using WindowMaxS64 = WindowedMinMaxS64<WindowedMaxCompareS64>;


//------------------------------------------------------------------------------
// DeviceClockSync

/*
    DeviceClockSync

    This estimates the system time when shutter occurred for a single device.
    These system times can be directly compared between cameras.

    Notes:

    We should use the depth capture time rather than the color capture time,
    because both are defined as the mid-point of exposure, which is fairly fuzzy
    for the color camera since it has a long exposure and may be set differently
    on each camera.  The depth camera on the other hand has a super short
    exposure time and is very close to the sync.

    This class makes the assumption that the minimum time between the device
    timestamp for a capture and the system timestamp we read the capture is
    approximately the same for all cameras.

    From that assumption, we are able to determine the relative offset between
    different device timestamps, which allows us to adjust the device timestamp
    of one camera into the same time domain as another camera allowing them to
    be directly compared.

    Once we can directly compare device timestamps we can perform more accurate
    matching between frames captured from multiple devices.  This allows us to
    operate correctly even when the system is under heavy load.

    Clock drift will affect the system and device clocks over time, so we
    continuously recompute the clock offsets.
*/
class DeviceClockSync
{
public:
    void Reset();

    // Returns sync pulse time in system clock since boot in microseconds.
    // Also updates the synchronization code with a sample.
    // usbread_system_usec = k4a_image_get_system_timestamp_nsec(MasterDepth) / 1000
    // sync_device_usec = k4a_image_get_device_timestamp_usec(MasterDepth) + depth_delay_off_color_usec
    uint64_t CalculateSyncSystemUsec(
        uint64_t usbread_system_usec,
        uint64_t sync_device_usec);

protected:
    // Calculates the minimum difference between system clock time and device time.
    WindowMinS64 MinDeltas;
};


//------------------------------------------------------------------------------
// VideoTimestampCleaner

/*
    This smooths raw timestamps into timestamps suitable for a video player to
    consume, meaning they do not roll backwards and they do not jump around.

    Unavoidable time jumps trigger a discontinuity warning that can be used to
    force a keyframe in the video stream.
*/

class VideoTimestampCleaner
{
public:
    // Clean up the provided timestamp to make the series monotonic.
    // Returns a system timestamp in `shutter_system_usec` time domain.
    // discontinuity: Set to true when the returned time had to jump.
    uint64_t Clean(
        uint64_t shutter_device_usec,
        uint64_t shutter_system_usec,
        bool& discontinuity);

protected:
    uint64_t LastReturnedSystemUsec = 0;
    uint64_t LastDeviceUsec = 0;

    static const int64_t kMaxMismatchUsec = 4000;
};


} // namespace core
