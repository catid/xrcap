// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    JPEG decoder

    Based on nvJPEG for high performance for multiple camera capture.
    Also tried: turbojpeg, NVDEC, ffmpeg.  All other options seem to be several
    times slower than nvJPEG so we do not implement those.
*/

#pragma once

#include "Cuda.hpp"

#include <nvjpeg.h>
#include <tbb/tbb.h>

#include <memory>
#include <atomic>

namespace nvcuvid {


//------------------------------------------------------------------------------
// Tools

const char* NvJpegStatusToString(nvjpegStatus_t status);


//------------------------------------------------------------------------------
// JpegResultBuffer

struct ResultChannel
{
    // Size of the memory region
    int Size = 0;

    // This pointer is valid on GPU and on CPU after sync
    void* ManagedPtr = nullptr;

    // Host copy of the data
    std::vector<uint8_t> HostData;

    // These are filled in by the JpegDecoder code
    int Width = 0;
    int Height = 0;
    int Stride = 0;
};

struct JpegResultBuffer
{
    // Automatically frees memory
    ~JpegResultBuffer();

    int ChannelCount = 0;
    ResultChannel Channels[NVJPEG_MAX_COMPONENT];
    unsigned TotalSize = 0;
    cudaStream_t NvStream = nullptr;
    bool AvailableOnCpu = false;
};


//------------------------------------------------------------------------------
// JpegBufferAllocator

class JpegBufferAllocator
{
public:
    void Initialize(
        cudaStream_t stream,
        int channel_count,
        const int* sizes);

    void Shutdown();

    // Is the allocator compatible with the requested buffer type?
    bool IsCompatible(
        cudaStream_t stream,
        int channel_count,
        const int* sizes) const;

    // This fills in the pointer and size but not the others
    std::shared_ptr<JpegResultBuffer> Allocate();
    void Free(std::shared_ptr<JpegResultBuffer>& buffer);

protected:
    cudaStream_t NvStream = nullptr;
    int ChannelCount = 0;
    int Sizes[4];

    std::atomic<int> AllocationCount = ATOMIC_VAR_INIT(0);

    std::mutex Lock;
    std::vector< std::shared_ptr<JpegResultBuffer> > Freed;
};


//------------------------------------------------------------------------------
// JpegDecoder

struct JpegData
{
    const uint8_t* Data = nullptr;
    unsigned Bytes = 0;
};

class JpegDecoder
{
public:
    // This must be initialized and shutdown on the same thread due to TBB.
    bool Initialize(int width, int height);
    void Shutdown();

    // Decode JPEG batch
    bool Decode(
        std::vector<JpegData>& jpegs,
        std::shared_ptr<JpegBufferAllocator>& allocator,
        std::vector<std::shared_ptr<JpegResultBuffer>>& decoded,
        bool copy_back_to_cpu);

protected:
    int Width = 0, Height = 0;
    bool Initialized = false;

    std::shared_ptr<JpegBufferAllocator> Allocator;

    CudaContext Context;
    nvjpegHandle_t NvJpeg = nullptr;
    nvjpegJpegState_t NvState = nullptr;
    cudaStream_t NvStream = nullptr;

    int BatchInitializeCount = 0;

    std::vector<std::shared_ptr<JpegResultBuffer>> OutputBuffers;
    std::vector<nvjpegImage_t> OutputImages;
};


} // namespace nvcuvid
