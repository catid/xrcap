// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#ifdef _WIN32

#include "MfxDirect3D9.hpp"

#include <core_logging.hpp>
#include <core_string.hpp>

#include <comdef.h>

#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y','V','1','2')
#define D3DFMT_NV16 (D3DFORMAT)MAKEFOURCC('N','V','1','6')
#define D3DFMT_P010 (D3DFORMAT)MAKEFOURCC('P','0','1','0')
#define D3DFMT_P210 (D3DFORMAT)MAKEFOURCC('P','2','1','0')
#define D3DFMT_IMC3 (D3DFORMAT)MAKEFOURCC('I','M','C','3')
#define D3DFMT_AYUV (D3DFORMAT)MAKEFOURCC('A','Y','U','V')
#if (MFX_VERSION >= 1027)
#define D3DFMT_Y210 (D3DFORMAT)MAKEFOURCC('Y','2','1','0')
#define D3DFMT_Y410 (D3DFORMAT)MAKEFOURCC('Y','4','1','0')
#endif

#define MFX_FOURCC_IMC3 (MFX_MAKEFOURCC('I','M','C','3')) // This line should be moved into mfxstructures.h in new API version

namespace mfx {


//------------------------------------------------------------------------------
// Tools

std::string HresultString(HRESULT hr)
{
    std::ostringstream oss;
    oss << _com_error(hr).ErrorMessage() << " [hr=" << hr << "]";
    return oss.str();
}

bool D3DClearSurface(IDirect3DSurface9* pSurface, bool yuv)
{
    D3DSURFACE_DESC desc{};
    HRESULT hr = pSurface->GetDesc(&desc);
    if (FAILED(hr)) {
        spdlog::error("D3DClearRGBSurface: pSurface->GetDesc failed: {}", HresultString(hr));
        return false;
    }

    D3DLOCKED_RECT locked{};
    hr = pSurface->LockRect(&locked, 0, D3DLOCK_NOSYSLOCK);
    if (FAILED(hr)) {
        spdlog::error("D3DClearRGBSurface: pSurface->LockRect failed: {}", HresultString(hr));
        return false;
    }

    const unsigned plane_bytes = desc.Height * locked.Pitch;
    memset(locked.pBits, 100, plane_bytes);
    if (yuv) {
        // Clear UV plane also
        memset((mfxU8*)locked.pBits + plane_bytes, 50, plane_bytes / 2);
    }

    pSurface->UnlockRect();
    if (FAILED(hr)) {
        spdlog::error("D3DClearRGBSurface: pSurface->UnlockRect failed: {}", HresultString(hr));
        return false;
    }

    return true;
}

D3DFORMAT D3DFormatFromFourCC(mfxU32 four_cc)
{
    switch (four_cc)
    {
    case MFX_FOURCC_NV12:
        return D3DFMT_NV12;
    case MFX_FOURCC_YV12:
        return D3DFMT_YV12;
    case MFX_FOURCC_NV16:
        return D3DFMT_NV16;
    case MFX_FOURCC_YUY2:
        return D3DFMT_YUY2;
    case MFX_FOURCC_RGB3:
        return D3DFMT_R8G8B8;
    case MFX_FOURCC_RGB4:
        return D3DFMT_A8R8G8B8;
    case MFX_FOURCC_P8:
        return D3DFMT_P8;
    case MFX_FOURCC_P010:
        return D3DFMT_P010;
    case MFX_FOURCC_AYUV:
        return D3DFMT_AYUV;
    case MFX_FOURCC_P210:
        return D3DFMT_P210;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
        return D3DFMT_Y210;
    case MFX_FOURCC_Y410:
        return D3DFMT_Y410;
#endif
    case MFX_FOURCC_A2RGB10:
        return D3DFMT_A2R10G10B10;
    case MFX_FOURCC_ABGR16:
    case MFX_FOURCC_ARGB16:
        return D3DFMT_A16B16G16R16;
    case MFX_FOURCC_IMC3:
        return D3DFMT_IMC3;
    default:
        return D3DFMT_UNKNOWN;
    }
}


//------------------------------------------------------------------------------
// COMSession

void COMSession::Initialize()
{
    HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        spdlog::warn("Failed to start COM session: Error = {}", hr);
    }
}

