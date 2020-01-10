// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Capture Protocol

    Capture Client <-> Rendezvous Server <-> Capture Server

    The servers are password protected to control access to the data.
    The video data is encrypted to avoid interception.

    We use SPAKE2+EE to key the encryption.
*/

#pragma once

#include <cstdint>
#include <string>
#include <array>

#include <DepthCalibration.hpp> // depth_mesh
#include <DepthMesh.hpp> // depth_mesh
//#include <tonk.h> // tonk

namespace protos {


//------------------------------------------------------------------------------
// Constants

// Bytes per second max
static const int kBandwidthLimitBPS = 16 * 1000 * 1000;

// Port used for capture server
static const uint16_t kCaptureServerPort = 28772;

// Port used for capture rendezvous server
static const uint16_t kRendezvousServerPort = 28773;

// Maximum cameras per capture PC
static const unsigned kMaxCameras = 8;

// Tonk Reliable In-Order Channel for rendezvous
static const uint32_t kChannelRendezvous = 50; //TonkChannel_Reliable0;

// Tonk Reliable In-Order Channel for authentication
static const uint32_t kChannelAuthentication = 51; //TonkChannel_Reliable1;

// Tonk Reliable In-Order Channel for controls
static const uint32_t kChannelControl = 52; //TonkChannel_Reliable2;

// Tonk Reliable In-Order Channel for video imagery
static const uint32_t kChannelImage = 53; //TonkChannel_Reliable3;

// Tonk Reliable In-Order Channel for video depth
static const uint32_t kChannelDepth = 54; //TonkChannel_Reliable4;

#define AUTH_CLIENT_STRING "client"
#define AUTH_SERVER_STRING "server"

// CaptureManager modes
enum Modes
{
    Mode_Disabled,
    Mode_Calibration,
    Mode_CaptureLowQual,
    Mode_CaptureHighQual,

    Mode_Count
};

// Overall status codes
enum StatusCodes
{
    StatusCode_Idle,
    StatusCode_Initializing,
    StatusCode_Capturing,
    StatusCode_NoCameras,
    StatusCode_BadUsbConnection,
    StatusCode_FirmwareVersionMismatch,
    StatusCode_SyncCableMisconfigured,

    StatusCode_Count
};

// Camera status codes
enum CameraCodes
{
    CameraCode_Idle,
    CameraCode_Initializing,
    CameraCode_StartFailed,
    CameraCode_Capturing,
    CameraCode_ReadFailed,
    CameraCode_SlowWarning,

    CameraCode_Count
};

// Supported video types
enum VideoTypes
{
    VideoType_Lossless, ///< Used for depth compression only
    VideoType_H264,
    VideoType_H265,

    VideoType_Count
};

// Connect result
enum ConnectResult
{
    ConnectResult_NotFound,
    ConnectResult_NotReady,
    ConnectResult_Connecting,
    ConnectResult_Direct, // Already on the right server
    ConnectResult_WrongName, // Server name does not match

    ConnectResult_Count
};

// Result of password authentication
enum AuthResult
{
    AuthResult_Deny,
    AuthResult_Accept,

    AuthResult_Count
};


//------------------------------------------------------------------------------
// Message Types

// Message types
enum MessageTypes
{
    // Sets the name of the capture server
    MessageType_RegisterCaptureServer,

    // Each capture server requests a number of TDMA ranges for depth capture
    // from the rendezvous server, and the rendezvous server doles these out
    // without authentication.
    MessageType_RequestTDMA,
    MessageType_AssignTDMA,

    // Connect to capture server (as a viewer client) by name
    MessageType_ConnectName,

    // Result of trying to connect
    MessageType_ConnectResult,

    // Authentication handshake
    MessageType_AuthServerHello, // Provides PublicData
    MessageType_AuthClientReply, // response1
    MessageType_AuthServerProof, // response2
    MessageType_AuthClientProof, // response3
    MessageType_AuthResult,

    // Configure the server from the viewer
    MessageType_RequestKeyframe, // No data associated with this type
    MessageType_SetMode,
    MessageType_SetExposure,
    MessageType_SetClip,
    MessageType_SetCompression,
    MessageType_SetLighting,

    // Status update about overall camera capture
    MessageType_Status,

    // Calibration data for cameras
    MessageType_Calibration,

    // Update for camera extrinsics
    MessageType_Extrinsics,

    // Video info
    MessageType_VideoInfo,

    // Video frames
    MessageType_BatchInfo,
    MessageType_FrameHeader,

