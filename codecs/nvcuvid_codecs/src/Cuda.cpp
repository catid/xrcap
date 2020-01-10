// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "Cuda.hpp"

#include <spdlog/spdlog.h>

namespace nvcuvid {


//------------------------------------------------------------------------------
// Tools

const char* CUresultToName(CUresult result)
{
    const char* str = nullptr;
    cuGetErrorName(result, &str);
    if (!str) {
        return "(Unknown)";
    }
    return str;
}

const char* CUresultToString(CUresult result)
{
    const char* str = nullptr;
    cuGetErrorString(result, &str);
    if (!str) {
        return "(Unknown)";
    }
    return str;
}


//------------------------------------------------------------------------------
// CUDA Context

bool CudaContext::Create(int gpu_index)
{
    if (Context) {
        return true; // Already created
    }

    GpuIndex = gpu_index;

    CUresult result = cuInit(0);
    if (result != CUDA_SUCCESS) {
        spdlog::error("cuInit failed: {}[{}] {}", result, CUresultToName(result), CUresultToString(result));
        return false;
    }

    result = cuDeviceGet(&Device, gpu_index);
    if (result != CUDA_SUCCESS) {
        spdlog::error("cuDeviceGet failed: {}[{}] {}", result, CUresultToName(result), CUresultToString(result));
        return false;
    }

    cudaError_t err = cudaGetDeviceProperties(&Properties, Device);
    if (err != cudaSuccess) {
        spdlog::error("cudaGetDeviceProperties failed: {}[{}] {}", err, cudaGetErrorName(err), cudaGetErrorString(err));
        return false;
    }

    // Reuse the primary context to play nicer with application code
    result = cuDevicePrimaryCtxRetain(&Context, Device);
    if (result != CUDA_SUCCESS) {
        spdlog::error("cuDevicePrimaryCtxRetain failed: {}[{}] {}", result, CUresultToName(result), CUresultToString(result));
        return false;
    }

    return true;
}

void CudaContext::Destroy()
{
    if (Context) {
        cuDevicePrimaryCtxRelease(Device);
        Context = nullptr;
    }
}


} // namespace nvcuvid
