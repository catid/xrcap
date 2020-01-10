// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "MfxTools.hpp"

#include <core_logging.hpp>
#include <core_string.hpp>

namespace mfx {


//------------------------------------------------------------------------------
// Tools

const char* MfxStatusToString(mfxStatus status)
{
    switch (status)
    {
    case MFX_ERR_NONE: return "MFX_ERR_NONE";
    case MFX_ERR_UNKNOWN: return "MFX_ERR_UNKNOWN";
    case MFX_ERR_NULL_PTR: return "MFX_ERR_NULL_PTR";
    case MFX_ERR_UNSUPPORTED: return "MFX_ERR_UNSUPPORTED";
    case MFX_ERR_MEMORY_ALLOC: return "MFX_ERR_MEMORY_ALLOC";
    case MFX_ERR_NOT_ENOUGH_BUFFER: return "MFX_ERR_NOT_ENOUGH_BUFFER";
    case MFX_ERR_INVALID_HANDLE: return "MFX_ERR_INVALID_HANDLE";
    case MFX_ERR_LOCK_MEMORY: return "MFX_ERR_LOCK_MEMORY";
    case MFX_ERR_NOT_INITIALIZED: return "MFX_ERR_NOT_INITIALIZED";
    case MFX_ERR_NOT_FOUND: return "MFX_ERR_NOT_FOUND";
    case MFX_ERR_MORE_DATA: return "MFX_ERR_MORE_DATA";
    case MFX_ERR_MORE_SURFACE: return "MFX_ERR_MORE_SURFACE";
    case MFX_ERR_ABORTED: return "MFX_ERR_ABORTED";
    case MFX_ERR_DEVICE_LOST: return "MFX_ERR_DEVICE_LOST";
    case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM: return "MFX_ERR_INCOMPATIBLE_VIDEO_PARAM";
    case MFX_ERR_INVALID_VIDEO_PARAM: return "MFX_ERR_INVALID_VIDEO_PARAM";
    case MFX_ERR_UNDEFINED_BEHAVIOR: return "MFX_ERR_UNDEFINED_BEHAVIOR";
    case MFX_ERR_DEVICE_FAILED: return "MFX_ERR_DEVICE_FAILED";
    case MFX_ERR_MORE_BITSTREAM: return "MFX_ERR_MORE_BITSTREAM";
    case MFX_ERR_INCOMPATIBLE_AUDIO_PARAM: return "MFX_ERR_INCOMPATIBLE_AUDIO_PARAM";
    case MFX_ERR_INVALID_AUDIO_PARAM: return "MFX_ERR_INVALID_AUDIO_PARAM";
    case MFX_ERR_GPU_HANG: return "MFX_ERR_GPU_HANG";
    case MFX_ERR_REALLOC_SURFACE: return "MFX_ERR_REALLOC_SURFACE";
    case MFX_WRN_IN_EXECUTION: return "MFX_WRN_IN_EXECUTION";
    case MFX_WRN_DEVICE_BUSY: return "MFX_WRN_DEVICE_BUSY";
    case MFX_WRN_VIDEO_PARAM_CHANGED: return "MFX_WRN_VIDEO_PARAM_CHANGED";
    case MFX_WRN_PARTIAL_ACCELERATION: return "MFX_WRN_PARTIAL_ACCELERATION";
    case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM: return "MFX_WRN_INCOMPATIBLE_VIDEO_PARAM";
    case MFX_WRN_VALUE_NOT_CHANGED: return "MFX_WRN_VALUE_NOT_CHANGED";
    case MFX_WRN_OUT_OF_RANGE: return "MFX_WRN_OUT_OF_RANGE";
    case MFX_WRN_FILTER_SKIPPED: return "MFX_WRN_FILTER_SKIPPED";
    case MFX_WRN_INCOMPATIBLE_AUDIO_PARAM: return "MFX_WRN_INCOMPATIBLE_AUDIO_PARAM";
    case MFX_TASK_WORKING: return "MFX_TASK_WORKING";
    case MFX_TASK_BUSY: return "MFX_TASK_BUSY";
    case MFX_ERR_MORE_DATA_SUBMIT_TASK: return "MFX_ERR_MORE_DATA_SUBMIT_TASK";
    default: break;
    }
    return "(Unknown code)";
}

const char* MfxImplementationToString(int implementation)
{
    switch (MFX_IMPL_BASETYPE(implementation))
    {
    case MFX_IMPL_AUTO: return "MFX_IMPL_AUTO";
    case MFX_IMPL_SOFTWARE: return "MFX_IMPL_SOFTWARE";
    case MFX_IMPL_HARDWARE: return "MFX_IMPL_HARDWARE";
    case MFX_IMPL_AUTO_ANY: return "MFX_IMPL_AUTO_ANY";
    case MFX_IMPL_HARDWARE_ANY: return "MFX_IMPL_HARDWARE_ANY";
    case MFX_IMPL_HARDWARE2: return "MFX_IMPL_HARDWARE2";
    case MFX_IMPL_HARDWARE3: return "MFX_IMPL_HARDWARE3";
    case MFX_IMPL_HARDWARE4: return "MFX_IMPL_HARDWARE4";
    case MFX_IMPL_RUNTIME: return "MFX_IMPL_RUNTIME";
    default: break;
    }
    return "(Unknown implementation)";
}

const char* MfxChromaFormatToString(int format)
{
    switch (format)
    {
    case MFX_CHROMAFORMAT_MONOCHROME: return "Monochrome";
    case MFX_CHROMAFORMAT_YUV420: return "YUV420";
    case MFX_CHROMAFORMAT_YUV444: return "YUV444";
    case MFX_CHROMAFORMAT_YUV411: return "YUV411";
    case MFX_CHROMAFORMAT_YUV422H: return "YUV422H";
    case MFX_CHROMAFORMAT_YUV422V: return "YUV422V";
    default: break;
    }
    return "(Unknown chroma)";
}

std::string FourCCToString(uint32_t fourcc)
{
    char cstr[5] = {
        static_cast<char>( fourcc & 0xff ),
        static_cast<char>( (fourcc >> 8) & 0xff ),
        static_cast<char>( (fourcc >> 16) & 0xff ),
        static_cast<char>( (fourcc >> 24) & 0xff ),
    };
    return cstr;
}

bool MfxStatusInvalidatesCodec(mfxStatus status)
{
    if (status >= MFX_ERR_NONE) {
        return false;
    }
    switch (status)
    {
    case MFX_ERR_UNKNOWN: return true;
    case MFX_ERR_NULL_PTR: return false;
    case MFX_ERR_UNSUPPORTED: return false;
    case MFX_ERR_MEMORY_ALLOC: return false;
    case MFX_ERR_NOT_ENOUGH_BUFFER: return false;
    case MFX_ERR_INVALID_HANDLE: return false;
    case MFX_ERR_LOCK_MEMORY: return true;
    case MFX_ERR_NOT_INITIALIZED: return true;
    case MFX_ERR_NOT_FOUND: return true;
    case MFX_ERR_MORE_DATA: return false;
    case MFX_ERR_MORE_SURFACE: return false;
    case MFX_ERR_ABORTED: return true;
    case MFX_ERR_DEVICE_LOST: return true;
    case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM: return true;
    case MFX_ERR_INVALID_VIDEO_PARAM: return true;
    case MFX_ERR_UNDEFINED_BEHAVIOR: return true;
    case MFX_ERR_DEVICE_FAILED: return true;
    case MFX_ERR_MORE_BITSTREAM: return false;
    case MFX_ERR_INCOMPATIBLE_AUDIO_PARAM: return false;
    case MFX_ERR_INVALID_AUDIO_PARAM: return false;
    case MFX_ERR_GPU_HANG: return true;
    case MFX_ERR_REALLOC_SURFACE: return false;
    default: break;
    }
    return false;
}


//------------------------------------------------------------------------------
// MfxContext

bool MfxContext::Initialize()
{
    if (Initialized) {
        return true;
    }
    InitFailed = true;

    mfxInitParam init_param{};
    init_param.Version.Major = 1;
    init_param.Version.Minor = 0;
    init_param.GPUCopy = MFX_GPUCOPY_ON;
    init_param.Implementation = MFX_IMPL_AUTO_ANY; // Automatically pick the best

    mfxStatus status = Session.InitEx(init_param);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Session.InitEx failed: {} {}", status, MfxStatusToString(status));
        return false;
    }
    core::ScopedFunction session_scope([this] {
        mfxStatus status = Session.Close();
        if (status < MFX_ERR_NONE) {
            spdlog::warn("Session.Close failed: {} {}", status, MfxStatusToString(status));
        }
    });
    spdlog::debug("New MFX session initialized");

