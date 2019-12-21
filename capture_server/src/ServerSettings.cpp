// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "ServerSettings.hpp"

#include <core_mmap.hpp>
#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// Server Settings

bool LoadFromFile(const std::string& file_path, ServerSettings& settings)
{
    MappedReadOnlySmallFile mmf;

    if (!mmf.Read(file_path.c_str())) {
        spdlog::error("Failed to load settings file: {}", file_path);
        return false;
    }

    std::string file_data(reinterpret_cast<const char*>(mmf.GetData()), mmf.GetDataBytes());

    try {
        YAML::Node node = YAML::Load(file_data);

        settings.ServerUdpPort = node["port"].as<int>(protos::kCaptureServerPort);
        settings.RendezvousServerHostname = node["rendezvous_host"].as<std::string>("localhost");
        settings.RendezvousServerPort = node["rendezvous_port"].as<int>(protos::kRendezvousServerPort);
        settings.ServerName = node["name"].as<std::string>("Default");
        settings.ServerPasswordHash = node["password_hash"].as<std::string>("");
        settings.EnableMultiServers = node["multi_servers"].as<bool>(false);
    } catch (YAML::ParserException& ex) {
        spdlog::error("YAML parse failed: {}", ex.what());
        return false;
    }

    return true;
}

bool SaveToFile(const ServerSettings& settings, const std::string& file_path)
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "port";
    out << YAML::Value << settings.ServerUdpPort;
    out << YAML::Key << "rendezvous_host";
    out << YAML::Value << settings.RendezvousServerHostname;
    out << YAML::Key << "rendezvous_port";
    out << YAML::Value << settings.RendezvousServerPort;
    out << YAML::Key << "name";
    out << YAML::Value << settings.ServerName;
    out << YAML::Key << "password_hash";
    out << YAML::Value << settings.ServerPasswordHash;
    out << YAML::Key << "multi_servers";
    out << YAML::Value << settings.EnableMultiServers;
    out << YAML::EndMap;

    if (!out.good()) {
        spdlog::error("Yaml emitter failed: {}", out.GetLastError());
        return false;
    }

    return WriteBufferToFile(file_path.c_str(), out.c_str(), out.size());
}


} // namespace core
