// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "CaptureProtocol.hpp"
#include <yaml-cpp/yaml.h>

namespace core {


//------------------------------------------------------------------------------
// Constants

#define CAPTURE_SERVER_DEFAULT_SETTINGS "ServerSettings.yaml"


//------------------------------------------------------------------------------
// Server Settings

struct ServerSettings
{
    int ServerUdpPort = protos::kCaptureServerPort;
    std::string RendezvousServerHostname = "localhost";
    int RendezvousServerPort = protos::kRendezvousServerPort;
    std::string ServerName = "Default";
    std::string ServerPasswordHash = "";
    bool EnableMultiServers = false;
};

bool LoadFromFile(const std::string& file_path, ServerSettings& settings);
bool SaveToFile(const ServerSettings& settings, const std::string& file_path);


} // namespace core
