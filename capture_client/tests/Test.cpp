// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "capture_client.h"

#include <core_logging.hpp>
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

    SetupAsyncDiskLog("capture_client_test.txt");

    spdlog::info("Capture client test for Azure Kinect DK");

    std::string name = "Test";
    std::string password = "password";
    std::string server = "localhost";
    int port = 28773;

    if (argc <= 5) {
        spdlog::info("Please provide arguments:");
        spdlog::info("    capture_client_test.exe NAME PASSWORD SERVER PORT");
        spdlog::info("Using example:");
        spdlog::info("    capture_client_test.exe \"Test\" password localhost 28773");
    } else {
        name = argv[1];
        password = argv[2];
        server = argv[3];
        port = atoi(argv[4]);
    }

    spdlog::info("Server Name = `{}`", name);
    spdlog::info("Password = `{}`", password);
    spdlog::info("Server Address = `{}`", server);
    spdlog::info("Server Port = `{}`", port);

    xrcap_connect(server.c_str(), port, name.c_str(), password.c_str());

    int frame_number = 0;
    int frame_count = 0;

    while (!Terminated)
    {
        XrcapFrame frame;
        XrcapStatus status;
        xrcap_get(&frame, &status);

        if (frame.Valid && frame_number != frame.FrameNumber)
        {
            frame_number = frame.FrameNumber;
            ++frame_count;
            spdlog::info("Frames {} received.  FrameCount={}", frame_number, frame_count);
        }

        spdlog::info("State:{} Mode:{} CaptureStatus:{} Count:{} Cam0:{} Cam1:{} Cam2:{}",
            xrcap_stream_state_str(status.State),
            xrcap_stream_mode_str(status.Mode),
            xrcap_capture_status_str(status.CaptureStatus),
            status.CameraCount,
            xrcap_camera_code_str(status.CameraCodes[0]),
            xrcap_camera_code_str(status.CameraCodes[1]),
            xrcap_camera_code_str(status.CameraCodes[2]));

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    spdlog::info("Shutting down...");

    uint64_t t0 = GetTimeUsec();
    xrcap_shutdown();
    uint64_t t1 = GetTimeUsec();

    spdlog::info("Shutting down complete in {} msec", (t1 - t0) / 1000.f);

    return CORE_APP_SUCCESS;
}