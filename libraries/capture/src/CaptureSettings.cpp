// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureSettings.hpp"

#include <fmt/format.h>

#include <core_mmap.hpp>
#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// Capture Settings

std::string FileNameFromSerial(const std::string& serial)
{
    return fmt::format(CAPTURE_SETTINGS_EXTRINSICS_FORMAT, serial);
}

bool LoadFromFile(const std::string& file_path, protos::CameraExtrinsics& extrinsics)
{
    MappedReadOnlySmallFile mmf;

    if (!mmf.Read(file_path.c_str())) {
        spdlog::warn("Failed to load capture settings file: {}", file_path);
        return false;
    }

    std::string file_data(reinterpret_cast<const char*>(mmf.GetData()), mmf.GetDataBytes());

    try {
        YAML::Node node = YAML::Load(file_data);

        extrinsics.IsIdentity = node["identity"].as<bool>(false) ? 1 : 0;
        if (!extrinsics.IsIdentity) {
            extrinsics.Transform = node["transform"].as<std::array<float, 16>>();
        }
    } catch (YAML::Exception& ex) {
        spdlog::error("YAML parse for capture settings {} failed: {}", file_path, ex.what());
        return false;
    }

    return true;
}

bool SaveToFile(const protos::CameraExtrinsics& extrinsics, const std::string& file_path)
{
    std::vector<float> adapter(16);
    for (int i = 0; i < 16; ++i) {
        adapter[i] = extrinsics.Transform[i];
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "identity";
    out << YAML::Value << (extrinsics.IsIdentity != 0);
    if (!extrinsics.IsIdentity) {
        out << YAML::Key << "transform";
        out << YAML::Value << adapter;
    }
    out << YAML::EndMap;

    if (!out.good()) {
        spdlog::error("Yaml emitter for capture settings {} failed: {}", file_path, out.GetLastError());
        return false;
    }

    return WriteBufferToFile(file_path.c_str(), out.c_str(), out.size());
}


} // namespace core
