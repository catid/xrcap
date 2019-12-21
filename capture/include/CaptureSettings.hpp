// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "CaptureProtocol.hpp"
#include <yaml-cpp/yaml.h>

namespace core {


//------------------------------------------------------------------------------
// Extrinsics Settings

// e.g. extrinsics_SERIAL.yaml where SERIAL = camera serial
#define CAPTURE_SETTINGS_EXTRINSICS_FORMAT "extrinsics_{}.yaml"

std::string FileNameFromSerial(const std::string& serial);

bool LoadFromFile(const std::string& file_path, protos::CameraExtrinsics& extrinsics);
bool SaveToFile(const protos::CameraExtrinsics& extrinsics, const std::string& file_path);


} // namespace core
