// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Common tools for Intel QuickSync Video extensions via the Intel Media SDK
*/

#pragma once

#include <mfxcommon.h>
#include <mfxvideo++.h>
#include <mfxjpeg.h>

#include <string>

#include <memory>
#include <list>
#include <vector>
#include <mutex>
#include <atomic>

namespace mfx {


//------------------------------------------------------------------------------
// Tools

const char* MfxStatusToString(mfxStatus status);
const char* MfxImplementationToString(int implementation);
const char* MfxChromaFormatToString(int format);

std::string FourCCToString(uint32_t fourcc);

bool MfxStatusInvalidatesCodec(mfxStatus status);

// Round up to the next multiple of 16
inline unsigned RoundUp16(unsigned n) {
    return (n + 15) & ~15;
}


//------------------------------------------------------------------------------
// MfxContext

struct MfxContext
{
    bool Initialize();
    void Shutdown();
    ~MfxContext()
    {
        Shutdown();
    }

    bool Initialized = false;
    bool InitFailed = false;

    MFXVideoSession Session{};
    mfxIMPL Implementation = MFX_IMPL_AUTO;
    bool SupportsJpegDecodeAndLowLatency = false;
    bool SupportsIntraRefresh = false;

    // Can we use D3D9(aka DXVA2) or VAAPI with this context?
    // If false then we need to use system memory allocators
    bool SupportsGpuSurfaces = false;
    int GpuAdapterIndex = -1;
};


//------------------------------------------------------------------------------
// RawFrame

struct RawFrame
{
    mfxFrameSurface1 Surface{};
    std::vector<uint8_t> Data;

    // Number of FrameReference objects that own this raw frame
    std::atomic<int> RefCount = ATOMIC_VAR_INIT(0);

    inline bool IsLocked() const
    {
        if (RefCount > 0) {
            return true;
        }
        const uint16_t is_locked = _InterlockedOr16((short*)&Surface.Data.Locked, 0);
        return (is_locked != 0);
    }
};
using rawframe_t = std::shared_ptr<RawFrame>;

struct FrameReference
{
    FrameReference(rawframe_t& frame);
    ~FrameReference();

    rawframe_t Raw;
};
using frameref_t = std::shared_ptr<FrameReference>;


//------------------------------------------------------------------------------
// BaseAllocator

// Allocator object must outlive any allocations.
// Base class for all other allocators
class BaseAllocator
{
public:
    // Note: This modifies the provided context on success to use this object
    // for D3D allocation.
    virtual bool Initialize(
        std::shared_ptr<MfxContext> context,
        const mfxVideoParam& video_params) = 0;
    virtual void Shutdown() = 0;

    bool IsVideoMemory = false;
    bool Initialized = false;
    bool InitFailed = false;
    std::shared_ptr<MfxContext> Context;

    // Use by the application to allocate a frame
    virtual frameref_t Allocate() = 0;

    // Gets a reference to a frame indicated by the MFX API
    virtual frameref_t GetFrameById(mfxMemId mid) = 0;

    virtual frameref_t CopyToSystemMemory(frameref_t from) = 0;
};


//------------------------------------------------------------------------------
// SystemAllocator

class SystemAllocator : public BaseAllocator
{
public:
    // Returns false if format is not supported.
    // Note this will *modify* the provided context to use this allocator,
    // meaning the context cannot be shared with other MFX objects if
    // the objects have different output allocation needs.
    bool Initialize(
        std::shared_ptr<MfxContext> context,
        const mfxVideoParam& video_params) override;
    void Shutdown() override
    {
    }

    // Initializes an NV12 system memory allocator from scratch, generating a
    // context and mfxVideoParam.  This is a helper function allowing encoding
    // or decoding of data originating from system memory.
    bool InitializeNV12SystemOnly(unsigned width, unsigned height, unsigned framerate);

    // Use by the application to allocate a frame
    frameref_t Allocate() override;

    // Gets a reference to a frame indicated by the MFX API
    frameref_t GetFrameById(mfxMemId mid) override;

    frameref_t CopyToSystemMemory(frameref_t from) override
    {
        return from;
    }

private:
    mfxVideoParam VideoParams{};

    std::mutex Lock;
    std::vector<rawframe_t> RawFrames;


    rawframe_t AllocateNew();
};


} // namespace mfx
