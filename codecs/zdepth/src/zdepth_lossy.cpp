// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "zdepth_lossy.hpp"

#include "libdivide.h"

#include <zstd.h> // Zstd
#include <string.h> // memcpy

#include <core_logging.hpp>

namespace lossy {


//------------------------------------------------------------------------------
// Constants

// Size of a block for predictor selection purposes
static const int kBlockSize = 8;

// Zstd compression level
static const int kZstdLevel = 1;

const char* DepthResultString(DepthResult result)
{
    switch (result)
    {
    case DepthResult::Success: return "Success";
    case DepthResult::FileTruncated: return "FileTruncated";
    case DepthResult::WrongFormat: return "WrongFormat";
    case DepthResult::Corrupted: return "Corrupted";
    case DepthResult::MissingFrame: return "MissingFrame";
    default: break;
    }
    return "Unknown";
}


//------------------------------------------------------------------------------
// Tools

bool IsDepthFrame(const uint8_t* file_data, unsigned file_bytes)
{
    if (file_bytes < kDepthHeaderBytes) {
        return false;
    }
    if (file_data[0] != kDepthFormatMagic) {
        return false;
    }
    return true;
}

bool IsKeyFrame(const uint8_t* file_data, unsigned file_bytes)
{
    if (!IsDepthFrame(file_data, file_bytes)) {
        return false;
    }
    return (file_data[1] & 1) != 0;
}


//------------------------------------------------------------------------------
// Depth Quantization

uint16_t AzureKinectQuantizeDepth(uint16_t depth)
{
    if (depth <= 200) {
        return 0; // Too close
    }
    if (depth < 750) {
        return depth - 200;
    }
    if (depth < 1500) {
        return 550 + (depth - 750) / 2;
    }
    if (depth < 3000) {
        return 925 + (depth - 1500) / 4;
    }
    if (depth < 6000) {
        return 1300 + (depth - 3000) / 8;
    }
    if (depth < 11840) {
        return 1675 + (depth - 6000) / 16;
    }
    return 0; // Too far
}

uint16_t AzureKinectDequantizeDepth(uint16_t quantized)
{
    if (quantized == 0) {
        return 0;
    }
    if (quantized < 550) {
        return quantized + 200;
    }
    if (quantized < 925) {
        return 750 + (quantized - 550) * 2;
    }
    if (quantized < 1300) {
        return 1500 + (quantized - 925) * 4;
    }
    if (quantized < 1675) {
        return 3000 + (quantized - 1300) * 8;
    }
    if (quantized < 2040) {
        return 6000 + (quantized - 1675) * 16;
    }
    return 0; // Invalid value
}

void QuantizeDepthImage(
    int n,
    const uint16_t* depth,
    std::vector<uint16_t>& quantized)
{
    quantized.resize(n);
    uint16_t* dest = quantized.data();

    for (int i = 0; i < n; ++i) {
        dest[i] = AzureKinectQuantizeDepth(depth[i]);
    }
}

void DequantizeDepthImage(std::vector<uint16_t>& depth_inout)
{
    const int n = static_cast<int>( depth_inout.size() );
    uint16_t* depth = depth_inout.data();

    for (int i = 0; i < n; ++i) {
        depth[i] = AzureKinectDequantizeDepth(depth[i]);
    }
}


//------------------------------------------------------------------------------
// Depth Rescaling

void RescaleImage_11Bits(
    std::vector<uint16_t>& quantized,
    uint16_t& min_value,
    uint16_t& max_value)
{
    uint16_t* data = quantized.data();
    const int size = static_cast<int>( quantized.size() );

    // Find extrema
    int i;
    for (i = 0; i < size; ++i) {
        if (data[i] != 0) {
            break;
        }
    }
    if (i >= size) {
        min_value = max_value = 0;
        return;
    }
    unsigned smallest = data[i];
    unsigned largest = smallest;
    for (; i < size; ++i) {
        const unsigned x = data[i];
        if (x == 0) {
            continue;
        }
        if (smallest > x) {
            smallest = x;
        }
        if (largest < x) {
            largest = x;
        }
    }

    min_value = static_cast<uint16_t>( smallest );
    max_value = static_cast<uint16_t>( largest );

    // Handle edge cases
    const unsigned range = largest - smallest + 1;
    if (range >= 2048) {
        return;
    }
    if (range <= 1) {
        if (smallest != 0) {
            for (i = 0; i < size; ++i) {
                unsigned x = data[i];
                if (x == 0) {
                    continue;
                }
                data[i] = 1;
            }
        }
        return;
    }
    const unsigned rounder = range / 2;

    using branchfree_t = libdivide::branchfree_divider<unsigned>;
    branchfree_t fast_d = range;

    // Rescale the data
    for (i = 0; i < size; ++i) {
        unsigned x = data[i];
        if (x == 0) {
            continue;
        }
        x -= smallest;
        unsigned y = (x * 2047 + rounder) / fast_d;
        data[i] = static_cast<uint16_t>(y + 1);
    }
}

void UndoRescaleImage_11Bits(
    uint16_t min_value,
    uint16_t max_value,
    std::vector<uint16_t>& quantized)
{
    uint16_t* data = quantized.data();
    const int size = static_cast<int>( quantized.size() );

    const unsigned smallest = min_value;
    const unsigned range = max_value - smallest + 1;
    if (range >= 2048) {
        return;
    }
    if (range <= 1) {
        for (int i = 0; i < size; ++i) {
            unsigned x = data[i];
            if (x == 0) {
                continue;
            }
            data[i] = static_cast<uint16_t>( x - 1 + smallest );
        }
        return;
    }

    // Rescale the data
    for (int i = 0; i < size; ++i) {
        unsigned x = data[i];
        if (x == 0) {
            continue;
        }
        --x;
        const unsigned y = (x * range + 1023) / 2047;
        data[i] = static_cast<uint16_t>(y + smallest);
    }
}


//------------------------------------------------------------------------------
// Zstd

void ZstdCompress(
    const std::vector<uint8_t>& uncompressed,
    std::vector<uint8_t>& compressed)
{
    compressed.resize(ZSTD_compressBound(uncompressed.size()));
    const size_t size = ZSTD_compress(
        compressed.data(),
        compressed.size(),
        uncompressed.data(),
        uncompressed.size(),
        kZstdLevel);
    if (ZSTD_isError(size)) {
        compressed.clear();
        return;
    }
    compressed.resize(size);
}

bool ZstdDecompress(
    const uint8_t* compressed_data,
    int compressed_bytes,
    int uncompressed_bytes,
    std::vector<uint8_t>& uncompressed)
{
    uncompressed.resize(uncompressed_bytes);
    const size_t size = ZSTD_decompress(
        uncompressed.data(),
        uncompressed.size(),
        compressed_data,
        compressed_bytes);
    if (ZSTD_isError(size)) {
        return false;
    }
    if (size != static_cast<size_t>( uncompressed_bytes )) {
        return false;
    }
    return true;
}


//------------------------------------------------------------------------------
// DepthCompressor

void DepthCompressor::Compress(
    int width,
    int height,
    bool hevc,
    unsigned framerate,
    const uint16_t* unquantized_depth,
    std::vector<uint8_t>& compressed,
    bool keyframe)
{
    DepthHeader header;
    header.Magic = kDepthFormatMagic;
    header.Flags = 0;
    if (keyframe) {
        header.Flags |= DepthFlags_Keyframe;
    }
    if (hevc) {
        header.Flags |= DepthFlags_HEVC;
    }
    header.Width = static_cast<uint16_t>( width );
    header.Height = static_cast<uint16_t>( height );
    const int n = width * height;

    // Enforce keyframe if we have not compressed anything yet
    if (FrameCount == 0) {
        keyframe = true;
    }
    header.FrameNumber = static_cast<uint16_t>( FrameCount );
    ++FrameCount;

    QuantizeDepthImage(n, unquantized_depth, QuantizedDepth);
    RescaleImage_11Bits(QuantizedDepth, header.MinimumDepth, header.MaximumDepth);
    Filter(QuantizedDepth);

    if (!Encoder || LastWidth != (unsigned)width || LastHeight != (unsigned)height)
    {
        spdlog::debug("Zdepth lossy encoder resolution changed: {}x{}", width, height);

        LastWidth = (unsigned)width;
        LastHeight = (unsigned)height;
        Encoder = std::make_unique<mfx::VideoEncoder>();

        const float bitrate_scale = width * height / static_cast<float>(320 * 288);
        const unsigned bitrate = static_cast<unsigned>( 3000000 * bitrate_scale );

        mfx::EncoderParams encoder_params;
        encoder_params.Bitrate = bitrate;
        encoder_params.Quality = 20;
        encoder_params.ProcAmp.Enabled = false; // No denoising etc
        encoder_params.FourCC = hevc ? MFX_CODEC_HEVC : MFX_CODEC_AVC;
        encoder_params.Framerate = framerate;
        encoder_params.Height = height;
        encoder_params.Width = width;
        encoder_params.IntraRefreshCycleSize = framerate;
        encoder_params.IntraRefreshQPDelta = -5;

        Context = std::make_shared<mfx::MfxContext>();
        if (!Context->Initialize()) {
            spdlog::error("Zdepth: Video encoder initialization failed");
            Encoder.reset();
            return;
        }

        Allocator = std::make_shared<mfx::SystemAllocator>();

        const bool success = Allocator->InitializeNV12SystemOnly(width, height, framerate);
        if (!success || !Encoder->Initialize(Allocator, encoder_params)) {
            Encoder.reset();
            return;
        }

        Parser.reset();
    }

    // Copy frame data to buffer allocator
    mfx::frameref_t frame = Allocator->Allocate();
    auto& surface_data = frame->Raw->Surface.Data;
    memcpy(surface_data.Y, Low.data(), n);
    memset(surface_data.U, 0, n / 2);

    // Interleave Zstd compression with video encoder work.
    // Only saves about 400 microseconds from a 5000 microsecond encode.
    ZstdCompress(High, HighOut);
    header.HighUncompressedBytes = static_cast<uint32_t>( High.size() );
    header.HighCompressedBytes = static_cast<uint32_t>( HighOut.size() );

    // Start encoder
    mfx::VideoEncoderOutput video = Encoder->Encode(frame, keyframe);

    if (video.Bytes <= 0) {
        spdlog::error("Zdepth lossy encoder failed: Reseting encoder!");
        Encoder.reset();
        return;
    }

    // Parse encoded video:

    if (!Parser) {
        Parser = std::make_unique<core::VideoParser>();
        VideoParameters.clear();
    }
    Parser->Reset();
    Parser->ParseVideo(
        hevc,
        video.Data,
        video.Bytes);

    if (Parser->Pictures.size() != 1) {
        spdlog::error("Zdepth: Found {} frames in encoder output", Parser->Pictures.size());
        Encoder.reset();
        return;
    }

    if (Parser->TotalParameterBytes > 0)
    {
        VideoParameters.resize(Parser->TotalParameterBytes);
        uint8_t* dest = VideoParameters.data();
        for (auto& nalu : Parser->Parameters) {
            memcpy(dest, nalu.Ptr, nalu.Bytes);
            dest += nalu.Bytes;
        }
    }

    auto& picture = Parser->Pictures[0];
    int compressed_bytes = picture.TotalBytes;
    if (keyframe) {
        compressed_bytes += static_cast<int>( VideoParameters.size() );
    }

    LowOut.resize(compressed_bytes);
    uint8_t* dest = LowOut.data();

    if (keyframe) {
        if (VideoParameters.empty()) {
            spdlog::error("Zdepth: Video parameters not available for keyframe");
            Encoder.reset();
            return;
        }
        memcpy(dest, VideoParameters.data(), VideoParameters.size());
        dest += VideoParameters.size();
    }

    for (auto& nalu : picture.Ranges) {
        memcpy(dest, nalu.Ptr, nalu.Bytes);
        dest += nalu.Bytes;
    }

    header.LowCompressedBytes = static_cast<uint32_t>( LowOut.size() );

    // Calculate output size
    size_t total_size = kDepthHeaderBytes + HighOut.size() + LowOut.size();
    compressed.resize(total_size);
    uint8_t* copy_dest = compressed.data();

    // Write header
    memcpy(copy_dest, &header, kDepthHeaderBytes);
    copy_dest += kDepthHeaderBytes;

    // Concatenate the compressed data
    memcpy(copy_dest, HighOut.data(), HighOut.size());
    copy_dest += HighOut.size();
    memcpy(copy_dest, LowOut.data(), LowOut.size());
}

DepthResult DepthCompressor::Decompress(
    const std::vector<uint8_t>& compressed,
    int& width,
    int& height,
    std::vector<uint16_t>& depth_out)
{
    if (compressed.size() < kDepthHeaderBytes) {
        return DepthResult::FileTruncated;
    }
    const uint8_t* src = compressed.data();

    const DepthHeader* header = reinterpret_cast<const DepthHeader*>( src );
    if (header->Magic != kDepthFormatMagic) {
        return DepthResult::WrongFormat;
    }
    const bool keyframe = (header->Flags & DepthFlags_Keyframe) != 0;
    const unsigned frame_number = header->FrameNumber;

#if 0 // This is okay I guess since we are using intra-frame compression.
    if (!keyframe && frame_number != CompressedFrameNumber + 1) {
        return DepthResult::MissingPFrame;
    }
#endif

    width = header->Width;
    height = header->Height;
    if (width < 1 || width > 4096 || height < 1 || height > 4096) {
        return DepthResult::Corrupted;
    }

    // Read header
    unsigned total_bytes = kDepthHeaderBytes + header->HighCompressedBytes + header->LowCompressedBytes;
    if (header->HighUncompressedBytes < 2) {
        return DepthResult::Corrupted;
    }
    if (compressed.size() != total_bytes) {
        return DepthResult::FileTruncated;
    }

    src += kDepthHeaderBytes;
    const uint8_t* zstd_src = src;
    src += header->HighCompressedBytes;
    const uint8_t* video_src = src;
    //src += header->LowCompressedBytes;

    if (!IntelDecoder || LastWidth != static_cast<unsigned>(width) || LastHeight != static_cast<unsigned>(height))
    {
        if (!keyframe) {
            return DepthResult::MissingFrame;
        }

        LastWidth = width;
        LastHeight = height;
        IntelDecoder.reset();
#ifdef ZDEPTH_NVCUVID
        NvidiaDecoder.reset();
#endif

        IntelDecoder = std::make_unique<mfx::VideoDecoder>();
        if (!IntelDecoder->Initialize(
            false, // prefer on CPU
            (header->Flags & DepthFlags_HEVC) != 0 ? MFX_CODEC_HEVC : MFX_CODEC_AVC,
            video_src,
            header->LowCompressedBytes
        ))
        {
            spdlog::error("Intel decoder failed to initialize");
            IntelDecoder.reset();
        } else {
            spdlog::info("Zdepth lossy encoder initialized: resolution={}x{}", width, height);
        }
    }

    const uint8_t* decoded_low_data = nullptr;

    if (!IntelDecoder) {
#ifdef ZDEPTH_NVCUVID
        if (!NvidiaDecoder) {
            NvidiaDecoder = std::make_unique<nvcuvid::VideoCodec>();
        }

        nvcuvid::VideoDecodeInput vinput{};
        vinput.Mode = nvcuvid::DecodeMode::MonochromeOnly;
        vinput.Type = (header->Flags & DepthFlags_HEVC) != 0 ? nvcuvid::VideoType::H265 : nvcuvid::VideoType::H264;
        vinput.Bytes = header->LowCompressedBytes;
        vinput.Data = video_src;
        vinput.Width = header->Width;
        vinput.Height = header->Height;

        if (!NvidiaDecoder->Decode(vinput, Low)) {
            spdlog::error("Nvidia decoder failed");
            NvidiaDecoder.reset();
            return DepthResult::Error;
        }
        decoded_low_data = Low.data();
#else
        spdlog::error("Intel GPU decoder is not available: Please enable it in your BIOS settings.");
        return DepthResult::Error;
#endif
    }

    // Decompress high bits
    bool success = ZstdDecompress(
        zstd_src,
        header->HighCompressedBytes,
        header->HighUncompressedBytes,
        High);
    if (!success) {
        return DepthResult::Corrupted;
    }

    // Finish decoding low bits
    if (IntelDecoder) {
        mfx::frameref_t frame = IntelDecoder->Decode(
            video_src,
            header->LowCompressedBytes);
        if (!frame) {
            spdlog::error("Failed to decode video frame");
            IntelDecoder.reset();
            return DepthResult::Error;
        }
        decoded_low_data = frame->Raw->Data.data();
    }

    Unfilter(width, height, decoded_low_data, depth_out);
    UndoRescaleImage_11Bits(header->MinimumDepth, header->MaximumDepth, depth_out);
    DequantizeDepthImage(depth_out);

    return DepthResult::Success;
}


//------------------------------------------------------------------------------
// DepthCompressor : Filtering

void DepthCompressor::Filter(
    const std::vector<uint16_t>& depth_in)
{
    const int n = static_cast<int>( depth_in.size() );
    const uint16_t* depth = depth_in.data();

    High.clear();
    Low.clear();
    High.resize(n / 2); // One byte for every two depth values
    Low.resize(n + n / 2); // Leave room for unused chroma channel

    // Split data into high/low parts
    for (int i = 0; i < n; i += 2) {
        const uint16_t depth_0 = depth[i];
        const uint16_t depth_1 = depth[i + 1];

        unsigned high_0 = 0, high_1 = 0;
        uint8_t low_0 = static_cast<uint8_t>( depth_0 );
        uint8_t low_1 = static_cast<uint8_t>( depth_1 );

        if (depth_0 != 0) {
            // Read high bits
            high_0 = depth_0 >> 8;

            // Fold to avoid sharp transitions from 255..0
            if (high_0 & 1) {
                low_0 = 255 - low_0;
            }

            // Preserve zeroes by offseting the values by 1
            ++high_0;
        }

        if (depth_1 != 0) {
            // Read high bits
            high_1 = depth_1 >> 8;

            // Fold to avoid sharp transitions from 255..0
            if (high_1 & 1) {
                low_1 = 255 - low_1;
            }

            // Preserve zeroes by offseting the values by 1
            ++high_1;
        }

        High[i / 2] = static_cast<uint8_t>( high_0 | (high_1 << 4) );
        Low[i] = low_0;
        Low[i + 1] = low_1;
    }
}

void DepthCompressor::Unfilter(
    int width,
    int height,
    const uint8_t* low_data,
    std::vector<uint16_t>& depth_out)
{
    const int n = width * height;
    depth_out.resize(n);
    uint16_t* depth = depth_out.data();
    const uint8_t* high_data = High.data();

    for (int i = 0; i < n; i += 2) {
        const uint8_t high = high_data[i / 2];
        uint8_t low_0 = low_data[i];
        uint8_t low_1 = low_data[i + 1];
        unsigned high_0 = high & 15;
        unsigned high_1 = high >> 4;

        if (high_0 == 0) {
            depth[i] = 0;
        } else {
            high_0--;
            if (high_0 & 1) {
                low_0 = 255 - low_0;
            }
            uint16_t x = static_cast<uint16_t>(low_0 | (high_0 << 8));

            // This value is expected to always be at least 1
            if (x == 0) {
                x = 1;
            }

            depth[i] = x;
        }

        if (high_1 == 0) {
            depth[i + 1] = 0;
        } else {
            high_1--;
            if (high_1 & 1) {
                low_1 = 255 - low_1;
            }
            uint16_t y = static_cast<uint16_t>(low_1 | (high_1 << 8));

            // This value is expected to always be at least 1
            if (y == 0) {
                y = 1;
            }

            depth[i + 1] = y;
        }
    }
}


} // namespace lossy
