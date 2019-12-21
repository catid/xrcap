// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Xrcap capture client C API for the RGBD Capture Server.
    Built on top of CaptureClient.hpp
*/


//------------------------------------------------------------------------------
// C Boilerplate

#ifndef CAPTURE_CLIENT_H
#define CAPTURE_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(XRCAP_BUILDING)
# if defined(XRCAP_DLL) && defined(_WIN32)
    #define XRCAP_EXPORT __declspec(dllexport)
# else
    #define XRCAP_EXPORT
# endif
#else
# if defined(XRCAP_DLL) && defined(_WIN32)
    #define XRCAP_EXPORT __declspec(dllimport)
# else
    #define XRCAP_EXPORT extern
# endif
#endif


//------------------------------------------------------------------------------
// Constants

#define XRCAP_VERSION 0

#define XRCAP_DIRECT_PORT 28772
#define XRCAP_RENDEZVOUS_PORT 28773

#define XRCAP_PERSPECTIVE_COUNT 8

typedef enum XrcapPlaybackState_t {
    XrcapPlaybackState_Idle       = 0,
    XrcapPlaybackState_LiveStream = 1,
    XrcapPlaybackState_Playing    = 2,
    XrcapPlaybackState_Paused     = 3,
    XrcapPlaybackState_Count
} XrcapPlaybackState;

XRCAP_EXPORT const char* xrcap_playback_state_str(int32_t state);

typedef enum XrcapStreamState_t {
    XrcapStreamState_Idle              = 0,
    XrcapStreamState_Reconnecting      = 1,
    XrcapStreamState_ServerOffline     = 2,
    XrcapStreamState_ServerBusy        = 3,
    XrcapStreamState_Relaying          = 4,
    XrcapStreamState_Authenticating    = 5,
    XrcapStreamState_WrongServerName   = 6,
    XrcapStreamState_IncorrectPassword = 7,
    XrcapStreamState_Live              = 8,
    XrcapStreamState_Count
} XrcapStreamState;

XRCAP_EXPORT const char* xrcap_stream_state_str(int32_t state);

typedef enum XrcapStreamMode_t {
    XrcapStreamMode_Disabled     = 0,
    XrcapStreamMode_Calibration  = 1,
    XrcapStreamMode_CaptureLowQ  = 2,
    XrcapStreamMode_CaptureHighQ = 3,
    XrcapStreamMode_Count
} XrcapStreamMode;

XRCAP_EXPORT const char* xrcap_stream_mode_str(int32_t mode);

typedef enum XrcapCaptureStatus_t {
    XrcapCaptureStatus_Idle                    = 0,
    XrcapCaptureStatus_Initializing            = 1,
    XrcapCaptureStatus_Capturing               = 2,
    XrcapCaptureStatus_NoCameras               = 3,
    XrcapCaptureStatus_BadUsbConnection        = 4,
    XrcapCaptureStatus_FirmwareVersionMismatch = 5,
    XrcapCaptureStatus_SyncCableMisconfigured  = 6,
    XrcapCaptureStatus_Count
} XrcapCaptureStatus;

XRCAP_EXPORT const char* xrcap_capture_status_str(int32_t capture_status);

typedef enum XrcapCameraCodes_t {
    XrcapCameraCodes_Idle         = 0,
    XrcapCameraCodes_Initializing = 1,
    XrcapCameraCodes_StartFailed  = 2,
    XrcapCameraCodes_Capturing    = 3,
    XrcapCameraCodes_ReadFailed   = 4,
    XrcapCameraCodes_SlowWarning  = 5,
    XrcapCameraCodes_Count
} XrcapCameraCodes;

XRCAP_EXPORT const char* xrcap_camera_code_str(int32_t camera_code);

typedef enum XrcapVideo_t {
    XrcapVideo_Lossless = 0,
    XrcapVideo_H264     = 1,
    XrcapVideo_H265     = 2,
    XrcapVideo_Count
} XrcapVideo;

XRCAP_EXPORT const char* xrcap_video_str(int32_t video_code);

