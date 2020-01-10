// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core_logging.hpp"
using namespace core;

int main(int argc, char* argv[])
{
    CORE_UNUSED2(argc, argv);

    SetupAsyncDiskLog("core_test.txt");

    spdlog::info("Core library tests");

    return CORE_APP_SUCCESS;
}
