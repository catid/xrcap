// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Logging based on sdplog


    Examples:

    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);
    
    spdlog::warn("Easy padding in numbers like {:08d}", 12);
    spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    spdlog::info("Support for floats {:03.2f}", 1.23456);
    spdlog::info("Positional args are {1} {0}..", "too", "supported");
    spdlog::info("{:<30}", "left aligned");
    
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    spdlog::debug("This message should be displayed..");    
    
    // change log pattern
    spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");
    
    // Compile time log levels
    // define SPDLOG_ACTIVE_LEVEL to desired level
    SPDLOG_TRACE("Some trace message with param {}", {});
    SPDLOG_DEBUG("Some debug message");
*/

#pragma once

#include "core.hpp"

#include <spdlog/spdlog.h>

namespace core {


//------------------------------------------------------------------------------
// Tools

// OS-specific directory path for logs
std::string GetLogFilePath(
    const std::string& company_name,
    const std::string& application_name);

// Get an OS-specific file path for a settings file
std::string GetSettingsFilePath(
    const std::string& company_name,
    const std::string& file_name);

// Set up color console and rotated disk logging from a background thread
void SetupAsyncDiskLog(const std::string& filename);


} // core
