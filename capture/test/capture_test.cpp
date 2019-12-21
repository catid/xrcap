// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include <CaptureManager.hpp>

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

    SetupAsyncDiskLog("capture_test.txt");

    spdlog::info("Test to make sure capture library builds in isolation");

    RuntimeConfiguration config{};
    config.Mode = CaptureMode::CaptureLowQual;
    config.VideoNeeded = true;
    config.ImagesNeeded = true;

    CaptureManager manager;
    std::atomic<int> FrameCount = ATOMIC_VAR_INIT(0);

    manager.Initialize(&config, [&](std::shared_ptr<ImageBatch>& batch) {
        const int frame_count = ++FrameCount;
        if (frame_count % 100 == 0) {
            spdlog::info("Got batch {}", batch->BatchNumber);
        }
    });

    while (!Terminated)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    manager.Shutdown();

    return CORE_APP_SUCCESS;
}
