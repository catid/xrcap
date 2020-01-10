// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "ViewerSettings.hpp"

#include <core_mmap.hpp>
#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// Viewer Settings

bool LoadFromFile(const std::string& file_path, ViewerSettings& settings)
{
    MappedReadOnlySmallFile mmf;

    if (!mmf.Read(file_path.c_str())) {
        spdlog::error("Failed to load settings file: {}", file_path);
        return false;
    }

    std::string file_data(reinterpret_cast<const char*>(mmf.GetData()), mmf.GetDataBytes());

    try {
        YAML::Node node = YAML::Load(file_data);

        settings.ServerHostname = node["rendezvous_host"].as<std::string>();
        settings.ServerPort = node["rendezvous_port"].as<int>();
        settings.ServerName = node["name"].as<std::string>();
        settings.ServerPassword = node["password"].as<std::string>();
    } catch (YAML::ParserException& ex) {
        spdlog::error("YAML parse failed: {}", ex.what());
        return false;
    }

    return true;
}

bool SaveToFile(const ViewerSettings& settings, const std::string& file_path)
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "rendezvous_host";
    out << YAML::Value << settings.ServerHostname;
    out << YAML::Key << "rendezvous_port";
    out << YAML::Value << settings.ServerPort;
    out << YAML::Key << "name";
    out << YAML::Value << settings.ServerName;
    out << YAML::Key << "password";
    out << YAML::Value << settings.ServerPassword;
    out << YAML::EndMap;

    if (!out.good()) {
        spdlog::error("Yaml emitter failed: {}", out.GetLastError());
        return false;
    }

    return WriteBufferToFile(file_path.c_str(), out.c_str(), out.size());
}


} // namespace core