    mfxVersion version{};
    status = Session.QueryVersion(&version);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Session.QueryVersion failed: {} {}", status, MfxStatusToString(status));
        return false;
    }
    spdlog::debug("MFX version: {}.{}", version.Major, version.Minor);

    status = Session.QueryIMPL(&Implementation);
    if (status < MFX_ERR_NONE) {
        spdlog::error("Session.QueryIMPL failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    GpuAdapterIndex = -1;
    switch (MFX_IMPL_BASETYPE(Implementation))
    {
    case MFX_IMPL_HARDWARE: GpuAdapterIndex = 0; break;
    case MFX_IMPL_HARDWARE2: GpuAdapterIndex = 1; break;
    case MFX_IMPL_HARDWARE3: GpuAdapterIndex = 2; break;
    case MFX_IMPL_HARDWARE4: GpuAdapterIndex = 3; break;
    default: break;
    }
    SupportsGpuSurfaces = GpuAdapterIndex >= 0;

    spdlog::debug("MFX Implementation: 0x{} ({}) GpuAdapterIndex={}",
        core::HexString(Implementation),
        MfxImplementationToString(Implementation),
        GpuAdapterIndex);

    spdlog::debug("MFX supports GPU surfaces: {}", SupportsGpuSurfaces);

    SupportsJpegDecodeAndLowLatency = version.Major > 1 || version.Minor >= 3;
    spdlog::debug("MFX supports JPEG decode and Low Latency: {}", SupportsJpegDecodeAndLowLatency);

    SupportsIntraRefresh = version.Major > 1 || version.Minor >= 6;
    spdlog::debug("MFX supports Intra-Refresh: {}", SupportsIntraRefresh);

    status = Session.SetPriority(MFX_PRIORITY_HIGH);
    if (status < MFX_ERR_NONE) {
        spdlog::warn("Session.SetPriority failed: {} {}", status, MfxStatusToString(status));
    }

    session_scope.Cancel();
    Initialized = true;
    InitFailed = false;
    return true;
}

void MfxContext::Shutdown()
{
    if (Initialized) {
        Session.Close();
    }
    Initialized = false;
    InitFailed = false;
}


//------------------------------------------------------------------------------
// FrameReference

FrameReference::FrameReference(rawframe_t& frame)
{
    Raw = frame;
    Raw->RefCount++;
}

FrameReference::~FrameReference()
{
    if (Raw) {
        Raw->RefCount--;
    }
}


//------------------------------------------------------------------------------
// SystemAllocator

bool SystemAllocator::InitializeNV12SystemOnly(unsigned width, unsigned height, unsigned framerate)
{
    Context = std::make_shared<MfxContext>();
    if (!Context->Initialize()) {
        return false;
    }

    mfxVideoParam video_param{};
    auto& frame_info = video_param.mfx.FrameInfo;
    frame_info.AspectRatioH = 0;
    frame_info.AspectRatioW = 0;
    frame_info.BitDepthChroma = 8;
    frame_info.BitDepthLuma = 8;
    frame_info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    frame_info.CropW = static_cast<mfxU16>( width );
    frame_info.CropH = static_cast<mfxU16>( height );
    frame_info.CropX = 0;
    frame_info.CropY = 0;
    frame_info.FourCC = MFX_FOURCC_NV12;
    frame_info.FrameRateExtN = static_cast<mfxU32>( framerate );
    frame_info.FrameRateExtD = 1;
    frame_info.Width = static_cast<mfxU16>( mfx::RoundUp16(width) );
    frame_info.Height = static_cast<mfxU16>( mfx::RoundUp16(height) );
    frame_info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    return Initialize(Context, video_param);
}

bool SystemAllocator::Initialize(
    std::shared_ptr<MfxContext> context,
    const mfxVideoParam& video_params)
{
    Context = context;
    VideoParams = video_params;
    IsVideoMemory = false;
    InitFailed = true;

    const auto& info = VideoParams.mfx.FrameInfo;

    switch (info.FourCC)
    {
    case MFX_FOURCC_NV12:
        break;
    default:
        spdlog::error("FIXME: Unsupported FourCC={}", info.FourCC);
        return false;
    }

    const unsigned preallocate_count = 2 + VideoParams.AsyncDepth;
    for (unsigned i = 0; i < preallocate_count; ++i) {
        AllocateNew();
    }

    InitFailed = false;
    Initialized = true;
    return true;
}

frameref_t SystemAllocator::Allocate()
{
    std::lock_guard<std::mutex> locker(Lock);

    for (auto& frame : RawFrames) {
        if (!frame->IsLocked()) {
            return std::make_shared<FrameReference>( frame );
        }
    }

    rawframe_t frame = AllocateNew();
    return std::make_shared<FrameReference>( frame );
}

rawframe_t SystemAllocator::AllocateNew()
{
    rawframe_t frame = std::make_shared<RawFrame>();

    const auto& info = VideoParams.mfx.FrameInfo;
    frame->Surface.Info = info;

    unsigned Width2 = RoundUp16(info.Width);
    unsigned Height2 = RoundUp16(info.Height);
    unsigned PlaneBytes = Width2 * Height2;
    unsigned ImageBytes = PlaneBytes * 3 / 2;

    frame->Data.resize(ImageBytes);
    uint8_t* y_plane = frame->Data.data();

    auto& data = frame->Surface.Data;
    data.Y = y_plane;
    data.UV = data.Y + PlaneBytes;
    data.V = data.UV + 1;
    data.A = nullptr;
    data.Pitch = static_cast<mfxU16>( Width2 );

    const unsigned frame_index = static_cast<unsigned>( RawFrames.size() );

    RawFrames.push_back(frame);
    frame->Surface.Data.MemId = (mfxMemId)(uintptr_t)frame_index;

    return frame;
}

frameref_t SystemAllocator::GetFrameById(mfxMemId mid)
{
    std::lock_guard<std::mutex> locker(Lock);

    const unsigned index = static_cast<unsigned>( (uintptr_t)mid );
    if (index >= RawFrames.size()) {
        return nullptr;
    }

    auto& raw_frame = RawFrames[index];
    return std::make_shared<FrameReference>( raw_frame );
}


} // namespace mfx
