// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core_logging.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#ifdef _WIN32
#include <shlwapi.h>
#include <shlobj.h>
#pragma comment(lib,"shlwapi.lib")
#endif // _WIN32

namespace core {


//------------------------------------------------------------------------------
// Tools

std::string GetLogFilePath(
    const std::string& company_name,
    const std::string& application_name)
{
    const std::string file_name = fmt::format("{}.log", application_name);

#ifdef _WIN32
    char szPath[MAX_PATH];
    HRESULT hr = ::SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath);
    if (FAILED(hr)) {
        return file_name;
    }

    const std::string company_path = fmt::format("\\{}", company_name);
    BOOL result = ::PathAppendA(szPath, company_path.c_str());
    if (!result) {
        return file_name;
    }

    ::CreateDirectory(szPath, nullptr);

    const std::string app_path = fmt::format("\\{}", file_name);
    result = ::PathAppendA(szPath, app_path.c_str());
    if (!result) {
        return file_name;
    }

    return szPath;
#else
    return log_file_name;
#endif
}

std::string GetSettingsFilePath(
    const std::string& company_name,
    const std::string& file_name)
{
#ifdef _WIN32
    char szPath[MAX_PATH];
    HRESULT hr = ::SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath);
    if (FAILED(hr)) {
        return file_name;
    }

    const std::string company_path = fmt::format("\\{}", company_name);
    BOOL result = ::PathAppendA(szPath, company_path.c_str());
    if (!result) {
        return file_name;
    }

    ::CreateDirectory(szPath, nullptr);

    const std::string app_path = fmt::format("\\{}", file_name);
    result = ::PathAppendA(szPath, app_path.c_str());
    if (!result) {
        return file_name;
    }

    return szPath;
#else
    return file_name;
#endif
}

static void AtExitWrapper()
{
    spdlog::info("Terminated");
    spdlog::shutdown();
}

void SetupAsyncDiskLog(const std::string& filename)
{
    spdlog::init_thread_pool(8192, 1);
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        filename,
        4*1024*1024,
        3);
    std::vector<spdlog::sink_ptr> sinks {
        stdout_sink,
        rotating_sink
    };
    auto logger = std::make_shared<spdlog::async_logger>(
        filename,
        sinks.begin(), sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);
        //spdlog::async_overflow_policy::block);
    spdlog::register_logger(logger);

    spdlog::set_default_logger(logger);

    spdlog::set_pattern("[%H:%M:%S %z] [%^%L%$] %v");

    spdlog::set_level(spdlog::level::debug); // Set global log level to debug

    // Register an atexit() callback so we do not need manual shutdown in app code
    std::atexit(AtExitWrapper);
}


} // namespace core