    MessageType_Count
};

#pragma pack(push)
#pragma pack(1)

static const unsigned kCaptureServerNameMax = 256;

struct MessageRegisterCaptureServer
{
    uint8_t Type = static_cast<uint8_t>( MessageType_RegisterCaptureServer );

    char Name[kCaptureServerNameMax];
    uint64_t Guid;
};

struct MessageRequestTDMA
{
    uint8_t Type = static_cast<uint8_t>( MessageType_RequestTDMA );

    uint8_t CameraCount;
};

struct MessageAssignTDMA
{
    uint8_t Type = static_cast<uint8_t>( MessageType_AssignTDMA );

    uint8_t CameraCount; // Number of camera offsets
    // Followed by an array of int16_t offsets such as 0, 1, -1, etc..
    // This can be multiplied by the camera depth shutter time to arrive at TDMA slots.
};

struct MessageConnectName
{
    uint8_t Type = static_cast<uint8_t>( MessageType_ConnectName );

    char Name[kCaptureServerNameMax];
    uint16_t IgnoreGuidCount; // Number of Guids that follow this message (maybe 0)
    // Following is a set of Guids that should be ignored and not initiate a new connection.
    // This is used to allow the rendezvous server to connect clients to new servers that
    // come online.
};

struct MessageConnectResult
{
    uint8_t Type = static_cast<uint8_t>( MessageType_ConnectResult );

    uint8_t Result; // enum ConnectResult
    uint16_t ServerCount; // Number of servers found
    uint64_t ServerGuid;
};

static const unsigned kPublicDataBytes = 36; // crypto_spake_PUBLICDATABYTES
static const unsigned kResponse1Bytes = 32; // crypto_spake_RESPONSE1BYTES
static const unsigned kResponse2Bytes = 64; // crypto_spake_RESPONSE2BYTES
static const unsigned kResponse3Bytes = 32; // crypto_spake_RESPONSE3BYTES

struct MessageAuthServerHello
{
    uint8_t Type = static_cast<uint8_t>( MessageType_AuthServerHello );

    uint8_t PublicData[kPublicDataBytes];
};

struct MessageAuthClientReply
{
    uint8_t Type = static_cast<uint8_t>( MessageType_AuthClientReply );

    uint8_t Response1[kResponse1Bytes];
};

struct MessageAuthServerProof
{
    uint8_t Type = static_cast<uint8_t>( MessageType_AuthServerProof );

    uint8_t Response2[kResponse2Bytes];
};

struct MessageAuthClientProof
{
    uint8_t Type = static_cast<uint8_t>( MessageType_AuthClientProof );

    uint8_t Response3[kResponse3Bytes];
};

struct MessageAuthResult
{
    uint8_t Type = static_cast<uint8_t>( MessageType_AuthResult );

    uint8_t Result; // enum AuthResult
};

struct MessageStatus
{
    uint8_t Type = static_cast<uint8_t>( MessageType_Status );

    uint8_t Mode; // enum Modes
    uint8_t CaptureStatus; // enum StatusCodes
    uint32_t CameraCount;
    uint8_t CameraStatus[kMaxCameras];
};

struct MessageSetMode
{
    uint8_t Type = static_cast<uint8_t>( MessageType_SetMode );

    uint8_t Mode; // enum Modes
};

struct MessageSetExposure
{
    uint8_t Type = static_cast<uint8_t>( MessageType_SetExposure );

    // Non-zero: Auto-Exposure Enabled
    int32_t AutoEnabled = 1;
    uint32_t ExposureUsec = 0;
    uint32_t AutoWhiteBalanceUsec = 0;
};

struct MessageSetClip
{
    uint8_t Type = static_cast<uint8_t>( MessageType_SetClip );

    // Non-zero: Enabled
    int32_t Enabled = 0;

    float ClipRadiusMeters = 1.5f;
    float ClipFloorMeters = -0.5f;
    float ClipCeilingMeters = 2.2f;
};

struct MessageSetLighting
{
    uint8_t Type = static_cast<uint8_t>( MessageType_SetLighting );

    int32_t CameraIndex = -1;
    float Brightness = 0.f; // -100.0 to +100.0 (default 0)
    float Saturation = 1.f; // 0.0 to 10.0 (default 1)
};

// Keep in sync with struct XrcapCompression
// These settings are applied on the capture server side.
struct CompressionSettings
{
    // RGB video settings
    uint32_t ColorBitrate = 4000000; ///< 4 Mbps
    uint8_t ColorQuality = 25; ///< 1-51 (1=best)
    uint8_t ColorVideo = static_cast<uint8_t>( VideoTypes::VideoType_H264 ); ///< enum VideoTypes