// Should be the same as the depth_mesh DepthCalibration.hpp
typedef enum XrcapLensModel_t {
    XrcapLensModel_Unknown       = 0,
    XrcapLensModel_Theta         = 1,
    XrcapLensModel_Polynomial_3K = 2,
    XrcapLensModel_Rational_6KT  = 3,
    XrcapLensModel_Brown_Conrady = 4,
    XrcapLensModel_Count
} XrcapLensModel;

XRCAP_EXPORT const char* xrcap_lens_model_str(int32_t model);


//------------------------------------------------------------------------------
// Compression

// Configures live capture compression
typedef struct XrcapCompression_t {
    // RGB video settings
    uint32_t ColorBitrate; ///< 4000000 = 4 Mbps
    uint8_t ColorQuality; ///< 1-51 (1=best)
    uint8_t ColorVideo; ///< enum XrcapVideo

    // Depth video settings
    uint8_t DepthVideo; ///< enum XrcapVideo

    // Eliminates sensor noise from capturing indoors without studio lighting.
    // 0: Disabled
    // 1..100: Enabled - Recommended: Reduces sensor noise to reduce bandwidth
    uint8_t DenoisePercent;

    // Non-zero: Enable stabilization filter to make static objects more accurate.
    // In Calibration mode: Always enabled, to improve accuracy.
    uint8_t StabilizationFilter;

    // Non-zero: Enable edge filter to remove the outside edge of surfaces
    // This is useful to eliminate depth pixels with less confidence that
    // lead to unsightly seams when combining multiple meshes.
    uint8_t EdgeFilter;

    // Non-zero: Enable filter to remove nearfield objects from the backdrop
    // Recommended for close-ups, should disable for 2 meter+ stand-off
    uint8_t FacePaintingFix;
} XrcapCompression;


//------------------------------------------------------------------------------
// Playback

typedef struct XrcapPlayback_t {
    // Duration of the video in microseconds
    uint64_t VideoDurationUsec;

    // Current playback timestamp in microseconds
    uint64_t VideoTimeUsec;

    // Number of video frames
    uint32_t VideoFrameCount;

    // Current video frame number
    uint32_t VideoFrame;

    // Playback state: enum XrcapPlaybackState
    int32_t State;

    // Current dejitter queue length in milliseconds
    uint32_t DejitterQueueMsec;
} XrcapPlayback;


//------------------------------------------------------------------------------
// Recording

typedef struct XrcapRecording_t {
    // Number of bytes written to file
    uint64_t FileSizeBytes;

    // Accumulated duration of the video in microseconds
    uint64_t VideoDurationUsec;

    // Number of video frames accumulated so far
    uint32_t VideoFrameCount;

    // Is recording file open?
    uint8_t RecordingFileOpen;

    // Is recording paused?
    uint8_t Paused;
} XrcapRecording;


//------------------------------------------------------------------------------
// Streaming Status

// Return of xrcap_get()
typedef struct XrcapStatus_t {
    // XrcapStreamState: Library status.
    int32_t State;

    // FIXME: Report per-server status

    // XrcapStreamMode: App mode from xrcap_set_server_capture_mode().
    int32_t Mode;

    // XrcapCaptureStatus: Status of the capture server.
    int32_t CaptureStatus;

    // Number of cameras attached to the server.
    int32_t CameraCount;

    // XrcapCameraCodes: Status code for each camera on the server.
    int32_t CameraCodes[XRCAP_PERSPECTIVE_COUNT];

    // Bits per second received from server
    uint32_t BitsPerSecond;

    // Loss rate 0..1
    float PacketlossRate;

    // One Way Delay (OWD) from server to client in microseconds
    uint32_t TripUsec;
} XrcapStatus;


//------------------------------------------------------------------------------
// Calibration

typedef struct XrcapCameraIntrinsics_t {
    // Sensor resolution
    int32_t Width, Height;

    // Quick reject for projections on the plane
    //float MaxRadiusForProjection;

    // How to interpret the intrinsics (mostly has no effect)
    uint32_t LensModel;

    // Intrinsics
    float cx, cy;
    float fx, fy;
    float k1, k2, k3, k4, k5, k6;
    float codx, cody;
    float p1, p2;
} XrcapCameraIntrinsics;

