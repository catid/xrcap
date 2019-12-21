// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Stand-alone library that wraps different methods to encode/decode video.
    Meant to be tweaked and modified for each application, not as a fully
    general video codec library.

    On Intel Windows the best way is to use Nvidia's CUDA nvcuvid library,
    and maybe MediaFoundation for Intel Quick Sync Video (QSV).
    On Intel Linux the best way is to use ffmpeg's vaapi plugin for QSV.
    On Android/iOS there are OS-specific APIs around some pretty unreliable hw.
    Other platforms mostly use V4L2.

    Currently only CUDA is implemented, but it is designed to make it
    easier to add more hardware-accelerated backends.

    Note that most hardware encoders are limited to one/two sessions at a time,
    so it is often not desired to make more than one encoder instance.
    NVDEC does not have this artificial limitation and you are able to decode as
    many videos in parallel as you like.
*/

/*
    Why not use NvPipe?
    https://github.com/NVIDIA/NvPipe

    While NvPipe does seem to support 16-bit monochrome data, the manner
    in which it does this is not recommended: The high and low bytes are
    split into halves of the Y channel of an image, doubling the resolution.
    So the video encoder runs twice as slow.  Single bit errors in the Y channel
    are then magnified in the resulting decoded values by 256x, which is not
    acceptable for depth data because this is basically unusable.

    Other features of NvPipe are not useful for depth compression, and it
    abstracts away the more powerful nvcuvid API that allows applications to
    dispatch multiple encodes in parallel in a scatter-gather pattern, and to
    tune the encoder parameters like intra-refresh, AQ, and so on.
*/

#pragma once

#include "Cuda.hpp"

// Nvidia NVENC/NVDEC uses the attached GPU for efficient encoding via CUDA
#include <NvEncoder.h>
#include <NvEncoderCuda.h>
#include <NvDecoder.h>
#include <nppcore.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace nvcuvid {


//------------------------------------------------------------------------------
// Constants

enum class VideoType
{
    // NVENC only supports these two
    H264,
    H265,
};

enum class DecodeMode
{
    // Only passing in VPS,SPS,PPS so we do not expect an image back
    IgnoreOutput,

    // Only return the Y channel of the resulting image
    MonochromeOnly,

    // Return YUV420 multi-planar image contiguous in memory
    YUV420,
};


//------------------------------------------------------------------------------
// Video Parameters

struct VideoParameters
{
    // Use intra-refresh when keyframes are not in use?
    // Set to false if you are manually injecting keyframes,
    // and set to true if you want the system to automatically do this
    // in a more efficient way.
    bool UseIntraRefresh = false;

    // Using H265 instead here leads to files with half the error that are about
    // 25% smaller.  But the encoder/decoder is not always available.
    VideoType Type = VideoType::H264;

    // Depth image resolution
    int Width = 0, Height = 0;

    // Frames per second of camera
    int Fps = 30;

    // Constant bitrate selected
    int Bitrate = 2000000;
};


//------------------------------------------------------------------------------
// Video Input Image

// YUV 4:2:0 format required.
// U,V can be set to null
struct VideoInputImage
{
    bool IsKeyframe = false;
    bool IsDevicePtr = false;

    void* Y = nullptr;
    int Width = 0;
    int Stride = 0;
    int Height = 0;

    void* U = nullptr;
    void* V = nullptr;
    int ChromaWidth = 0;
    int ChromaStride = 0;
    int ChromaHeight = 0;
};


//------------------------------------------------------------------------------
// Video Decode Input

struct VideoDecodeInput
{
    // Mode to operate in
    DecodeMode Mode = DecodeMode::IgnoreOutput;

    // Resolution of the Y channel of the image we expect to decode
    int Width = 0, Height = 0;

    // Type of video decode
    VideoType Type = VideoType::H264;

    // Source data
    const uint8_t* Data = nullptr;
    int Bytes = 0;
};


//------------------------------------------------------------------------------
// Video Codec

class VideoCodec
{
public:
    // This clears vPacket before filling it
    bool EncodeBegin(
        const VideoParameters& params,
        const VideoInputImage& image,
        std::vector<std::vector<uint8_t>>& vPacket);

    // This clears vPacket before filling it
    bool EncodeFinish(
        std::vector<std::vector<uint8_t>>& vPacket);

    bool Decode(
        const VideoDecodeInput& input,
        std::vector<uint8_t>& decoded);

protected:
    VideoParameters Params{};

    // Shared state
    uint64_t NextTimestamp = 0;

    // CUDA NVENC/NVDEC
    GUID CodecGuid;
    bool CudaNonfunctional = false;
    CudaContext Context;
    CUstream NvStream = nullptr; // Note this is the same as cudaStream_t
    NppStreamContext nppStreamContext{};
    std::shared_ptr<NvEncoderCuda> CudaEncoder;
    std::shared_ptr<NvDecoder> CudaDecoder;


    bool EncodeBeginNvenc(
        const VideoInputImage& image,
        std::vector<std::vector<uint8_t>>& vPacket);
    bool EncodeFinishNvenc(
        std::vector<std::vector<uint8_t>>& vPacket);
    bool DecodeNvdec(
        const VideoDecodeInput& input,
        std::vector<uint8_t>& decoded);
    void CleanupCuda();

    bool CreateEncoder();

    // This does any format conversion as required
    bool CopyImageToFrame(const VideoInputImage& image, const NvEncInputFrame* frame);
};


} // namespace nvcuvid