    // Depth video settings
    // In Calibration mode: Always Lossless.
    uint8_t DepthVideo = static_cast<uint8_t>( VideoTypes::VideoType_Lossless ); ///< enum VideoTypes

    // Eliminates sensor noise from capturing indoors without studio lighting.
    // 0: Disabled
    // 1..100: Enabled
    uint8_t DenoisePercent = 100;

    // Non-zero: Enable stabilization filter to make static objects more accurate.
    // In Calibration mode: Always enabled, to improve accuracy.
    uint8_t StabilizationFilter = 1;

    // Non-zero: Enable edge filter to remove the outside edge of surfaces
    // This is useful to eliminate depth pixels with less confidence that
    // lead to unsightly seams when combining multiple meshes.
    uint8_t EdgeFilter = 1;

    // Non-zero: Enable filter to remove nearfield objects from the backdrop
    // Recommended for close-ups, should disable for 2 meter+ stand-off
    uint8_t FacePaintingFix = 0;
};

struct MessageSetCompression
{
    uint8_t Type = static_cast<uint8_t>( MessageType_SetCompression );

    CompressionSettings Settings{};
};

struct MessageCalibration
{
    uint8_t Type = static_cast<uint8_t>( MessageType_Calibration );

    uint32_t CameraIndex;
    core::CameraCalibration Calibration;
};

struct CameraExtrinsics
{
    // Must be identical to XrcapExtrinsics
    // Extrinsics that transform from a secondary camera to the primary one
    int32_t IsIdentity = 1;

    std::array<float, 16> Transform;

    bool operator==(const CameraExtrinsics& rhs) const;
    inline bool operator!=(const CameraExtrinsics& rhs) const
    {
        return !(*this == rhs);
    }
};

struct MessageExtrinsics
{
    uint8_t Type = static_cast<uint8_t>( MessageType_Extrinsics );

    uint32_t CameraIndex;
    CameraExtrinsics Extrinsics;
};

struct MessageVideoInfo
{
    uint8_t Type = static_cast<uint8_t>( MessageType_VideoInfo );

    // VideoTypes: H.264/H.265?
    uint8_t VideoType;

    // Video parameters.  These can be extracted from the coded video but
    // it is a lot more convenient to transmit them separately.
    uint32_t Width, Height, Framerate, Bitrate;

    bool operator==(const MessageVideoInfo& other)
    {
        return VideoType == other.VideoType &&
            Width == other.Width &&
            Height == other.Height &&
            Framerate == other.Framerate &&
            Bitrate == other.Bitrate;
    }
    bool operator!=(const MessageVideoInfo& other)
    {
        return !(*this == other);
    }
};

struct MessageBatchInfo
{
    uint8_t Type = static_cast<uint8_t>( MessageType_BatchInfo );

    // Maximum number of camera frames that might be sent
    uint32_t CameraCount;

    // Shutter time in microseconds since system boot of server.
    // This can be converted to local time using tonk_from_remote_time().
    uint64_t VideoBootUsec;
};

struct MessageFrameHeader
{
    uint8_t Type = static_cast<uint8_t>( MessageType_FrameHeader );

    // Unique incrementing number for each frame
    uint32_t FrameNumber;

    // Set to 0 if this is an I-frame.
    // Otherwise set to -1 to indicate the previous frame is referenced.
    // The client should keep track of the received frames to facilitate
    // temporal SVC where some frames are not transmitted in the video.
    int32_t BackReference;

    // Non-zero: This is the last frame in the batch
    uint8_t IsFinalFrame;

    // Index for the camera
    uint32_t CameraIndex;

    // Accelerometer reading for calibration
    float Accelerometer[3];

    // Image bytes for this frame
    uint32_t ImageBytes;

    // Depth bytes for this frame
    uint32_t DepthBytes;

    // Metadata
    uint32_t ExposureUsec = 0;
    uint32_t AutoWhiteBalanceUsec = 0;
    uint32_t ISOSpeed = 0;
    float Brightness = 0.f;
    float Saturation = 1.f;
};

#pragma pack(pop)


//------------------------------------------------------------------------------
// Tools

// Sanitizes a buffer containing a string that may or may not be null-terminated
std::string SanitizeString(const char* buffer, size_t bytes);


} // namespace protos
