// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <cuda.h> // CUDA driver API
#include <cuda_runtime_api.h> // CUDA runtime API

namespace nvcuvid {


//------------------------------------------------------------------------------
// Tools

const char* CUresultToName(CUresult result);
const char* CUresultToString(CUresult result);


//------------------------------------------------------------------------------
// CUDA Context

struct CudaContext
{
    ~CudaContext()
    {
        Destroy();
    }

    bool Valid() const
    {
        return Context != nullptr;
    }

    CUcontext Context = nullptr;

    CUdevice Device = 0;
    cudaDeviceProp Properties{};
    int GpuIndex = 0;


    // Create the context
    bool Create(int gpu_index = 0);
    void Destroy();
};


} // namespace nvcuvid
