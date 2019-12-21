// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Video decoder using Intel QuickSync Video extensions via the Intel Media SDK
    with system memory for input and output.
*/

#pragma once

#include "MfxTools.hpp"
#ifdef _WIN32
#include "MfxDirect3D9.hpp"
#endif

#include <core.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace mfx {


//------------------------------------------------------------------------------
// VideoDecoder

class VideoDecoder
{
public:
    ~VideoDecoder()
    {
        Shutdown();
    }
    void Shutdown();

    // Pass the video parameters and first frame to initialize the decoder.
    bool Initialize(
        bool gpu_output, // output on GPU?
        mfxU32 codec_fourcc,
        const uint8_t* data,
        int bytes);

    frameref_t Decode(const uint8_t* data, int bytes);

    // Allocator to share with encoder if needed
    std::shared_ptr<BaseAllocator> Allocator;

private:
    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);

    std::shared_ptr<MfxContext> Context;

    mfxVideoParam VideoParams{};

    std::unique_ptr<MFXVideoDECODE> MfxDecode;
    bool NeedsReset = false;

    frameref_t FrameRef;
};


} // namespace mfx
