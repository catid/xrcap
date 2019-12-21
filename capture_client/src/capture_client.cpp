// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

//------------------------------------------------------------------------------
// C Boilerplate

#include "capture_client.h"
#include "CaptureClient.hpp"

#include <mutex>

#ifdef __cplusplus
extern "C" {
#endif

static core::CaptureClient m_Client;


//------------------------------------------------------------------------------
// Constants

// XrcapStreamState
XRCAP_EXPORT const char* xrcap_stream_state_str(int32_t state)
{
    static_assert(XrcapStreamState_Count == 9, "Update this");
    switch (state)
    {
    case XrcapStreamState_Idle: return "Idle";
    case XrcapStreamState_Reconnecting: return "Reconnecting";
    case XrcapStreamState_ServerOffline: return "Server Offline";
    case XrcapStreamState_ServerBusy: return "Server Busy";
    case XrcapStreamState_Relaying: return "Relaying";
    case XrcapStreamState_Authenticating: return "Authenticating";
    case XrcapStreamState_WrongServerName: return "Wrong Server Name";
    case XrcapStreamState_IncorrectPassword: return "Incorrect Password";
    case XrcapStreamState_Live: return "Live";
    default: break;
    }
    return "(Invalid XrcapStreamState)";
}

// XrcapPlaybackState
XRCAP_EXPORT const char* xrcap_playback_state_str(int32_t state)
{
    static_assert(XrcapPlaybackState_Count == 4, "Update this");
    switch (state)
    {
    case XrcapPlaybackState_Idle: return "Idle";
    case XrcapPlaybackState_LiveStream: return "Live-Stream";
    case XrcapPlaybackState_Playing: return "Playing";
    case XrcapPlaybackState_Paused: return "Paused";
    default: break;
    }
    return "(Invalid XrcapPlaybackState)";
}

// XrcapStreamMode
XRCAP_EXPORT const char* xrcap_stream_mode_str(int32_t mode)
{
    static_assert(XrcapStreamMode_Count == 4, "Update this");
    static_assert(protos::Mode_Count == 4, "Update this");
    switch (mode)
    {
    case XrcapStreamMode_Disabled: return "Disabled";
    case XrcapStreamMode_Calibration: return "Calibration";
    case XrcapStreamMode_CaptureLowQ: return "Capture(Low Quality)";
    case XrcapStreamMode_CaptureHighQ: return "Capture(High Quality)";
    default: break;
    }
    return "(Invalid XrcapStreamMode)";
}

// XrcapCaptureStatus
XRCAP_EXPORT const char* xrcap_capture_status_str(int32_t capture_status)
{
    static_assert(XrcapCaptureStatus_Count == 7, "Update this");
    static_assert(protos::StatusCode_Count == 7, "Update this");
    switch (capture_status)
    {
    case XrcapCaptureStatus_Idle: return "Idle";
    case XrcapCaptureStatus_Initializing: return "Initializing";
    case XrcapCaptureStatus_Capturing: return "Capturing";
    case XrcapCaptureStatus_NoCameras: return "No Cameras";
    case XrcapCaptureStatus_BadUsbConnection: return "Bad USB Connection";
    case XrcapCaptureStatus_FirmwareVersionMismatch: return "Firmware Version Mismatch";
    case XrcapCaptureStatus_SyncCableMisconfigured: return "Sync Cable Misconfigured";
    default: break;
    }
    return "(Invalid XrcapCaptureStatus)";
}

// XrcapCameraCodes
XRCAP_EXPORT const char* xrcap_camera_code_str(int32_t camera_code)
{
    static_assert(XrcapCameraCodes_Count == 6, "Update this");
    static_assert(protos::CameraCode_Count == 6, "Update this");
    switch (camera_code)
    {
    case XrcapCameraCodes_Idle: return "Idle";
    case XrcapCameraCodes_Initializing: return "Initializing";
    case XrcapCameraCodes_StartFailed: return "Start failed";
    case XrcapCameraCodes_Capturing: return "Capturing";
    case XrcapCameraCodes_ReadFailed: return "Read failed";
    case XrcapCameraCodes_SlowWarning: return "Slow Warning";
    default: break;
    }
    return "(Invalid XrcapCameraCodes)";
}

// XrcapVideo
XRCAP_EXPORT const char* xrcap_video_str(int32_t video_code)
{
    static_assert(XrcapVideo_Count == 3, "Update this");
    switch (video_code)
    {
    case XrcapVideo_Lossless: return "Lossless";
    case XrcapVideo_H264: return "H.264";
    case XrcapVideo_H265: return "H.265";
    default: break;
    }
    return "(Invalid XrcapVideo)";
}

// XrcapLensModel
XRCAP_EXPORT const char* xrcap_lens_model_str(int32_t model)
{
    static_assert(XrcapLensModel_Count == 5, "Update this");
    switch (model)
    {
    case XrcapLensModel_Unknown: return "Unknown";
    case XrcapLensModel_Theta: return "Theta";
    case XrcapLensModel_Polynomial_3K: return "Polynomial 3K";
    case XrcapLensModel_Rational_6KT: return "Rational 6KT";
    case XrcapLensModel_Brown_Conrady: return "Brown Conrady";
    default: break;
    }
    return "(Invalid XrcapLensModel)";
}



//------------------------------------------------------------------------------
// API

XRCAP_EXPORT void xrcap_connect(
    const char* server_address,
    int32_t server_port,
    const char* session_name,
    const char* password
)
{
    m_Client.Connect(server_address, server_port, session_name, password);
}

XRCAP_EXPORT void xrcap_get(XrcapFrame* frame, XrcapStatus* status)
{
    m_Client.Get(frame, status);
}

XRCAP_EXPORT void xrcap_set_server_capture_mode(int32_t mode)
{
    m_Client.SetServerCaptureMode(mode);
}

XRCAP_EXPORT void xrcap_shutdown()
{
    m_Client.Shutdown();
}

XRCAP_EXPORT void xrcap_playback_settings(
    uint32_t dejitter_queue_msec)
{
    m_Client.PlaybackSettings(dejitter_queue_msec);
}

XRCAP_EXPORT void xrcap_set_lighting(
    uint64_t guid,
    uint32_t camera_index,
    float brightness,
    float saturation)
{
    m_Client.SetLighting(
        guid,
        camera_index,
        brightness,
        saturation);
}

XRCAP_EXPORT void xrcap_set_exposure(
    int32_t auto_enabled,
    uint32_t exposure_usec,
    uint32_t awb_usec)
{
    m_Client.SetExposure(
        auto_enabled,
        exposure_usec,
        awb_usec);
}

XRCAP_EXPORT void xrcap_set_clip(
    int32_t enabled,
    float radius_meters,
    float floor_meters,
    float ceiling_meters)
{
    m_Client.SetClip(
        enabled,
        radius_meters,
        floor_meters,
        ceiling_meters);
}

XRCAP_EXPORT void xrcap_set_extrinsics(
    uint64_t guid,
    uint32_t camera_index,
    const XrcapExtrinsics* extrinsics)
{
    if (!extrinsics) {
        return;
    }
    const protos::CameraExtrinsics* protos_extrinsics =
        reinterpret_cast<const protos::CameraExtrinsics*>( extrinsics );

    m_Client.SetExtrinsics(guid, camera_index, *protos_extrinsics);
}

XRCAP_EXPORT void xrcap_set_compression(
    const XrcapCompression* compression)
{
    if (!compression) {
        return;
    }
    const protos::CompressionSettings* protos_compression =
        reinterpret_cast<const protos::CompressionSettings*>( compression );

    m_Client.SetCompression(*protos_compression);
}

XRCAP_EXPORT void xrcap_reset()
{
    m_Client.Reset();
}

XRCAP_EXPORT void xrcap_playback_tricks(uint32_t pause, uint32_t loop_repeat)
{
    m_Client.PlaybackTricks(pause != 0, loop_repeat != 0);
}

XRCAP_EXPORT int32_t xrcap_playback_read_file(const char* file_path)
{
    return m_Client.PlaybackReadFile(file_path) ? 1 : 0;
}

XRCAP_EXPORT void xrcap_playback_append(const void* data, uint32_t bytes)
{
    m_Client.PlaybackAppend(data, bytes);
}

XRCAP_EXPORT void xrcap_get_playback_state(XrcapPlayback* playback_state)
{
    if (!playback_state) {
        return;
    }

    m_Client.GetPlaybackState(*playback_state);
}

XRCAP_EXPORT void xrcap_playback_seek(uint64_t video_usec)
{
    m_Client.PlaybackSeek(video_usec);
}

XRCAP_EXPORT int32_t xrcap_record(const char* file_path)
{
    return m_Client.Record(file_path) ? 1 : 0;
}

XRCAP_EXPORT void xrcap_record_pause(uint32_t pause)
{
    m_Client.RecordPause(pause != 0);
}

XRCAP_EXPORT void xrcap_record_state(XrcapRecording* recording_state)
{
    if (!recording_state) {
        return;
    }

    m_Client.GetRecordingState(*recording_state);
}



//------------------------------------------------------------------------------
// C Boilerplate

#ifdef __cplusplus
} // extern "C"
#endif
