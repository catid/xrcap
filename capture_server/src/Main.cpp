// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureFrontend.hpp"
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

    SetupAsyncDiskLog(GetLogFilePath("xrcap", "capture_server"));

    SetTonkLogCallback([](const std::string& msg) {
        spdlog::debug("Tonk: {}", msg);
    });

    spdlog::info("App started: Multi-camera capture server for Azure Kinect DK");

    CaptureFrontend frontend;
    frontend.Initialize();

    while (!frontend.IsTerminated() && !Terminated) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    frontend.Shutdown();
    return CORE_APP_SUCCESS;
}
