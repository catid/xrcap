// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "MfxVideoDecoder.hpp"
#include "MfxVideoEncoder.hpp"

#include <core_logging.hpp>
#include <core_mmap.hpp>
using namespace core;


//------------------------------------------------------------------------------
// CTRL+C

#include <csignal>

std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);

void SignalHandler(int)
{
    Terminated = true;
}


//------------------------------------------------------------------------------
// Entrypoint

int main(int argc, char* argv[])
{
    CORE_UNUSED2(argc, argv);

    std::signal(SIGINT, SignalHandler);

    SetupAsyncDiskLog("mfx_test.txt");

    spdlog::info("Test for libmfx jpeg decode, h264 video encoder");

    std::string filename = "input.mjpg";

    if (argc > 1) {
        filename = argv[1];
    }

    MappedReadOnlySmallFile mmf;

    if (!mmf.Read(filename.c_str())) {
        spdlog::error("Failed to open input file: {}", filename);
        return CORE_APP_FAILURE;
    }

    mfx::VideoDecoder decoder;

    const uint64_t ta = GetTimeUsec();

    // Note: Cannot use video memory for HEVC
    if (!decoder.Initialize(false, MFX_CODEC_JPEG, mmf.GetData(), mmf.GetDataBytes()))
    {
        spdlog::error("Failed to initialize video decoder");
        return CORE_APP_FAILURE;
    }

    const uint64_t tb = GetTimeUsec();
    spdlog::info("Encoder initialized in {} msec", (tb - ta) / 1000.f);

    mfx::VideoEncoder encoder;

    bool encoder_initialized = false;

    //std::vector<mfx::frameref_t> decoded_frames;

    for (int i = 0; i < 100; ++i)
    {
        const uint64_t t0 = GetTimeUsec();

        mfx::frameref_t output = decoder.Decode(
            mmf.GetData(),
            mmf.GetDataBytes());

        if (!output) {
            spdlog::error("Failed to decode input file");
            return CORE_APP_FAILURE;
        }

        //decoded_frames.push_back(output);

        const uint64_t t1 = GetTimeUsec();
        spdlog::info("Successfully decoded in {} msec", (t1 - t0) / 1000.f);

        if (!encoder_initialized)
        {
            mfx::EncoderParams params;
            auto& frame_info = output->Raw->Surface.Info;
            params.Width = frame_info.Width;
            params.Height = frame_info.Height;
            params.Framerate = 30;
            params.FourCC = MFX_CODEC_HEVC;
            params.Bitrate = 4000000;
            params.Quality = 25;
            params.ProcAmp.Enabled = true;
            params.ProcAmp.DenoisePercentage = 100;
            params.IntraRefreshCycleSize = 15;
            params.IntraRefreshQPDelta = -5;

            if (!encoder.Initialize(decoder.Allocator, params))
            {
                spdlog::error("Failed to initialize video encoder");
                return CORE_APP_FAILURE;
            }

            const uint64_t t2 = GetTimeUsec();
            spdlog::info("Encoder initialized in {} msec", (t2 - t1) / 1000.f);

            encoder_initialized = true;
        }

        bool keyframe = (i % 6 == 0);

        const uint64_t t3 = GetTimeUsec();

        mfx::VideoEncoderOutput video = encoder.Encode(output, keyframe);
        if (video.Bytes <= 0) {
            spdlog::error("Encode failed");
            return CORE_APP_FAILURE;
        }

        const uint64_t t4 = GetTimeUsec();

        spdlog::info("Successfully encoded in {} msec. keyframe={} size={} bytes", (t4 - t3) / 1000.f, keyframe, video.Bytes);
    }

    encoder.Shutdown();
    decoder.Shutdown();

    return CORE_APP_SUCCESS;
}
