// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "MfxVideoEncoder.hpp"

#include <core_logging.hpp>
#include <core_string.hpp>

namespace mfx {


//------------------------------------------------------------------------------
// MfxEncoder

bool MfxEncoder::Initialize(
    std::shared_ptr<BaseAllocator> alloc,
    const EncoderParams& params)
{
    Terminated = false;

    Allocator = alloc;
    Context = Allocator->Context;

    Encoder = std::make_unique<MFXVideoENCODE>(Context->Session);

    auto& mfx = VideoParams.mfx;
    mfx.CodecId = params.FourCC;
    if (params.FourCC == MFX_CODEC_HEVC)
    {
        // Notes:
        // + The HEVC support is done through plugins that may need to be installed.
        // + HEVC has a bug where QVBR does not work (no work-around possible):
        // https://github.com/Intel-Media-SDK/MediaSDK/commit/5c1a3d9841b17b1336e205b55546e1e20c7a7649
        // + MFX_TARGETUSAGE_BEST_QUALITY runs at 30 FPS or less for larger images
        // so we should probably use MFX_TARGETUSAGE_BALANCED instead for these.
        if (params.Width < 1920) {
            mfx.TargetUsage = MFX_TARGETUSAGE_BEST_QUALITY;
        } else {
            mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
        }
        if (params.Quality == 0) {
            mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        } else {
            mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        }
    }
    else
    {
        if (params.Quality == 0) {
            mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        } else {
            mfx.RateControlMethod = MFX_RATECONTROL_QVBR;
        }
    }
    mfx.TargetKbps = static_cast<mfxU16>( params.Bitrate / 1000 );
    mfx.Quality = static_cast<mfxU16>( params.Quality );

    auto& info = mfx.FrameInfo;
    info.FrameRateExtN = params.Framerate;
    info.FrameRateExtD = 1;
    info.FourCC = MFX_FOURCC_NV12;
    info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    info.CropX = 0;
    info.CropY = 0;
    info.CropW = static_cast<mfxU16>( params.Width );
    info.CropH = static_cast<mfxU16>( params.Height );
    // Width must be a multiple of 16
    // Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    info.Width = static_cast<mfxU16>( RoundUp16(params.Width) );
    info.Height = static_cast<mfxU16>( RoundUp16(params.Height) );

    // We submit frames in display order
    mfx.EncodedOrder = 0;

    // Low latency mode
    VideoParams.AsyncDepth = 1; // No output delay
    mfx.GopRefDist = 1;// I and P frames only

    // Maximum required size of the decoded picture buffer in frames for AVC and HEVC decoders
    CodingOptions.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    CodingOptions.Header.BufferSz = static_cast<mfxU32>( sizeof(CodingOptions) );
    CodingOptions.MaxDecFrameBuffering = 1;
    CodingOptions.AUDelimiter = MFX_CODINGOPTION_OFF;
    ExtendedBuffers.push_back(reinterpret_cast<mfxExtBuffer*>( &CodingOptions ));

    if (Context->SupportsIntraRefresh && params.IntraRefreshCycleSize != 0) {
        CodingOptions2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
        CodingOptions2.Header.BufferSz = static_cast<mfxU32>( sizeof(CodingOptions2) );
        CodingOptions2.IntRefType = MFX_REFRESH_VERTICAL;
        CodingOptions2.IntRefCycleSize = static_cast<mfxU16>( params.IntraRefreshCycleSize );
        CodingOptions2.IntRefQPDelta = static_cast<mfxI16>( params.IntraRefreshQPDelta );
        ExtendedBuffers.push_back(reinterpret_cast<mfxExtBuffer*>( &CodingOptions2 ));
    }

    if (mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        // Quality control
        CodingOptions3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
        CodingOptions3.Header.BufferSz = static_cast<mfxU32>( sizeof(CodingOptions3) );
        CodingOptions3.QVBRQuality = static_cast<mfxU16>( params.Quality );
        ExtendedBuffers.push_back(reinterpret_cast<mfxExtBuffer*>( &CodingOptions3 ));
    }

    // Specify extended buffers
    VideoParams.ExtParam = ExtendedBuffers.data();
    VideoParams.NumExtParam = static_cast<mfxU16>( ExtendedBuffers.size() );

    if (Allocator->IsVideoMemory) {
        VideoParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    } else {
        VideoParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    mfxStatus status = Encoder->Query(&VideoParams, &VideoParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Encoder->Query rejected video format: {} {}", status, MfxStatusToString(status));
        return false;
    }
    if (status == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
        spdlog::warn("Encoder warning: Incompatible video params");
    }

    spdlog::info("Detected video: Format:{} Resolution={}x{} Chroma={} FPS={}/{}",
        FourCCToString(VideoParams.mfx.CodecId),
        info.Width, info.Height,
        MfxChromaFormatToString(info.ChromaFormat),
        info.FrameRateExtN, info.FrameRateExtD);

    mfxFrameAllocRequest request{};
    status = Encoder->QueryIOSurf(&VideoParams, &request);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Encoder->QueryIOSurf failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    status = Encoder->Init(&VideoParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Encoder->Init failed: {} {}", status, MfxStatusToString(status));
        if (alloc->IsVideoMemory && params.FourCC == MFX_CODEC_HEVC) {
            spdlog::error("HEVC encoder does not support video memory input!");
        }
        return false;
    }
    if (status == MFX_WRN_PARTIAL_ACCELERATION) {
        spdlog::warn("Encoder warning: Partial acceleration");
    }

    status = Encoder->GetVideoParam(&VideoParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Encoder->GetVideoParam failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    // Adding extra bytes because they are needed in practice
    const unsigned buffer_bytes = VideoParams.mfx.BufferSizeInKB * 1000 * 2 + 4096;
    Output.resize(buffer_bytes);

    NeedsReset = false;

    return true;
}

void MfxEncoder::Shutdown()
{
    Terminated = true;

    Encoder.reset();
    Allocator.reset();
    Context.reset();
}

bool MfxEncoder::Process(frameref_t input, bool force_keyframe)
{
    // Trigger a reset to recover from decoder errors.
    // Must feed in parameter sets again after a reset.
    if (NeedsReset) {
        spdlog::warn("Reseting encoder on failure");
        mfxStatus status = Encoder->Reset(&VideoParams);
        if (status < MFX_ERR_NONE) {
            spdlog::error("Encoder->Reset failed: {} {}", status, MfxStatusToString(status));
            status = Encoder->Close();
            if (status < MFX_ERR_NONE) {
                spdlog::error("Encoder->Close failed: {} {}", status, MfxStatusToString(status));
            }
            return false;
        }
        NeedsReset = false;
    }

    mfxBitstream bs{};
    bs.Data = (mfxU8*)Output.data();
    bs.MaxLength = static_cast<mfxU32>( Output.size() );

    mfxStatus status;
    mfxSyncPoint sync_point = nullptr;

    while (!Terminated)
    {
        sync_point = nullptr;

        mfxEncodeCtrl keyframe_ctrl{};
        keyframe_ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;

        status = Encoder->EncodeFrameAsync(
            force_keyframe ? &keyframe_ctrl : nullptr,
            &input->Raw->Surface,
            &bs,
            &sync_point);

        if (status < MFX_ERR_NONE) {
            NeedsReset |= MfxStatusInvalidatesCodec(status);
            return false;
        }

        // If the hardware is not busy or we have a result:
        if (status != MFX_WRN_DEVICE_BUSY || sync_point) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const uint32_t sync_wait_msec = 100;
    status = Context->Session.SyncOperation(sync_point, sync_wait_msec);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Context->Session.SyncOperation failed: {} {}", status, MfxStatusToString(status));
        NeedsReset |= MfxStatusInvalidatesCodec(status);
        return false;
    }

    WrittenBytes = bs.DataLength;
    return WrittenBytes > 0;
}


//------------------------------------------------------------------------------
// MfxDenoiser

bool MfxDenoiser::Initialize(
    std::shared_ptr<BaseAllocator> alloc,
    const EncoderParams& params)
{
    Terminated = false;

    Allocator = alloc;
    Context = Allocator->Context;
    InitParams = params;

    Vpp = std::make_unique<MFXVideoVPP>(Context->Session);

    if (!SetupVPPParams(InitParams)) {
        return false;
    }

    mfxStatus status = Vpp->Init(&VPPParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Vpp->Init rejected video format: {} {}", status, MfxStatusToString(status));
        return false;
    }

    NeedsReset = false;
    return true;
}

void MfxDenoiser::Shutdown()
{
    Terminated = true;

    if (Vpp) {
        Vpp->Close();
        Vpp.reset();
    }
    Allocator.reset();
    Context.reset();
}

bool MfxDenoiser::ChangeProcAmp(const ProcAmpParams& params)
{
    if (InitParams.ProcAmp == params) {
        return true; // Changes already applied
    }
    InitParams.ProcAmp = params;

    if (!SetupVPPParams(InitParams)) {
        return false;
    }

    mfxStatus status = Vpp->Reset(&VPPParams);
    if (status < MFX_ERR_NONE) {
        spdlog::error("ChangeColor: Vpp->Reset failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    return true;
}

static float ClampFloat(float x, float min_value, float max_value)
{
    if (x < min_value) {
        return min_value;
    }
    if (x > max_value) {
        return max_value;
    }
    return x;
}

static unsigned ClampUnsigned(unsigned x, unsigned min_value, unsigned max_value)
{
    if (x < min_value) {
        return min_value;
    }
    if (x > max_value) {
        return max_value;
    }
    return x;
}

bool MfxDenoiser::SetupVPPParams(const EncoderParams& params)
{
    VPPParams.vpp.In.BitDepthChroma = 8;
    VPPParams.vpp.In.BitDepthLuma = 8;
    VPPParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    VPPParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    VPPParams.vpp.In.CropX = 0;
    VPPParams.vpp.In.CropY = 0;
    VPPParams.vpp.In.CropW = static_cast<mfxU16>( params.Width );
    VPPParams.vpp.In.CropH = static_cast<mfxU16>( params.Height );
    VPPParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.In.FrameRateExtN = params.Framerate;
    VPPParams.vpp.In.FrameRateExtD = 1;
    VPPParams.vpp.In.Width = static_cast<mfxU16>( RoundUp16(params.Width) );
    VPPParams.vpp.In.Height = static_cast<mfxU16>( RoundUp16(params.Height) );

    VPPParams.vpp.Out = VPPParams.vpp.In;

    if (Allocator->IsVideoMemory) {
        VPPParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    } else {
        VPPParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }

    mfxFrameAllocRequest vpp_requests[2];
    mfxStatus status = Vpp->QueryIOSurf(&VPPParams, vpp_requests);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Vpp->QueryIOSurf rejected video format: {} {}", status, MfxStatusToString(status));
        return false;
    }

    const bool enabled_brightness = true; // Always on

    ExtDoUseAlg.clear();
    ExtDoUseAlg.push_back(MFX_EXTBUFF_VPP_DENOISE); // Enable denoising
    if (enabled_brightness) {
        ExtDoUseAlg.push_back(MFX_EXTBUFF_VPP_PROCAMP); // Turn on processing amplified
    }

    ExtDoUse.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
    ExtDoUse.Header.BufferSz = sizeof(mfxExtVPPDoUse);
    ExtDoUse.NumAlg = static_cast<mfxU32>( ExtDoUseAlg.size() );
    ExtDoUse.AlgList = ExtDoUseAlg.data();

    ExtDoNotUseAlg.clear();
    ExtDoNotUseAlg.push_back(MFX_EXTBUFF_VPP_SCENE_ANALYSIS); // Turn off scene analysis
    ExtDoNotUseAlg.push_back(MFX_EXTBUFF_VPP_DETAIL); // Turn off detail enhancement
    if (!enabled_brightness) {
        ExtDoNotUseAlg.push_back(MFX_EXTBUFF_VPP_PROCAMP); // Turn off processing amplified
    }

    // Disable a number of iGPU features that slow down VPP too much
    ExtDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    ExtDoNotUse.Header.BufferSz = sizeof(mfxExtVPPDoNotUse);
    ExtDoNotUse.NumAlg = static_cast<mfxU32>( ExtDoNotUseAlg.size() );
    ExtDoNotUse.AlgList = ExtDoNotUseAlg.data();

    const auto& amp = InitParams.ProcAmp;

    if (enabled_brightness) {
        ExtProcAmpConfig.Header.BufferId = MFX_EXTBUFF_VPP_PROCAMP;
        ExtProcAmpConfig.Header.BufferSz = sizeof(ExtProcAmpConfig);
        ExtProcAmpConfig.Brightness = ClampFloat(amp.Brightness, -100.f, 100.f);
        ExtProcAmpConfig.Contrast = ClampFloat(amp.Contrast, 0.f, 10.f);
        ExtProcAmpConfig.Hue = ClampFloat(amp.Hue, -180.f, 180.f);
        ExtProcAmpConfig.Saturation = ClampFloat(amp.Saturation, 0.f, 10.f);
    }

    ExtDenoiseConfig.Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
    ExtDenoiseConfig.Header.BufferSz = sizeof(ExtDenoiseConfig);
    ExtDenoiseConfig.DenoiseFactor = static_cast<mfxU16>( ClampUnsigned(amp.DenoisePercentage, 0, 100) );

    ExtBuffer.clear();
    ExtBuffer.push_back((mfxExtBuffer*)&ExtDoUse);
    ExtBuffer.push_back((mfxExtBuffer*)&ExtDoNotUse);
    ExtBuffer.push_back((mfxExtBuffer*)&ExtDenoiseConfig);
    if (enabled_brightness) {
        ExtBuffer.push_back((mfxExtBuffer*)&ExtProcAmpConfig);
    }

    VPPParams.NumExtParam = static_cast<mfxU16>( ExtBuffer.size() );
    VPPParams.ExtParam = ExtBuffer.data();
    VPPParams.AsyncDepth = 1;

    return true;
}

frameref_t MfxDenoiser::Process(frameref_t input)
{
    // Trigger a reset to recover from decoder errors.
    // Must feed in parameter sets again after a reset.
    if (NeedsReset) {
        spdlog::warn("Reseting encoder on failure");
        mfxStatus status = Vpp->Reset(&VPPParams);
        if (status < MFX_ERR_NONE) {
            spdlog::error("Vpp->Reset failed: {} {}", status, MfxStatusToString(status));
            status = Vpp->Close();
            if (status < MFX_ERR_NONE) {
                spdlog::error("Vpp->Close failed: {} {}", status, MfxStatusToString(status));
            }
            return nullptr;
        }
        NeedsReset = false;
    }

    mfxStatus status;
    mfxSyncPoint sync_point = nullptr;
    frameref_t output_frame;

    while (!Terminated)
    {
        sync_point = nullptr;
        output_frame = Allocator->Allocate();

        status = Vpp->RunFrameVPPAsync(
            &input->Raw->Surface,
            &output_frame->Raw->Surface,
            nullptr,
            &sync_point);

        if (status < MFX_ERR_NONE) {
            spdlog::error("Vpp->RunFrameVPPAsync failed: {} {}", status, MfxStatusToString(status));
            NeedsReset |= MfxStatusInvalidatesCodec(status);
            return nullptr;
        }

        // If a frame is ready:
        if (sync_point) {
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
        spdlog::error("Vpp: Context->Session.SyncOperation failed: {} {}", status, MfxStatusToString(status));
        NeedsReset |= MfxStatusInvalidatesCodec(status);
        return nullptr;
    }

    // Transfer crop to output
    if (output_frame && output_frame->Raw)
    {
        auto& output_info = output_frame->Raw->Surface.Info;
        const auto& input_info = input->Raw->Surface.Info;
        output_info.CropX = input_info.CropX;
        output_info.CropY = input_info.CropY;
        output_info.CropW = input_info.CropW;
        output_info.CropH = input_info.CropH;
    }

    return output_frame;
}


//------------------------------------------------------------------------------
// VideoEncoder

bool VideoEncoder::Initialize(
    std::shared_ptr<BaseAllocator> alloc,
    const EncoderParams& params)
{
    if (!alloc) {
        spdlog::error("Null allocator");
        return false;
    }

    if (params.ProcAmp.Enabled) {
        spdlog::debug("MFX ProcAmp enabled: denoise={}% brightness={} saturation={}",
            params.ProcAmp.DenoisePercentage, params.ProcAmp.Brightness, params.ProcAmp.Saturation);
        Denoiser = std::make_unique<MfxDenoiser>();
        if (!Denoiser->Initialize(alloc, params)) {
            spdlog::error("Failed to initialize MFX Denoiser");
            return false;
        }
    }
    else {
        spdlog::debug("MFX Denoiser disabled");
    }

    Encoder = std::make_unique<MfxEncoder>();
    if (!Encoder->Initialize(alloc, params)) {
        spdlog::error("Failed to initialize MFX Encoder");
        return false;
    }

    return true;
}

void VideoEncoder::Shutdown()
{
    Encoder.reset();
    Denoiser.reset();
}

VideoEncoderOutput VideoEncoder::Encode(
    frameref_t& input,
    bool force_keyframe)
{
    if (!input) {
        spdlog::error("Video encoded input null");
        return VideoEncoderOutput();
    }

    bool success = false;
    if (Denoiser) {
        frameref_t output = Denoiser->Process(input);
        if (output) {
            success = Encoder->Process(output, force_keyframe);
        }
    } else {
        success = Encoder->Process(input, force_keyframe);
    }

    VideoEncoderOutput output{};
    if (success) {
        output.Bytes = Encoder->WrittenBytes;
        output.Data = Encoder->Output.data();
    }
    return output;
}


} // namespace mfx
