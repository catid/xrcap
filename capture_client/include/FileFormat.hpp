// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

// File format definitions shared between the file reader/writer

#pragma once

#include <cstdint>

namespace core {


//------------------------------------------------------------------------------
// Constants

enum FileChunkType
{
    FileChunk_Calibration = 0,
    FileChunk_Extrinsics  = 1,
    FileChunk_VideoInfo   = 2,
    FileChunk_BatchInfo   = 3,
    FileChunk_Frame       = 4,

    FileChunk_Count
};

const char* FileChunkTypeToString(unsigned chunk_type);

enum ChunkLensType
{
    ChunkLens_Unknown,
    ChunkLens_Theta,
    ChunkLens_Polynomial_3K,
    ChunkLens_Rational_6KT,
    ChunkLens_Brown_Conrady,

    ChunkLens_Count
};

const char* FileChunkLensToString(unsigned chunk_lens);

enum ChunkVideoType
{
    ChunkVideo_Lossless,
    ChunkVideo_H264,
    ChunkVideo_H265,

    ChunkVideo_Count
};

const char* FileChunkVideoToString(unsigned chunk_video);


//------------------------------------------------------------------------------
// Chunks

#pragma pack(push)
#pragma pack(1)

// Header on each chunk
struct FileChunkHeader
{
    uint32_t Length;
    uint32_t Type;
};

static const unsigned kFileChunkHeaderBytes = static_cast<unsigned>( sizeof(FileChunkHeader) );

// Uniquely identifies a camera in a multi-camera rig
struct GuidCameraIndex
{
    uint64_t ServerGuid = 0;
    uint32_t CameraIndex = 0;

    inline GuidCameraIndex()
    {
    }
    inline GuidCameraIndex(uint64_t guid, uint32_t index)
        : ServerGuid(guid)
        , CameraIndex(index)
    {
    }
    inline bool operator==(const GuidCameraIndex& rhs) const
    {
        return ServerGuid == rhs.ServerGuid && CameraIndex == rhs.CameraIndex;
    }
    inline bool operator<(const GuidCameraIndex& rhs) const
    {
        if (ServerGuid < rhs.ServerGuid) {
            return true;
        }
        else if (ServerGuid > rhs.ServerGuid) {
            return false;
        }
        return CameraIndex < rhs.CameraIndex;
    }
};

// Kept in sync with depth_mesh DepthCalibration.hpp CameraIntrinsics
struct ChunkIntrinsics
{
    // Sensor resolution
    int32_t Width, Height;

    // How to interpret the intrinsics (mostly has no effect)
    uint32_t LensModel; // enum ChunkLensType

    // Intrinsics
    float cx, cy;
    float fx, fy;
    float k[6];
    float codx, cody;
    float p1, p2;
};

/*
    Chunk 0: Calibration

    This provides updated intrinsics for each camera.  This is only expected to
    change if capture is restarted during recording.

    Each camera is uniquely identified by a pair of numbers: the server GUID,
    and the camera index.  The GUID is a random number assigned for each capture
    server, and the index is a number starting from 0 and incrementing by one
    for each camera attached to that capture server.  This pair of numbers is
    referenced in other chunks.

    To apply the transform on depth point P to color point Q:

        Q(x,y,z) = P(x, y, z) * R + T
*/

struct ChunkCalibration
{
    GuidCameraIndex CameraGuid;

    ChunkIntrinsics Color, Depth;

    // Extrinsics transform from 3D depth camera point to 3D point relative to color camera
    float RotationFromDepth[3 * 3];
    float TranslationFromDepth[3];
};

/*
    Chunk 1: Extrinsics

    This provides updated extrinsics for each camera.  This is only expected to
    change if recalibration occurs during recording.

    Each camera is uniquely identified by a pair of numbers: the server GUID,
    and the camera index.  The GUID is a random number assigned for each capture
    server, and the index is a number starting from 0 and incrementing by one
    for each camera attached to that capture server.  This pair of numbers is
    referenced in other chunks.

    After applying the intrinsics to generate a 3D point relative to the color
    camera, triangle indices and uv coordinates, this extrinsics transform
    orients the mesh so that meshes from multiple cameras are aligned.

    To apply the transform on mesh point P to color point Q:

        Q(x,y,z) = P(x, y, z) * R + T

    This matrix multiplication is expected to be performed inside the graphics
    shader rather than on the CPU, with the matrix expanded to a 4x4 transform
    and provided as a uniform.
*/

struct ChunkExtrinsics
{
    GuidCameraIndex CameraGuid;

