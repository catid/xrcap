// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Windows iGPU surfaces via the Direct3D9 API (DXVA2).

    Based on benchmarking, the D3D9 interface is measurably faster than D3D11.
*/

#pragma once

#ifdef _WIN32

#include "MfxTools.hpp"

#include <core_win32.hpp>
#include <wrl/client.h> /// Microsoft::WRL
#include <initguid.h>

#pragma warning(push)
#pragma warning(disable: 4201)
#include <d3d9.h>
#pragma warning(pop)

#include <dxva2api.h>

namespace mfx {

using namespace Microsoft::WRL;


//------------------------------------------------------------------------------
// Tools

std::string HresultString(HRESULT hr);

// Clears to some solid color
// yuv: Set to true for YUV and false for RGB
bool D3DClearSurface(IDirect3DSurface9* pSurface, bool yuv);

D3DFORMAT D3DFormatFromFourCC(mfxU32 four_cc);


//------------------------------------------------------------------------------
// COMSession

// Each thread that uses COM (Direct3D API) needs to have a COM session that
// lasts for the lifetime of all COM objects
struct COMSession
{
    COMSession()
    {
        Initialize();
    }
    ~COMSession()
    {
        Shutdown();
    }
    void Initialize();
    void Shutdown();
};


//------------------------------------------------------------------------------
// D3D9Context

struct D3D9Context
{
    // This does not modify the context.
    // It is the application's responsibility to make sure the MfxContext
    // goes out of scope after D3D9Context.
    bool Initialize(std::shared_ptr<MfxContext> context);
    void Shutdown();
    ~D3D9Context()
    {
        Shutdown();
    }

    bool Initialized = false;
    bool InitFailed = false;

    mfxHDL ManagerHandle = nullptr; // Reference to the Manager object
    HANDLE DeviceHandle = nullptr;

    ComPtr<IDirect3D9Ex> D3D;
    ComPtr<IDirect3DDevice9Ex> Device;
    ComPtr<IDirect3DDeviceManager9> Manager; // Created with DXVA2 API
    UINT ResetToken = 0;
};


//------------------------------------------------------------------------------
// D3DAllocator

class D3DAllocator;

// This is our user data attached to each allocated surface.
// The allocator mfxMemId is an index into the D3DAllocator::Surfaces array,
// which points to one of these structures.
struct D3DVideoSurface
{
    // This surface handle is allocated via DXVA so we do not need to release it
    IDirect3DSurface9* VideoSurface = nullptr;

    // Shared handle for this surface
    HANDLE SharedHandle = nullptr;

    // Self-checking: Reference to the allocator object for OOP
    D3DAllocator* Allocator = nullptr;

    // Self-checking: Index into the array of surfaces
    mfxMemId Mid = 0;

    // Raw frame object
    rawframe_t Raw;
};

// Allocator object must outlive any allocations
class D3DAllocator : public BaseAllocator
{
public:
    // Note: This modifies the provided context on success to use this object
    // for D3D allocation.
    bool Initialize(
        std::shared_ptr<MfxContext> context,
        const mfxVideoParam& video_params) override;

    void Shutdown() override;
    ~D3DAllocator()
    {
        Shutdown();
    }

    bool Initialized = false;
    bool InitFailed = false;

    // Use by the application to allocate a frame
    frameref_t Allocate() override;

    // Gets a reference to a frame indicated by the MFX API
    frameref_t GetFrameById(mfxMemId mid) override;

    frameref_t CopyToSystemMemory(frameref_t from) override;

protected:
    mfxVideoParam VideoParams{};

    std::shared_ptr<D3D9Context> D3D;

    ComPtr<IDirectXVideoAccelerationService> Service;

    // Lock to ensure thread safety of Surfaces array and allocator initialization
    std::mutex SurfacesLock;
    std::vector<std::shared_ptr<D3DVideoSurface>> Surfaces;

    mfxFrameAllocator Allocator{};
    bool AllocatorInitialized = false;
    unsigned DXVAType = DXVA2_VideoSoftwareRenderTarget;
    bool SharedHandlesEnabled = false;
    D3DFORMAT Format;
    unsigned Width = 0, Height = 0;
    unsigned Usage = 0; // TBD: What is this for?

    // Allocator for system buffers for copy-back
    std::unique_ptr<SystemAllocator> CopyAllocator;


    mfxStatus Alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);
    mfxStatus Free(mfxFrameAllocResponse* response);
    mfxStatus Lock(mfxMemId mid, mfxFrameData* ptr);
    mfxStatus Unlock(mfxMemId mid, mfxFrameData* ptr);
    mfxStatus GetHDL(mfxMemId mid, mfxHDL* handle);

    std::shared_ptr<D3DVideoSurface> GetSurface(mfxMemId mid);

    bool InitializeAllocator(mfxFrameAllocRequest* request);

    inline mfxMemId ArrayIndexToMemId(size_t index)
    {
        return (mfxMemId)(uintptr_t)(index + 1);
    }
    inline unsigned MemIdToArrayIndex(mfxMemId mid)
    {
        return (unsigned)(uintptr_t)mid - 1;
    }
};


} // namespace mfx

#endif // _WIN32