typedef struct XrcapCameraCalibration_t {
    // Intrinsics for each camera
    XrcapCameraIntrinsics Color, Depth;

    // Extrinsics
    float RotationFromDepth[3*3];
    float TranslationFromDepth[3];
} XrcapCameraCalibration;


//------------------------------------------------------------------------------
// Perspective

// Transform for a camera into a common reference frame
typedef struct XrcapExtrinsics_t {
    int32_t IsIdentity;
    // Stored row-first in memory
    float Transform[16];
} XrcapExtrinsics;

// Perspective includes a texture to render and a mesh description
typedef struct XrcapPerspective_t {
    // Check this first.  If Valid = 0, then do not render.
    int32_t Valid;

    // Image format is NV12, which is two channels.
    // Size of image and Y channel
    int32_t Width, Height;

    // Width * Height bytes in length
    uint8_t* Y;

    // Size of U, V channels
    int32_t ChromaWidth, ChromaHeight;

    // ChromaWidth * ChromaHeight * 2 bytes in length
    uint8_t* UV;

    // Number of indices (multiple of 3) for triangles to render
    uint32_t IndicesCount;
    uint32_t* Indices;

#define XRCAP_FLOAT_STRIDE 5

    // Vertices for mesh represented as repeated: x,y,z,u,v
    uint32_t FloatsCount;
    float* XyzuvVertices;

    // Transform for how the mesh is oriented in the scene (Model matrix)
    XrcapExtrinsics* Extrinsics;

    // Accelerometer reading for extrinsics calibration
    float Accelerometer[3];

    // Pointer to calibration data
    XrcapCameraCalibration* Calibration;

    // Information needed for setting extrinsics
    uint64_t Guid;
    uint32_t CameraIndex;

    // AWB and exposure settings for this frame
    uint32_t AutoWhiteBalanceUsec;
    uint32_t ExposureUsec;
    uint32_t ISOSpeed;

    // ProcAmp color enhancements for this frame
    float Brightness;
    float Saturation;
} XrcapPerspective;


//------------------------------------------------------------------------------
// Frame

// Return of xrcap_get()
typedef struct XrcapFrame_t {
    // Check this first.  If Valid = 0, then do not render.
    int32_t Valid;

    // Time since video start in microseconds, guaranteed monotonic.
    // Used to seek through the file.
    uint64_t VideoStartUsec;

    // Number for this frame.
    // Increments once for each frame to display.
    int32_t FrameNumber;

    // Exposure time in microseconds since the UNIX epoch, which may jump around
    // and be less suitable for use for video timestamps.
    uint64_t ExposureEpochUsec;

    // Perspectives to render.
    XrcapPerspective Perspectives[XRCAP_PERSPECTIVE_COUNT];
} XrcapFrame;


//------------------------------------------------------------------------------
// Frame API

// This should be called every frame.
// Fills the frame with the current state.
// The structure indicates if a mesh can be rendered this frame or if there is
// an error status.
// Calling this invalidates data from the previous frame.
XRCAP_EXPORT void xrcap_get(XrcapFrame* frame, XrcapStatus* status);

/*
    dejitter_queue_msec:

        Length of the queue used to dejitter frames arriving from multiple
        capture servers.  This also helps to smooth playback in case the frames
        arrive in bursts instead of at a regular pace.

        For use on a LAN, this can be configured low perhaps 100 milliseconds,
        which is helpful to allow for audio sync.
        For Internet streaming, this should be much larger, perhaps 500 msec.
*/
XRCAP_EXPORT void xrcap_playback_settings(uint32_t dejitter_queue_msec);

// Blocks until shutdown is complete.
XRCAP_EXPORT void xrcap_shutdown();


//------------------------------------------------------------------------------
// Streaming API

/*
    All functions are safe to call repeatedly each frame and will not slow down
    rendering of the application.

    Call xrcap_frame() to receive the current frame to render, which may be the
    same frame as the previous call.  Check `FrameNumber` to see if it changed.

    To start or stop capture remotely use xrcap_set_server_capture_mode().
*/