void COMSession::Shutdown()
{
    CoUninitialize();
}


//------------------------------------------------------------------------------
// D3D9Context

bool D3D9Context::Initialize(std::shared_ptr<MfxContext> context)
{
    Shutdown();

    InitFailed = true;

    if (!context->SupportsGpuSurfaces) {
        spdlog::error("MFX context does not support GPU surfaces: Must use system memory on this platform.");
        return false;
    }

    HRESULT hr;

    hr = ::Direct3DCreate9Ex(D3D_SDK_VERSION, D3D.GetAddressOf());
    if (FAILED(hr)) {
        spdlog::error("D3D9Context creation failed: Direct3DCreate9Ex failed: {}", HresultString(hr));
        return false;
    }

    const HWND desktop_window = GetDesktopWindow();

    D3DPRESENT_PARAMETERS present_params{};
    present_params.Windowed = true;
    present_params.hDeviceWindow = desktop_window;
    present_params.Flags = D3DPRESENTFLAG_VIDEO;
    present_params.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    present_params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // note that this setting leads to an implicit timeBeginPeriod call
    present_params.BackBufferCount = 1;
    present_params.BackBufferFormat = D3DFMT_A8R8G8B8;
    present_params.BackBufferWidth = 512;
    present_params.BackBufferHeight = 512;
    present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;

    // Mark the back buffer lockable if software DXVA2 could be used.
    // This is because software DXVA2 device requires a lockable render target
    // for the optimal performance.
    present_params.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

    hr = D3D->CreateDeviceEx(context->GpuAdapterIndex,
                             D3DDEVTYPE_HAL,
                             desktop_window,
                             D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                             &present_params,
                             nullptr,
                             Device.GetAddressOf());
    if (FAILED(hr)) {
        spdlog::error("D3D9Context creation failed: D3D->CreateDeviceEx failed: {}", HresultString(hr));
        return false;
    }

    hr = Device->ResetEx(&present_params, nullptr);
    if (FAILED(hr)) {
        spdlog::warn("D3D9Context creation warning: Device->ResetEx failed: {}", HresultString(hr));
        //return false;
    }

    hr = Device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr)) {
        spdlog::warn("D3D9Context creation warning: Device->Clear failed: {}", HresultString(hr));
        //return false;
    }

    hr = DXVA2CreateDirect3DDeviceManager9(&ResetToken, Manager.GetAddressOf());
    if (FAILED(hr)) {
        spdlog::error("D3D9Context creation failed: DXVA2CreateDirect3DDeviceManager9 failed: {}", HresultString(hr));
        return false;
    }

    ManagerHandle = (mfxHDL)Manager.Get();

    hr = Manager->ResetDevice(Device.Get(), ResetToken);
    if (FAILED(hr)) {
        spdlog::error("D3D9Context creation failed: Manager->ResetDevice failed: {}", HresultString(hr));
        return false;
    }

    hr = Manager->OpenDeviceHandle(&DeviceHandle);
    if (FAILED(hr)) {
        spdlog::error("D3D9Context creation failed: Manager->OpenDeviceHandle failed: {}", HresultString(hr));
        return false;
    }

    Initialized = true;
    InitFailed = false;
    return true;
}

void D3D9Context::Shutdown()
{
    if (Manager && DeviceHandle) {
        Manager->CloseDeviceHandle(DeviceHandle);
        DeviceHandle = nullptr;
    }
    ManagerHandle = nullptr;
    Manager.Reset();
    Device.Reset();
    D3D.Reset();
    Initialized = false;
    InitFailed = false;
}


