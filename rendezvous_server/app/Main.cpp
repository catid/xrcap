// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "RendezvousServer.hpp"
#include <core_logging.hpp>
#include <tonk.h>
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

    SetupAsyncDiskLog(GetLogFilePath("xrcap", "rendezvous_server"));

    SetTonkLogCallback([](const std::string& msg) {
        spdlog::debug("Tonk: {}", msg);
    });

    spdlog::info("App started: Rendezvous server for capture server/client");

    TonkAddress gateway, host;
    tonk_lan_info(&gateway, &host);

    spdlog::info("Rendezvous server address: {} : {}", host.NetworkString, protos::kRendezvousServerPort);

    RendezvousServer server;

    if (!server.Initialize()) {
        return CORE_APP_FAILURE;
    }

    spdlog::info("Rendezvous server started...");

    while (!Terminated) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info("Rendezvous server shutting down...");

    server.Shutdown();

    return CORE_APP_SUCCESS;
}
