// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "NvJpegDecoder.hpp"

#include <core.hpp>
#include <core_logging.hpp>

namespace nvcuvid {


//------------------------------------------------------------------------------
// Tools

const char* NvJpegStatusToString(nvjpegStatus_t status)
{
    switch (status)
    {
    case NVJPEG_STATUS_SUCCESS: return "NVJPEG_STATUS_SUCCESS";
    case NVJPEG_STATUS_NOT_INITIALIZED: return "NVJPEG_STATUS_NOT_INITIALIZED";
    case NVJPEG_STATUS_INVALID_PARAMETER: return "NVJPEG_STATUS_INVALID_PARAMETER";
    case NVJPEG_STATUS_BAD_JPEG: return "NVJPEG_STATUS_BAD_JPEG";
    case NVJPEG_STATUS_JPEG_NOT_SUPPORTED: return "NVJPEG_STATUS_JPEG_NOT_SUPPORTED";
    case NVJPEG_STATUS_ALLOCATOR_FAILURE: return "NVJPEG_STATUS_ALLOCATOR_FAILURE";
    case NVJPEG_STATUS_EXECUTION_FAILED: return "NVJPEG_STATUS_EXECUTION_FAILED";
    case NVJPEG_STATUS_ARCH_MISMATCH: return "NVJPEG_STATUS_ARCH_MISMATCH";
    case NVJPEG_STATUS_INTERNAL_ERROR: return "NVJPEG_STATUS_INTERNAL_ERROR";
    case NVJPEG_STATUS_IMPLEMENTATION_NOT_SUPPORTED: return "NVJPEG_STATUS_IMPLEMENTATION_NOT_SUPPORTED";
    default: break;
    }
    return "Unknown";
}


//------------------------------------------------------------------------------
// JpegBufferAllocator

void JpegBufferAllocator::Initialize(
    cudaStream_t stream,
    int channel_count,
    const int* sizes)
{
    NvStream = stream;
    ChannelCount = channel_count;
    for (int i = 0; i < channel_count; ++i) {
        Sizes[i] = sizes[i];
    }
}

void JpegBufferAllocator::Shutdown()
{
    Freed.clear();
}

bool JpegBufferAllocator::IsCompatible(
    cudaStream_t stream,
    int channel_count,
    const int* sizes) const
{
    if (stream != NvStream || ChannelCount != channel_count) {
        return false;
    }
    for (int i = 0; i < channel_count; ++i) {
        if (Sizes[i] != sizes[i]) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<JpegResultBuffer> JpegBufferAllocator::Allocate()
{
    std::shared_ptr<JpegResultBuffer> buffer;
    {
        std::lock_guard<std::mutex> locker(Lock);
        if (!Freed.empty()) {
            buffer = Freed.back();
            Freed.pop_back();
            return buffer;
        }
    }

    const int allocation_count = ++AllocationCount;
    spdlog::info("JpegBufferAllocator: Allocating buffer # {}", allocation_count);

    buffer = std::make_shared<JpegResultBuffer>();
    buffer->ChannelCount = ChannelCount;
    buffer->TotalSize = 0;
    buffer->NvStream = NvStream;

    for (int i = 0; i < ChannelCount; ++i)
    {
        const int channel_size = Sizes[i];
        auto& channel = buffer->Channels[i];
        channel.Size = channel_size;
        channel.Width = 0;
        channel.Height = 0;
        channel.Stride = 0;

        // Allocate device buffer
        cudaError_t error = cudaMallocManaged(
            &channel.ManagedPtr,
            channel_size);
        if (error != cudaSuccess) {
            spdlog::error("cudaMalloc failed: {} {} {}",
                error, cudaGetErrorName(error), cudaGetErrorString(error));
            return nullptr;
        }

        error = cudaStreamAttachMemAsync(
            NvStream,
            channel.ManagedPtr,
            channel_size,
            cudaMemAttachSingle);
        if (error != cudaSuccess) {
            spdlog::error("cudaStreamAttachMemAsync failed: {} {} {}",
                error, cudaGetErrorName(error), cudaGetErrorString(error));
            return nullptr;
        }

        buffer->TotalSize += channel_size;
    }

    for (int i = ChannelCount; i < NVJPEG_MAX_COMPONENT; ++i) {
        auto& channel = buffer->Channels[i];
        channel.ManagedPtr = nullptr;
        channel.Size = 0;
        channel.Width = 0;
        channel.Height = 0;
        channel.Stride = 0;
    }

    return buffer;
}

void JpegBufferAllocator::Free(std::shared_ptr<JpegResultBuffer>& buffer)
{
    std::lock_guard<std::mutex> locker(Lock);
    Freed.push_back(buffer);
}


//------------------------------------------------------------------------------
// JpegResultBuffer

JpegResultBuffer::~JpegResultBuffer()
{
    for (int i = 0; i < ChannelCount; ++i)
    {
        auto& channel = Channels[i];
        if (channel.ManagedPtr) {
            cudaFree(channel.ManagedPtr);
            channel.ManagedPtr = nullptr;
        }
    }
}


//------------------------------------------------------------------------------
// JpegDecoder

bool JpegDecoder::Initialize(int width, int height)
{
    if (Initialized && Width == width && Height == height) {
        return true;
    }

    Shutdown();

    Width = width;
    Height = height;

    if (!Context.Create()) {
        spdlog::error("Failed to create CUDA context");
        return false;
    }

    cudaError_t error = cudaStreamCreateWithFlags(&NvStream, cudaStreamNonBlocking);
    if (error != cudaSuccess) {
        spdlog::error("cudaStreamCreateWithFlags failed: {}[{}] {}", error, cudaGetErrorName(error), cudaGetErrorString(error));
        return false;
    }

    nvjpegStatus_t status = nvjpegCreateEx(
        NVJPEG_BACKEND_DEFAULT,
        nullptr, // dev allocator
        nullptr, // pinned allocator
        NVJPEG_FLAGS_DEFAULT,
        &NvJpeg);
    if (status != NVJPEG_STATUS_SUCCESS) {
        spdlog::error("nvjpegCreateEx failed: {} {}", status, NvJpegStatusToString(status), cudaGetErrorString(error));
        return false;
    }

    status = nvjpegJpegStateCreate(NvJpeg, &NvState);
    if (status != NVJPEG_STATUS_SUCCESS) {
        spdlog::error("nvjpegJpegStateCreate failed: {} {}", status, NvJpegStatusToString(status), cudaGetErrorString(error));
        return false;
    }

    Initialized = true;
    spdlog::info("Successfully initialized nvJPEG decoding");
    return true;
}

void JpegDecoder::Shutdown()
{
    if (NvStream) {
        cudaStreamDestroy(NvStream);
        NvStream = 0;
    }
    if (NvState) {
        nvjpegJpegStateDestroy(NvState);
        NvState = 0;
    }
    if (NvJpeg) {
        nvjpegDestroy(NvJpeg);
        NvJpeg = 0;
    }
    Width = 0;
    Height = 0;
    Initialized = false;
}

bool JpegDecoder::Decode(
    std::vector<JpegData>& jpegs,
    std::shared_ptr<JpegBufferAllocator>& allocator,
    std::vector<std::shared_ptr<JpegResultBuffer>>& decoded,
    bool copy_back_to_cpu)
{
    const int jpeg_count = static_cast<int>( jpegs.size() );
    if (jpeg_count <= 0) {
        spdlog::error("No jpegs");
        return false;
    }

    const uint64_t start_usec = core::GetTimeUsec();

    allocator.reset();
    decoded.clear();

    // Re-initialize the batch decoder if needed
    if (BatchInitializeCount != jpeg_count) {
        nvjpegStatus_t status = nvjpegDecodeBatchedInitialize(
            NvJpeg,
            NvState,
            jpeg_count, // batch size
            jpeg_count, // max CPU threads
            NVJPEG_OUTPUT_YUV);
        if (status != NVJPEG_STATUS_SUCCESS) {
            spdlog::error("nvjpegDecodeBatchedInitialize failed: {} {}", status, NvJpegStatusToString(status));
            return false;
        }
        OutputImages.resize(jpeg_count);
        OutputBuffers.resize(jpeg_count);
    }
    BatchInitializeCount = jpeg_count;

    int channels = 0;
    nvjpegChromaSubsampling_t subsampling;
    int widths[NVJPEG_MAX_COMPONENT];
    int heights[NVJPEG_MAX_COMPONENT];

    nvjpegStatus_t status = nvjpegGetImageInfo(
        NvJpeg,
        jpegs[0].Data,
        jpegs[0].Bytes,
        &channels,
        &subsampling,
        widths,
        heights);
    if (status != NVJPEG_STATUS_SUCCESS) {
        spdlog::error("nvjpegGetImageInfo failed: {} {}", status, NvJpegStatusToString(status));
        return false;
    }

    if (subsampling != NVJPEG_CSS_422 || channels != 3) {
        spdlog::error("JPEG data unexpected: subsampling={} channels={}", subsampling, channels);
        return false;
    }

    tbb::task_group phase_one_tasks;
    std::atomic<bool> failed = ATOMIC_VAR_INIT(false);

    for (int jpeg_index = 0; jpeg_index < jpeg_count; ++jpeg_index)
    {
        JpegData jpeg = jpegs[jpeg_index];
        phase_one_tasks.run([this, jpeg_index, jpeg, &failed]() {
            nvjpegStatus_t status = nvjpegDecodeBatchedPhaseOne(
                NvJpeg,
                NvState,
                jpeg.Data,
                jpeg.Bytes,
                jpeg_index, // image index
                jpeg_index, // thread index
                NvStream);
            if (status != NVJPEG_STATUS_SUCCESS) {
                spdlog::error("nvjpegDecodeBatchedPhaseOne failed: {} {}", status, NvJpegStatusToString(status));
                failed = true;
            }
        });
    }

    phase_one_tasks.wait();

    if (failed) {
        BatchInitializeCount = 0;
        spdlog::error("Worker reported failure");
        return false;
    }

    status = nvjpegDecodeBatchedPhaseTwo(
        NvJpeg,
        NvState,
        NvStream);
    if (status != NVJPEG_STATUS_SUCCESS) {
        spdlog::error("nvjpegDecodeBatchedPhaseTwo failed: {} = {}", status, NvJpegStatusToString(status));
        BatchInitializeCount = 0;
        return false;
    }

    int sizes[NVJPEG_MAX_COMPONENT];
    for (int i = 0; i < channels; ++i) {
        sizes[i] = widths[i] * heights[i];
    }

    if (!Allocator || !Allocator->IsCompatible(NvStream, channels, sizes))
    {
        Allocator = std::make_shared<JpegBufferAllocator>();
        Allocator->Initialize(NvStream, channels, sizes);
    }

    for (int jpeg_index = 0; jpeg_index < jpeg_count; ++jpeg_index)
    {
        auto buffer = Allocator->Allocate();
        if (!buffer) {
            spdlog::error("nvJPEG: Buffers.Allocate failed");
            return false;
        }
        OutputBuffers[jpeg_index] = buffer;

        for (int j = 0; j < channels; ++j) {
            buffer->Channels[j].Width = widths[j];
            buffer->Channels[j].Height = heights[j];
            buffer->Channels[j].Stride = widths[j];

            OutputImages[jpeg_index].channel[j] = reinterpret_cast<uint8_t*>( buffer->Channels[j].ManagedPtr );
            OutputImages[jpeg_index].pitch[j] = widths[j];
        }
    }

    status = nvjpegDecodeBatchedPhaseThree(
        NvJpeg,
        NvState,
        OutputImages.data(),
        NvStream);
    if (status != NVJPEG_STATUS_SUCCESS) {
        spdlog::error("nvjpegDecodeBatchedPhaseThree failed: {} {}", status, NvJpegStatusToString(status));
        BatchInitializeCount = 0;
        return false;
    }

    for (int jpeg_index = 0; jpeg_index < jpeg_count; ++jpeg_index)
    {
        auto& buffer = OutputBuffers[jpeg_index];

        if (copy_back_to_cpu)
        {
            for (int j = 0; j < channels; ++j)
            {
                auto& channel = buffer->Channels[j];

                channel.HostData.resize(channel.Stride * channel.Height);

                cudaError_t error = cudaMemcpy2DAsync(
                    channel.HostData.data(),
                    channel.Width,
                    channel.ManagedPtr,
                    channel.Stride,
                    channel.Width,
                    channel.Height,
                    cudaMemcpyKind::cudaMemcpyDeviceToHost,
                    NvStream);
                if (error != cudaSuccess) {
                    spdlog::error("cudaMemcpy2DAsync failed: {} {} {}",
                        error, cudaGetErrorName(error), cudaGetErrorString(error));
                    return false;
                }
            }
        }

        buffer->AvailableOnCpu = copy_back_to_cpu;
    }

    if (copy_back_to_cpu) {
        // Wait for all operations to complete
        cuStreamSynchronize(NvStream);
    }

    const uint64_t end_usec = core::GetTimeUsec();
    const uint64_t decode_usec = end_usec - start_usec;
    if (decode_usec > 33000) {
        spdlog::warn("Slow JPEG decode in {} msec", decode_usec / 1000.f);
    }

    allocator = Allocator;
    decoded = OutputBuffers;
    return true;
}


} // namespace nvcuvid