//------------------------------------------------------------------------------
// D3DAllocator

bool D3DAllocator::Initialize(
    std::shared_ptr<MfxContext> context,
    const mfxVideoParam& video_params)
{
    Context = context;
    VideoParams = video_params;
    InitFailed = true;
    IsVideoMemory = true;

    CopyAllocator = std::make_unique<SystemAllocator>();
    if (!CopyAllocator->Initialize(Context, VideoParams)) {
        spdlog::error("D3DAllocator init failed: CopyAllocator->Initialize failed");
        return false;
    }

    D3D = std::make_shared<D3D9Context>();
    if (!D3D->Initialize(context)) {
        spdlog::error("D3DAllocator init failed: D3D->Initialize failed");
        return false;
    }

    mfxStatus status = Context->Session.SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, D3D->ManagerHandle);
    if (status < MFX_ERR_NONE) {
        spdlog::error("D3DAllocator init failed: Context->Session.SetHandle failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    Allocator.pthis = this;
    Allocator.Alloc = [](mfxHDL pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response) -> mfxStatus
    {
        D3DAllocator* thiz = reinterpret_cast<D3DAllocator*>(pthis);
        return thiz->Alloc(request, response);
    };
    Allocator.Free = [](mfxHDL pthis, mfxFrameAllocResponse* response) -> mfxStatus
    {
        D3DAllocator* thiz = reinterpret_cast<D3DAllocator*>(pthis);
        return thiz->Free(response);
    };
    Allocator.Lock = [](mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr) -> mfxStatus
    {
        D3DAllocator* thiz = reinterpret_cast<D3DAllocator*>(pthis);
        return thiz->Lock(mid, ptr);
    };
    Allocator.Unlock = [](mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr) -> mfxStatus
    {
        D3DAllocator* thiz = reinterpret_cast<D3DAllocator*>(pthis);
        return thiz->Unlock(mid, ptr);
    };
    Allocator.GetHDL = [](mfxHDL pthis, mfxMemId mid, mfxHDL* handle) -> mfxStatus
    {
        D3DAllocator* thiz = reinterpret_cast<D3DAllocator*>(pthis);
        return thiz->GetHDL(mid, handle);
    };

    status = Context->Session.SetFrameAllocator(&Allocator);
    if (status < MFX_ERR_NONE) {
        spdlog::error("D3DAllocator init failed: Mfx->Session.SetFrameAllocator failed: {} {}", status, MfxStatusToString(status));
        return false;
    }

    Initialized = true;
    InitFailed = false;
    return true;
}

void D3DAllocator::Shutdown()
{
    Service.Reset();
    D3D.reset();
    CopyAllocator.reset();
    Context.reset();
    AllocatorInitialized = false;
    Initialized = false;
    InitFailed = false;
}

mfxStatus D3DAllocator::Alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
{
    std::lock_guard<std::mutex> locker(SurfacesLock);

    // Note: EncRequest.Type |= WILL_WRITE; // This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application

    if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) {
        return MFX_ERR_UNSUPPORTED;
    }
    if (0 != (request->Type & MFX_MEMTYPE_FROM_ENCODE)) {
        spdlog::warn("Refusing to allocate encoder output with D3D: Expecting this in system memory");
        return MFX_ERR_UNSUPPORTED;
    }

    // Ensure the allocator is initialized
    if (!InitializeAllocator(request)) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    unsigned needed_count = request->NumFrameSuggested;
    if (needed_count < 1) {
        needed_count = 1;
    }

    // Allocate array (freed in Free() below)
    mfxMemId* mids = (mfxMemId*)calloc(needed_count, sizeof(mfxMemId));
    if (!mids) {
        spdlog::error("calloc returned null");
        return MFX_ERR_MEMORY_ALLOC;
    }
    core::ScopedFunction mids_scope([mids]() {
        free(mids);
    });
    unsigned mid_write_next = 0;

    const unsigned surfaces_size = static_cast<unsigned>( Surfaces.size() );
    for (unsigned i = 0; i < surfaces_size; ++i) {
        if (Surfaces[i]->Raw->IsLocked()) {
            continue;
        }
        mids[mid_write_next++] = Surfaces[i]->Mid;
        if (mid_write_next >= needed_count) {
            break;
        }
    }

    const unsigned allocate_count = needed_count - mid_write_next;
    if (allocate_count > 0)
    {
        const unsigned first_new_offset = static_cast<unsigned>( Surfaces.size() );

        if (SharedHandlesEnabled)
        {
            for (unsigned i = 0; i < allocate_count; ++i)
            {
                const unsigned array_index = first_new_offset + i;
                const mfxMemId mid = ArrayIndexToMemId(array_index);

                std::shared_ptr<D3DVideoSurface> surface = std::make_shared<D3DVideoSurface>();
                surface->Allocator = this;
                surface->Mid = mid;

                HRESULT hr = Service->CreateSurface(
                    Width,
                    Height,
                    0, // backbuffers
                    Format,
                    D3DPOOL_DEFAULT,
                    Usage,
                    DXVAType,
                    &surface->VideoSurface,
                    &surface->SharedHandle);
                if (FAILED(hr) || !surface->VideoSurface || !surface->SharedHandle) {
                    spdlog::error("D3DAllocator: Service->CreateSurface failed: {}", HresultString(hr));
                    return MFX_ERR_MEMORY_ALLOC;
                }

                const auto& info = VideoParams.mfx.FrameInfo;
                surface->Raw = std::make_shared<RawFrame>();
                surface->Raw->Surface.Info = info;
                surface->Raw->Surface.Data.MemId = mid;

                // Remember surface even if we end up not using it
                Surfaces.push_back(surface);
                mids[mid_write_next++] = mid;
            }
        }
        else
        {
            std::vector<IDirect3DSurface9*> surfs(allocate_count);

            HRESULT hr = Service->CreateSurface(
                Width,
                Height,
                allocate_count - 1, // backbuffers
                Format,
                D3DPOOL_DEFAULT,
                Usage,
                DXVAType,
                surfs.data(),
                nullptr);
            if (FAILED(hr)) {
                spdlog::error("D3DAllocator: Service->CreateSurface N={} failed: {}", allocate_count, HresultString(hr));
                return MFX_ERR_MEMORY_ALLOC;
            }

            for (unsigned i = 0; i < allocate_count; ++i)
            {
                const unsigned array_index = first_new_offset + i;
                const mfxMemId mid = ArrayIndexToMemId(array_index);

                std::shared_ptr<D3DVideoSurface> surface = std::make_shared<D3DVideoSurface>();
                surface->Allocator = this;
                surface->Mid = mid;
                surface->VideoSurface = surfs[i];

                const auto& info = VideoParams.mfx.FrameInfo;
                surface->Raw = std::shared_ptr<RawFrame>();
                surface->Raw->Surface.Info = info;
                surface->Raw->Surface.Data.MemId = mid;

                // Remember surface even if we end up not using it
                Surfaces.push_back(surface);
                mids[mid_write_next++] = mid;
            }
        }

        spdlog::debug("Allocated {} D3D surfaces: shared={}", allocate_count, SharedHandlesEnabled);
    }

    response->mids = mids;
    response->NumFrameActual = static_cast<mfxU16>( needed_count );
    mids_scope.Cancel();
    return MFX_ERR_NONE;
}