// Connect to rendezvous or capture server (direct).
XRCAP_EXPORT void xrcap_connect(
    const char* server_address,
    int32_t server_port,
    const char* session_name,
    const char* password
);

// Set capture server mode.
XRCAP_EXPORT void xrcap_set_server_capture_mode(int32_t mode);


//------------------------------------------------------------------------------
// Playback API

/*
    This clears all playback state and disconnects from any capture servers.

    This MUST be called before a new playback begins or it may crash.

    This does not invalidate the last pinned frame from xrcap_get().
*/
XRCAP_EXPORT void xrcap_reset();

/*
    Modify playback.

    pause: Nonzero=Playback will stop and playback queue will keep building.

    To start the file paused, set pause=1 before reading the playback file.
    Check xrcap_get_playback_state() to see if VideoFrameCount > 10 to see
    if playback is ready.

    repeat: When playback ends, it restarts from the front.
*/
XRCAP_EXPORT void xrcap_playback_tricks(uint32_t pause, uint32_t loop_repeat);

/*
    Reads file asynchronously in the background and starts streaming once ready.

    Return 0 if file could not be opened.  Otherwise returns non-zero.
*/
XRCAP_EXPORT int32_t xrcap_playback_read_file(const char* file_path);

/*
    Provide more video data to the 3D video player.
    Frames are delivered via xrcap_get() that is also used for streaming from
    the capture servers.

    If playback has not started yet, this begins playback and disconnects from
    any capture servers.

    This keeps all video since the last reset in memory, enabling repeat
    or random access seeks.

    Pass nullptr and zero bytes to indicate the end of the video.
*/
XRCAP_EXPORT void xrcap_playback_append(const void* data, uint32_t bytes);

// Gets the current playback state
XRCAP_EXPORT void xrcap_get_playback_state(XrcapPlayback* playback_state);

// Seek to specified video timestamp
XRCAP_EXPORT void xrcap_playback_seek(uint64_t video_usec);


//------------------------------------------------------------------------------
// Recording API

/*
    Start/stop recording.

    Provide a file path to start recording, initially paused.
    Will stop recording if it is currently recording.

    Pass nullptr to stop recording.

    Return 0 if file could not be opened.  Otherwise returns non-zero.
*/
XRCAP_EXPORT int32_t xrcap_record(const char* file_path);

/*
    Pass 1 to pause recording, and 0 to resume recording.
    This is required to start recording initially.
*/
XRCAP_EXPORT void xrcap_record_pause(uint32_t pause);

/*
    Returns the current state of recording.
*/
XRCAP_EXPORT void xrcap_record_state(XrcapRecording* recording_state);


//------------------------------------------------------------------------------
// Lighting Calibration

XRCAP_EXPORT void xrcap_set_lighting(
    uint64_t guid,
    uint32_t camera_index,
    float brightness,
    float saturation);


//------------------------------------------------------------------------------
// Auto Exposure

XRCAP_EXPORT void xrcap_set_exposure(
    int32_t auto_enabled,
    uint32_t exposure_usec, // Note: Camera only supports a few exposure values
    uint32_t awb_usec); // Note: Camera only supports a range of AWB values


//------------------------------------------------------------------------------
// Clip Region

XRCAP_EXPORT void xrcap_set_clip(
    int32_t enabled,
    float radius_meters,
    float floor_meters,
    float ceiling_meters);


//------------------------------------------------------------------------------
// Extrinsics

// Set extrinsics given Guid, CameraIndex from XrcapPerspective
XRCAP_EXPORT void xrcap_set_extrinsics(
    uint64_t guid,
    uint32_t camera_index,
    const XrcapExtrinsics* extrinsics);


//------------------------------------------------------------------------------
// Compression

XRCAP_EXPORT void xrcap_set_compression(
    const XrcapCompression* compression);


//------------------------------------------------------------------------------
// C Boilerplate

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CAPTURE_CLIENT_H