    float Rotation[3 * 3];
    float Translation[3];
};

/*
    Chunk 2: Video Info

    This provides parameters for the color video stream that are needed
    for decoding.

    The other fields thank video type are purely informational and may be
    incorrect.  The source of truth is inside the coded video data itself
    in the VPS, SPS, PPS parameter sets.
*/

struct ChunkVideoInfo
{
    GuidCameraIndex CameraGuid;

    uint32_t VideoType; // enum ChunkVideoType
    uint32_t Width;
    uint32_t Height;
    uint32_t Framerate;
    uint32_t Bitrate;
};

/*
    Chunk 3: Batch Info

    This provides metadata for a batch of camera frames.
    It indicates the start of a new multi-camera mesh for render.
*/

struct ChunkBatchInfo
{
    /*
        The `MaxCameraCount` is the maximum number of frames that will be sent
        as part of the batch.  The actual number of frames may be between zero
        and this number.  For example if a camera failed to return a frame on
        the capture server then it will not be available in the recording.
    */
    uint32_t MaxCameraCount;

    // The `VideoUsec` field is a monotonic microsecond timestamp on the video
    // frame used for the presentation timestamp of the video frame.
    uint64_t VideoUsec;

    // The `VideoEpochUsec` field is the best estimate of the middle of exposure
    // time for all the color camera frames in the batch and is useful for
    // synchronizing the video with other data streams.
    uint64_t VideoEpochUsec;
};

/*
    Chunk 4: Frame

    This provides the compressed color and depth data from a single perspective
    in the multi-camera rig.

    If this is the final frame in the batch then `IsFinalFrame` will be
    non-zero.  This should be used by the application to indicate the complete
    batch has been delivered.

    Intrinsics and extrinsics for the color and depth cameras in this frame can
    be looked up given the `ServerGuid` and `CameraIndex` fields.

    The `ImageBytes` is the number of trailing bytes of `Image` field data.
    It should be interpreted as raw H.264 or H.265 coded video based on the
    `Video Info (Chunk type 2)` `VideoType` field.  The video is not expected to
    be seekable only to keyframes indicated by `Batch Info (Chunk type 3)`
    `BackReference` field = 0.  Other frames must be decoded in sequence
    starting from the keyframes.  There is no MP4 or other container inside
    this field.  Instead it is using Annex B formatted NAL units
    (starting with 00 00 01..).

    The `DepthBytes` is the number of trailing bytes of `Depth` field data.
    Its format is defined by the Zdepth library.  The format itself indicates
    whether it is a lossy or lossless format.

    The remaining fields are all optional metadata.
*/

struct ChunkFrameHeader
{
    uint8_t IsFinalFrame; // 1 = true, 0 = false

    GuidCameraIndex CameraGuid;

    // The `FrameNumber` is a number that increments by one for each frame that
    // is in the recording.
    uint32_t FrameNumber;

    /*
        The `BackReference` is either `0` or `-1` and indicates which prior
        frame is referenced by this frame.  A value of `0` means this is a
        keyframe and can be used as a sync point in the video stream.
        A value of `-1` means this frame depends on receipt of the prior frame.
        The player may decide to go ahead and attempt to decode the frame but
        will either fail to decode, or it will play back with reduced quality.
    */
    int32_t BackReference;

    uint32_t ImageBytes;
    uint32_t DepthBytes;

    float Accelerometer[3];
    uint32_t ExposureUsec;
    uint32_t AutoWhiteBalanceUsec;
    uint32_t ISOSpeed;
    float Brightness;
    float Saturation;

    // Image data here
    // Depth data here
};

#pragma pack(pop)


} // namespace core