bool D3DAllocator::InitializeAllocator(mfxFrameAllocRequest* request)
{
    unsigned dxva_type = 0;
    if (0 != (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET & request->Type)) {
        dxva_type = DXVA2_VideoProcessorRenderTarget;
    }
    else if (0 != (MFX_MEMTYPE_DXVA2_DECODER_TARGET & request->Type)) {
        dxva_type = DXVA2_VideoDecoderRenderTarget;
    }
    else {
        spdlog::error("D3DAllocator: Request type unsupported: {}", request->Type);
        return false;
    }

    const D3DFORMAT format = D3DFormatFromFourCC(request->Info.FourCC);

    const bool shared_handles_enabled = (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) != 0;

    const unsigned width = request->Info.Width;
    const unsigned height = request->Info.Height;

    if (AllocatorInitialized) {
        if (dxva_type != DXVAType) {
            spdlog::error("D3DAllocator: DXVA type request mismatch {} != {}", dxva_type, DXVAType);
            return false;
        }
        if (format != Format) {
            spdlog::error("D3DAllocator: DXVA format request mismatch {} != {}", format, Format);
            return false;
        }
        if (shared_handles_enabled != SharedHandlesEnabled) {
            spdlog::error("D3DAllocator: SharedHandlesEnabled mismatch");
            return false;
        }
        if (width != Width || height != Height) {
            spdlog::error("D3DAllocator: Resolution mismatch {}x{} != {}x{}", width, height, Width, Height);
            return false;
        }
        return true;
    }

    if (!D3D) {
        spdlog::error("D3DAllocator init failed: D3D is null");
        return false;
    }

    Format = format;
    DXVAType = dxva_type;
    SharedHandlesEnabled = shared_handles_enabled;
    Width = width;
    Height = height;

    IID service_id = IID_IDirectXVideoDecoderService;
    if (dxva_type == DXVA2_VideoProcessorRenderTarget) {
        service_id = IID_IDirectXVideoProcessorService;
    }

    HRESULT hr = D3D->Manager->GetVideoService(
        D3D->DeviceHandle,
        service_id,
        (void**)Service.GetAddressOf());
    if (FAILED(hr)) {
        spdlog::error("D3DAllocator init failed: Manager->OpenDeviceHandle failed: {}", HresultString(hr));
        return false;
    }

    spdlog::debug("Initialized D3D9 video service for allocations");

    AllocatorInitialized = true;
    return true;
}

