// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "MfxVideoDecoder.hpp"

#include <core_logging.hpp>
#include <core_string.hpp>

namespace mfx {


//------------------------------------------------------------------------------
// VideoDecoder

bool VideoDecoder::Initialize(
    bool gpu_output,
    mfxU32 codec_fourcc,
    const uint8_t* data,
    int bytes)
{
    if (!Context) {
        Context = std::make_shared<MfxContext>();
        if (!Context->Initialize()) {
            return false;
        }
    }

    MfxDecode = std::make_unique<MFXVideoDECODE>(Context->Session);

    mfxBitstream bs{};
    bs.Data = (mfxU8*)data;

    // Must set both of these for DecodeHeader() to work
    bs.DataLength = bs.MaxLength = bytes;

    // Must set codec id for it to decode
    VideoParams.mfx.CodecId = codec_fourcc;

    // When provided with parameter sets this extracts: CodecProfile, CodecLevel, IDR interval
    // This is a lot more convenient than dealing with the bitstream manually!
    mfxStatus status = MfxDecode->DecodeHeader(&bs, &VideoParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("MfxDecode->DecodeHeader failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    const auto& info = VideoParams.mfx.FrameInfo;

    spdlog::info("Detected video: Format:{} Resolution={}x{} Chroma={} FPS={}/{}",
        FourCCToString(VideoParams.mfx.CodecId),
        info.Width, info.Height,
        MfxChromaFormatToString(info.ChromaFormat),
        info.FrameRateExtN, info.FrameRateExtD);

    // Low latency mode
    VideoParams.AsyncDepth = 1; // No output delay

    mfxFrameAllocRequest request{};

    // Try using GPU memory:
    if (gpu_output && Context->SupportsGpuSurfaces && !Allocator)
    {
        VideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        status = MfxDecode->QueryIOSurf(&VideoParams, &request);
        if (status < MFX_ERR_NONE) {
            spdlog::warn("Decoder: MfxDecode->QueryIOSurf for D3D so falling back to system memory: {} {}", status, MfxStatusToString(status));
        }
        else
        {
            Allocator = std::make_shared<D3DAllocator>();
            if (!Allocator->Initialize(Context, VideoParams)) {
                spdlog::warn("Decoder: MFX allocator for D3D failed so falling back to system memory");
                Allocator.reset();
            }
        }
    }

    // Use system memory:
    if (!Allocator)
    {
        VideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        status = MfxDecode->QueryIOSurf(&VideoParams, &request);
        if (status < MFX_ERR_NONE) {
            spdlog::error("MfxDecode->QueryIOSurf failed: {} {}", status, MfxStatusToString(status));
            return false;
        }

        Allocator = std::make_shared<SystemAllocator>();
        if (!Allocator->Initialize(Context, VideoParams)) {
            return false;
        }
    }

    status = MfxDecode->Init(&VideoParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("MfxDecode->Init failed: {} {}", status, MfxStatusToString(status));
        return false;
    }
    if (MFX_WRN_PARTIAL_ACCELERATION == status) {
        spdlog::warn("Decoder warning: Partial acceleration");
    }

    status = MfxDecode->GetVideoParam(&VideoParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("MfxDecode->GetVideoParam failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    return true;
}

void VideoDecoder::Shutdown()
{
    Terminated = true;

    MfxDecode.reset();
    Allocator.reset();
    Context.reset();
}

frameref_t VideoDecoder::Decode(const uint8_t* data, int bytes)
{
    FrameRef.reset();

    // Trigger a reset to recover from decoder errors.
    // Must feed in parameter sets again after a reset.
    if (NeedsReset) {
        mfxStatus status = MfxDecode->Reset(&VideoParams);
        if (status < MFX_ERR_NONE) {
            spdlog::error("MfxDecode->Reset failed: {} {}", status, MfxStatusToString(status));
            status = MfxDecode->Close();
            if (status < MFX_ERR_NONE) {
                spdlog::error("MfxDecode->Close failed: {} {}", status, MfxStatusToString(status));
            }
            return nullptr;
        }
        NeedsReset = false;
    }

    mfxBitstream bs{};
    bs.Data = (mfxU8*)data;

    // Must set both of these for DecodeHeader() to work
    bs.DataLength = bs.MaxLength = bytes;
    bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME; // Hint: We are passing full frames

    mfxStatus status;
    frameref_t output_frame;
    mfxSyncPoint sync_point = nullptr;

    while (!Terminated)
    {
        sync_point = nullptr;

        frameref_t work_frame = Allocator->Allocate();

        mfxFrameSurface1* output_surface = nullptr;
        status = MfxDecode->DecodeFrameAsync(
            &bs,
            &work_frame->Raw->Surface,
            &output_surface,
            &sync_point);

        if (output_surface) {
            output_frame = Allocator->GetFrameById(output_surface->Data.MemId);
            if (!output_frame) {
                spdlog::error("Allocator.GetFrameByIndex failed to find memid");
                return nullptr;
            }
        }

        if (status < MFX_ERR_NONE) {
            spdlog::error("MfxDecode->DecodeFrameAsync failed: {} {}", status, MfxStatusToString(status));
            NeedsReset |= MfxStatusInvalidatesCodec(status);
            return nullptr;
        }

        // If a frame is ready:
        if (sync_point && output_surface) {
            break;
        }

        // Ignore warning for video parameter changes
        if (status != MFX_WRN_VIDEO_PARAM_CHANGED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    const uint32_t sync_wait_msec = 100;
    status = Context->Session.SyncOperation(sync_point, sync_wait_msec);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Decode: Context->Session.SyncOperation failed: {} {}", status, MfxStatusToString(status));
        NeedsReset |= MfxStatusInvalidatesCodec(status);
        return nullptr;
    }

    if (!output_frame) {
        spdlog::error("MfxDecode->DecodeFrameAsync output surface is null");
        return nullptr;
    }

    FrameRef = output_frame;
    return output_frame;
}


} // namespace mfx
