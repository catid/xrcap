// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Video encoder using Intel QuickSync Video extensions via the Intel Media SDK
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
// Params

struct ProcAmpParams
{
    // Enable ProcAmp features?
    bool Enabled = false;

    // Denoising
    unsigned DenoisePercentage = 100; // 0..100 (0 = off)

    // HSL modifications
    float Hue = 0.f; // -180 to 180 (default 0)
    float Saturation = 1.f; // 0.0 to 10.0 (default 1)
    float Brightness = 0.f; // -100.0 to +100.0 (default 0)

    // Contrast adjustment
    float Contrast = 1.f; // 0.0 to 10.0 (default 1)


    inline const bool FloatsEqual(float a, float b)
    {
        const float epsilon = 0.00001f;
        return std::abs(a - b) < epsilon;
    }

    bool operator==(const ProcAmpParams& params)
    {
        return DenoisePercentage == params.DenoisePercentage &&
            FloatsEqual(Hue, params.Hue) &&
            FloatsEqual(Saturation, params.Saturation) &&
            FloatsEqual(Brightness, params.Brightness) &&
            FloatsEqual(Contrast, params.Contrast);
    }
};

struct EncoderParams
{
    uint32_t FourCC = MFX_CODEC_AVC;
    unsigned Bitrate = 5000000;
    unsigned Quality = 25; // 1...51 (1=highest quality), 0=Bitrate only
    unsigned Framerate = 30;

    // Set to non-zero to enable intra-refresh
    unsigned IntraRefreshCycleSize = 15;
    int IntraRefreshQPDelta = -5;

    unsigned Width = 0;
    unsigned Height = 0;

    // If the above parameters are equivalent this returns true:
    bool EncoderParamsEqual(const EncoderParams& params) const
    {
        return FourCC == params.FourCC && Bitrate == params.Bitrate &&
            Quality == params.Quality && Framerate == params.Framerate &&
            IntraRefreshCycleSize == params.IntraRefreshCycleSize &&
            IntraRefreshQPDelta == params.IntraRefreshQPDelta &&
            Width == params.Width && Height == params.Height;
    }

    ProcAmpParams ProcAmp;
};


//------------------------------------------------------------------------------
// MfxEncoder

struct MfxEncoder
{
    ~MfxEncoder()
    {
        Shutdown();
    }
    bool Initialize(
        std::shared_ptr<BaseAllocator> alloc,
        const EncoderParams& params);
    void Shutdown();
    bool Process(frameref_t input, bool force_keyframe);

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);

    std::shared_ptr<MfxContext> Context;
    std::shared_ptr<BaseAllocator> Allocator;

    mfxVideoParam VideoParams{};
    mfxExtCodingOption CodingOptions{};
    mfxExtCodingOption2 CodingOptions2{};
    mfxExtCodingOption3 CodingOptions3{};
    std::vector<mfxExtBuffer*> ExtendedBuffers;

    std::unique_ptr<MFXVideoENCODE> Encoder;
    bool NeedsReset = false;

    std::vector<uint8_t> Output;
    unsigned WrittenBytes = 0;
};


//------------------------------------------------------------------------------
// MfxDenoiser

struct MfxDenoiser
{
    ~MfxDenoiser()
    {
        Shutdown();
    }
    bool Initialize(
        std::shared_ptr<BaseAllocator> alloc,
        const EncoderParams& params);
    bool ChangeProcAmp(const ProcAmpParams& params);
    void Shutdown();
    frameref_t Process(frameref_t input);

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);

    std::shared_ptr<MfxContext> Context;
    std::shared_ptr<BaseAllocator> Allocator;
    EncoderParams InitParams{};

    bool SetupVPPParams(const EncoderParams& params);

    mfxVideoParam VPPParams{};
    mfxExtVPPDoUse ExtDoUse{};
    std::vector<mfxU32> ExtDoUseAlg;
    mfxExtVPPDoNotUse ExtDoNotUse{};
    std::vector<mfxU32> ExtDoNotUseAlg;
    mfxExtVPPDenoise ExtDenoiseConfig{};
    mfxExtVPPProcAmp ExtProcAmpConfig{};
    std::vector<mfxExtBuffer*> ExtBuffer;

    std::unique_ptr<MFXVideoVPP> Vpp;

    bool NeedsReset = false;
};


//------------------------------------------------------------------------------
// VideoEncoder

struct VideoEncoderOutput
{
    uint8_t* Data = nullptr;
    unsigned Bytes = 0;
};

class VideoEncoder
{
public:
    ~VideoEncoder()
    {
        Shutdown();
    }

    bool Initialize(
        std::shared_ptr<BaseAllocator> alloc,
        const EncoderParams& params);
    void Shutdown();

    bool ChangeProcAmp(const ProcAmpParams& params)
    {
        if (!Denoiser) {
            return false; // Unavailable: Return false so encoder will be re-initialized
        }
        return Denoiser->ChangeProcAmp(params);
    }

    mfxVideoParam GetVideoParams() const
    {
        return Encoder->VideoParams;
    }

    VideoEncoderOutput Encode(
        frameref_t& input,
        bool force_keyframe = false);

private:
    std::unique_ptr<MfxDenoiser> Denoiser;
    std::unique_ptr<MfxEncoder> Encoder;
};


} // namespace mfx
