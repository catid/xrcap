// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "CaptureProtocol.hpp"
#include <yaml-cpp/yaml.h>

namespace core {


//------------------------------------------------------------------------------
// Constants

#define CAPTURE_VIEWER_DEFAULT_SETTINGS "ViewerSettings.yaml"


//------------------------------------------------------------------------------
// Viewer Settings

struct ViewerSettings
{
    std::string ServerHostname = "localhost";
    int ServerPort = protos::kRendezvousServerPort;
    std::string ServerName = "Default";
    std::string ServerPassword = "";
};

void operator>>(const YAML::Node& node, ViewerSettings& settings);
void operator>>(ViewerSettings& settings, YAML::Node& node);

bool LoadFromFile(const std::string& file_path, ViewerSettings& settings);
bool SaveToFile(const ViewerSettings& settings, const std::string& file_path);


} // namespace core
