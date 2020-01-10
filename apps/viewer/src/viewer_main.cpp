// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core_logging.hpp"
#include "ViewerWindow.hpp"
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

    SetCurrentThreadName("Main");

    SetupAsyncDiskLog(GetLogFilePath("xrcap", "viewer"));

    spdlog::info("Viewer application");

    std::string file_path;
    if (argc >= 2) {
        file_path = argv[1];
    }

    ViewerWindow window;
    window.Initialize(file_path);

    std::signal(SIGINT, SignalHandler);

    while (!window.IsTerminated() && !Terminated) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    window.Shutdown();

    return CORE_APP_SUCCESS;
}