mfxStatus D3DAllocator::Free(mfxFrameAllocResponse* response)
{
    // Be lenient with API usage
    if (!response || !response->mids || response->NumFrameActual <= 0) {
        return MFX_ERR_NONE;
    }

    // We allocated the `mids` array so we will need to free() it
    core::ScopedFunction surfaces_scope([response]() {
        free(response->mids);
    });

    const unsigned surface_count = response->NumFrameActual;
    for (unsigned i = 0; i < surface_count; ++i)
    {
        std::shared_ptr<D3DVideoSurface> surface = GetSurface(response->mids[i]);
        if (!surface) {
            continue;
        }

        // Error checking
        if (surface->Allocator != this || surface->Mid != response->mids[i]) {
            spdlog::error("D3DAllocator: Surface does not match allocator: Stale pointer?");
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        if (surface->Raw->RefCount <= 0) {
            spdlog::error("D3DAllocator: Surface double-free detected!");
            continue;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DAllocator::Lock(mfxMemId mid, mfxFrameData* ptr)
{
    std::shared_ptr<D3DVideoSurface> surface = GetSurface(mid);
    if (!surface) {
        return MFX_ERR_INVALID_HANDLE;
    }

    if (!ptr) {
        spdlog::error("D3DAllocator: Lock ptr == null");
        return MFX_ERR_LOCK_MEMORY;
    }

    D3DSURFACE_DESC desc{};
    HRESULT hr = surface->VideoSurface->GetDesc(&desc);
    if (FAILED(hr)) {
        spdlog::error("D3DAllocator: pSurface->GetDesc failed: {}", HresultString(hr));
        return MFX_ERR_LOCK_MEMORY;
    }

    D3DLOCKED_RECT locked{};
    hr = surface->VideoSurface->LockRect(&locked, 0, D3DLOCK_NOSYSLOCK);
    if (FAILED(hr)) {
        spdlog::error("D3DAllocator: pSurface->LockRect failed: {}", HresultString(hr));
        return MFX_ERR_LOCK_MEMORY;
    }

    switch ((DWORD)desc.Format)
    {
    case D3DFMT_NV12:
    case D3DFMT_P010:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = (mfxU8 *)locked.pBits + desc.Height * locked.Pitch;
        ptr->V = (desc.Format == D3DFMT_P010) ? ptr->U + 2 : ptr->U + 1;
        break;
    case D3DFMT_YV12:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->V = ptr->Y + desc.Height * locked.Pitch;
        ptr->U = ptr->V + (desc.Height * locked.Pitch) / 4;
        break;
    case D3DFMT_YUY2:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        break;
    case D3DFMT_R8G8B8:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        break;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_A2R10G10B10:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case D3DFMT_P8:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = nullptr;
        ptr->V = nullptr;
        break;
    case D3DFMT_A16B16G16R16:
        ptr->V16 = (mfxU16*)locked.pBits;
        ptr->U16 = ptr->V16 + 1;
        ptr->Y16 = ptr->V16 + 2;
        ptr->A = (mfxU8*)(ptr->V16 + 3);
        ptr->PitchHigh = (mfxU16)((mfxU32)locked.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)((mfxU32)locked.Pitch % (1 << 16));
        break;
    case D3DFMT_IMC3:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->V = ptr->Y + desc.Height * locked.Pitch;
        ptr->U = ptr->Y + desc.Height * locked.Pitch *2;
        break;
    case D3DFMT_AYUV:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->V = (mfxU8 *)locked.pBits;
        ptr->U = ptr->V + 1;
        ptr->Y = ptr->V + 2;
        ptr->A = ptr->V + 3;
        break;
#if (MFX_VERSION >= 1027)
    case D3DFMT_Y210:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y16 = (mfxU16 *)locked.pBits;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->Y16 + 3;
        break;
    case D3DFMT_Y410:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y410 = (mfxY410 *)locked.pBits;
        ptr->Y = nullptr;
        ptr->V = nullptr;
        ptr->A = nullptr;
        break;
#endif
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DAllocator::Unlock(mfxMemId mid, mfxFrameData* ptr)
{
    std::shared_ptr<D3DVideoSurface> surface = GetSurface(mid);
    if (!surface) {
        return MFX_ERR_INVALID_HANDLE;
    }

    HRESULT hr = surface->VideoSurface->UnlockRect();
    if (FAILED(hr)) {
        spdlog::warn("D3DAllocator: pSurface->UnlockRect failed: {}", HresultString(hr));
        //return MFX_ERR_LOCK_MEMORY;
    }

    // Clear image data references
    if (ptr) {
        ptr->Pitch = 0;
        ptr->R = nullptr;
        ptr->G = nullptr;
        ptr->B = nullptr;
        ptr->A = nullptr;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DAllocator::GetHDL(mfxMemId mid, mfxHDL* ptr)
{
    if (!ptr) {
        spdlog::error("D3DAllocator: GetHDL ptr == null");
        return MFX_ERR_INVALID_HANDLE;
    }

    std::shared_ptr<D3DVideoSurface> surface = GetSurface(mid);
    if (!surface) {
        return MFX_ERR_INVALID_HANDLE;
    }

    *ptr = (mfxHDL)surface->VideoSurface;
    return MFX_ERR_NONE;
}

std::shared_ptr<D3DVideoSurface> D3DAllocator::GetSurface(mfxMemId mid)
{
    if (!mid) {
        return nullptr;
    }

    const unsigned index = (unsigned)(uintptr_t)mid;

    std::lock_guard<std::mutex> locker(SurfacesLock);

    if (index > Surfaces.size()) {
        spdlog::error("D3DAllocator: GetSurface index out of bounds: mid={}", index);
        return nullptr;
    }
    return Surfaces[index - 1];
}

frameref_t D3DAllocator::Allocate()
{
    std::lock_guard<std::mutex> locker(SurfacesLock);

    const unsigned surfaces_size = static_cast<unsigned>( Surfaces.size() );
    for (unsigned i = 0; i < surfaces_size; ++i) {
        auto& surface = Surfaces[i];

        if (surface->Raw->IsLocked()) {
            continue;
        }

        // Reset CopyToSystemMemory
        surface->Raw->Surface.Data.Y = nullptr;

        // This increments the reference count
        return std::make_shared<FrameReference>(surface->Raw);
    }

    // FIXME: If we can combine all these then refactor later

    const unsigned array_index = static_cast<unsigned>( Surfaces.size() );
    const mfxMemId mid = ArrayIndexToMemId(array_index);

    std::shared_ptr<D3DVideoSurface> surface = std::make_shared<D3DVideoSurface>();
    surface->Allocator = this;
    surface->Mid = mid;

    HRESULT hr = Service->CreateSurface(
        Width,
        Height,
        0, // backbuffers
        Format,
        D3DPOOL_DEFAULT,
        Usage,
        DXVAType,
        &surface->VideoSurface,
        SharedHandlesEnabled ? &surface->SharedHandle : nullptr);
    if (FAILED(hr) || !surface->VideoSurface) {
        spdlog::error("D3DAllocator: Service->CreateSurface failed: {}", HresultString(hr));
        return nullptr;
    }

    const auto& info = VideoParams.mfx.FrameInfo;
    surface->Raw = std::make_shared<RawFrame>();
    surface->Raw->Surface.Info = info;
    surface->Raw->Surface.Data.MemId = mid;

    Surfaces.push_back(surface);

    // This increments the reference count
    return std::make_shared<FrameReference>(surface->Raw);
}

frameref_t D3DAllocator::GetFrameById(mfxMemId mid)
{
    std::shared_ptr<D3DVideoSurface> surface = GetSurface(mid);
    if (!surface) {
        return nullptr;
    }

    // Reset CopyToSystemMemory
    surface->Raw->Surface.Data.Y = nullptr;

    // This increments the reference count
    return std::make_shared<FrameReference>(surface->Raw);
}

frameref_t D3DAllocator::CopyToSystemMemory(mfx::frameref_t input_frame)
{
    if (!input_frame) {
        return nullptr;
    }
    rawframe_t& input_raw = input_frame->Raw;
    if (!input_raw) {
        return nullptr;
    }

    frameref_t output_frame = CopyAllocator->Allocate();
    if (!output_frame) {
        return nullptr;
    }
    rawframe_t& output_raw = output_frame->Raw;
    if (!output_raw) {
        return nullptr;
    }

    const mfxMemId mid = input_raw->Surface.Data.MemId;

    mfxFrameData data{};
    mfxStatus status = Lock(mid, &data);
    if (status < MFX_ERR_NONE) {
        return false;
    }
    core::ScopedFunction lock_scope([this, mid, &data]() {
        Unlock(mid, &data);
    });

    auto& info = input_raw->Surface.Info;
    const unsigned pitch = data.Pitch;
    const unsigned width = info.CropW;
    const unsigned height = info.CropH;
    const unsigned plane_bytes = width * height;

    if (pitch != width) {
        spdlog::error("D3D format pitch={} width={} unsupported", pitch, width);
        return false;
    }
    if (info.FourCC != MFX_FOURCC_NV12) {
        spdlog::error("D3D non-NV12 format unsupported: {}", info.FourCC);
        return false;
    }

    uint8_t* dest = output_raw->Data.data();
    memcpy(dest, data.Y, plane_bytes);
    memcpy(dest + plane_bytes, data.UV, plane_bytes / 2);

    return output_frame;
}


} // namespace mfx

#endif // _WIN32
