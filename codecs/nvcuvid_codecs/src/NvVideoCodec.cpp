// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "NvVideoCodec.hpp"

#include <core.hpp>
#include <core_logging.hpp>

#include <nppi_color_conversion.h> // Need color conversion

namespace nvcuvid {


//------------------------------------------------------------------------------
// Video Codec : API

bool VideoCodec::EncodeBegin(
    const VideoParameters& params,
    const VideoInputImage& image,
    std::vector<std::vector<uint8_t>>& vPacket)
{
    // If resolution changed:
    if (params.Width != Params.Width || params.Height != Params.Height) {
        CleanupCuda();
    }
    Params = params;

    return EncodeBeginNvenc(image, vPacket);
}

bool VideoCodec::EncodeFinish(
    std::vector<std::vector<uint8_t>>& vPacket)
{
    return EncodeFinishNvenc(vPacket);
}

bool VideoCodec::Decode(
    const VideoDecodeInput& input,
    std::vector<uint8_t>& decoded)
{
    // If resolution changed:
    if (input.Width != Params.Width || input.Height != Params.Height) {
        CleanupCuda();
    }
    Params.Width = input.Width;
    Params.Height = input.Height;
    Params.Type = input.Type;

    return DecodeNvdec(input, decoded);
}


//------------------------------------------------------------------------------
// Video Codec : CUDA Backend

bool VideoCodec::EncodeBeginNvenc(
    const VideoInputImage& image,
    std::vector<std::vector<uint8_t>>& vPacket)
{
    try {
        if (!CudaEncoder) {
            if (!CreateEncoder()) {
                spdlog::error("CreateEncoder failed");
                return false;
            }
        }

        const NvEncInputFrame* frame = CudaEncoder->GetNextInputFrame();

        // If no frames available:
        if (!frame) {
            return false;
        }

        if (!CopyImageToFrame(image, frame)) {
            spdlog::error("Failed to copy image to video encoder input frame");
            return false;
        }

        // The other parameters are filled in by NvEncoder::DoEncode
        NV_ENC_PIC_PARAMS pic_params = { NV_ENC_PIC_PARAMS_VER };
        pic_params.inputPitch = frame->pitch;

        if (image.IsKeyframe) {
            // Force an IDR and prepend SPS, PPS units
            pic_params.encodePicFlags |= NV_ENC_PIC_FLAG_OUTPUT_SPSPPS | NV_ENC_PIC_FLAG_FORCEIDR;
            pic_params.pictureType = NV_ENC_PIC_TYPE_IDR;
        } else {
            pic_params.pictureType = NV_ENC_PIC_TYPE_P;
        }

        // pic_params.frameIdx = 0; // Optional
        pic_params.inputTimeStamp = NextTimestamp++;
        // pic_params.inputDuration = 0; // TBD
        // pic_params.codecPicParams.h264PicParams; // No tweaks seem useful
        // pic_params.qpDeltaMap = nullptr; // TBD
        // pic_params.qpDeltaMapSize = 0; // TBD

        // Encode frame and wait for the result.
        // This takes under a millisecond on modern gaming laptops.
        CudaEncoder->EncodeFrame(vPacket, &pic_params);
    }
    catch (NVENCException& ex) {
        spdlog::error("NVENC exception during encoding: {}", ex.getErrorString());
        return false;
    }

    return true;
}

bool VideoCodec::CreateEncoder()
{
    if (!Context.Create()) {
        spdlog::error("CUDA context create failed");
        return false;
    }

    cuCtxPushCurrent(Context.Context);
    core::ScopedFunction ctx_scope([]() {
        cuCtxPopCurrent(nullptr);
    });

    CUresult cu_result = cuStreamCreate(&NvStream, CU_STREAM_NON_BLOCKING);
    if (cu_result != CUDA_SUCCESS) {
        spdlog::error("cuStreamCreate failed: {}[{}] {}",
            cu_result, CUresultToName(cu_result), CUresultToString(cu_result));
        return false;
    }
    NppStatus npp_status = nppSetStream((cudaStream_t)NvStream);
    if (npp_status != NPP_SUCCESS) {
        spdlog::error("nppSetStream failed: {}", npp_status);
        return false;
    }
    npp_status = nppGetStreamContext(&nppStreamContext);
    if (npp_status != NPP_SUCCESS) {
        spdlog::error("nppGetStreamContext failed: {}", npp_status);
        return false;
    }

    CodecGuid = (Params.Type == VideoType::H264) ? NV_ENC_CODEC_H264_GUID : NV_ENC_CODEC_HEVC_GUID;

    CudaEncoder = std::make_shared<NvEncoderCuda>(
        Context.Context,
        Params.Width,
        Params.Height,
        NV_ENC_BUFFER_FORMAT_IYUV); // YUV 4:2:0 multiplanar

    NV_ENC_INITIALIZE_PARAMS encodeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    encodeParams.encodeConfig = &encodeConfig;

    CudaEncoder->CreateDefaultEncoderParams(
        &encodeParams,
        CodecGuid,
        NV_ENC_PRESET_LOW_LATENCY_HQ_GUID);

    encodeParams.frameRateNum = Params.Fps;
    encodeParams.frameRateDen = 1;
    encodeParams.enablePTD = 1; // Allow NVENC to choose picture types

    bool intra_refresh = Params.UseIntraRefresh;
    if (intra_refresh) {
        intra_refresh = CudaEncoder->GetCapabilityValue(
            CodecGuid,
            NV_ENC_CAPS_SUPPORT_INTRA_REFRESH);
    }

    // Enable intra-refresh for a more consistent frame size:
    if (Params.Type == VideoType::H264) {
        auto& h264Config = encodeConfig.encodeCodecConfig.h264Config;
        h264Config.repeatSPSPPS = 0;
        if (intra_refresh) {
            h264Config.enableIntraRefresh = 1;
            h264Config.intraRefreshPeriod = Params.Fps * 10;
            h264Config.intraRefreshCnt = Params.Fps;
        }
        h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
    } else { // HEVC:
        auto& hevcConfig = encodeConfig.encodeCodecConfig.hevcConfig;
        hevcConfig.repeatSPSPPS = 0;
        if (intra_refresh) {
            hevcConfig.enableIntraRefresh = 1;
            hevcConfig.intraRefreshPeriod = Params.Fps * 10;
            hevcConfig.intraRefreshCnt = Params.Fps;
        }
        hevcConfig.idrPeriod = NVENC_INFINITE_GOPLENGTH;
    }

    // Manual IDRs when application requests a keyframe
    encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    encodeConfig.frameIntervalP = 1;

    // Choose VBR mode allowing for spikes for tricky frames
    // NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ: Error bound is smaller
    // NV_ENC_PARAMS_RC_CBR_HQ: Seems to have a longer tail of errors
    // NV_ENC_PARAMS_RC_VBR_HQ: Also long error tail
    encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
    encodeConfig.rcParams.averageBitRate = Params.Bitrate;
    encodeConfig.rcParams.maxBitRate = Params.Bitrate;

    // Tune VBV size 
    encodeConfig.rcParams.vbvBufferSize = Params.Bitrate / Params.Fps;
    encodeConfig.rcParams.vbvInitialDelay = encodeConfig.rcParams.vbvBufferSize;

    // Disable adaptive quantization for this type of data.
    // It leads to much higher long tail errors.
    encodeConfig.rcParams.enableTemporalAQ = 0;
    encodeConfig.rcParams.enableAQ = 0; // Spatial
    encodeConfig.rcParams.aqStrength = 1; // Lower is better

    // Disable B-frames
    encodeConfig.rcParams.zeroReorderDelay = 1;

    // Enable non-reference P-frame optimization
    encodeConfig.rcParams.enableNonRefP = 1; // requires enablePTD=1

    CudaEncoder->CreateEncoder(&encodeParams);
    return true;
}

bool VideoCodec::EncodeFinishNvenc(
    std::vector<std::vector<uint8_t>>& vPacket)
{
    if (!CudaEncoder) {
        return false;
    }
    try {
        CudaEncoder->EndEncode(vPacket);
    }
    catch (NVENCException& /*ex*/) {
        return false;
    }

    return true;
}

bool VideoCodec::DecodeNvdec(
    const VideoDecodeInput& input,
    std::vector<uint8_t>& decoded)
{
    try {
        if (!CudaDecoder) {
            if (!Context.Create()) {
                return false;
            }

            CudaDecoder = std::make_shared<NvDecoder>(
                Context.Context,
                false, // Do not use device frame
                Params.Type == VideoType::H264 ? cudaVideoCodec_H264 : cudaVideoCodec_HEVC,
                nullptr, // No mutex
                true, // Low latency
                false, // Non-pitched frame
                nullptr, // No crop
                nullptr, // No resize
                Params.Width, // Max size
                Params.Height);
        }

        uint8_t** frames = nullptr;
        int64_t* timestamps = nullptr;
        int frame_count = 0;

        // Retries are needed according to Nvidia engineers:
        // https://github.com/NVIDIA/NvPipe/blob/b3d0a7511052824ff0481fa6eecb3e95eac1a722/src/NvPipe.cu#L969

        for (int i = 0; i < 3; ++i) {
            bool success = CudaDecoder->Decode(
                input.Data,
                input.Bytes,
                &frames,
                &frame_count,
                CUVID_PKT_ENDOFPICTURE, // Immediate result requested
                &timestamps,
                0, // Timestamp
                cudaStreamPerThread); // Use the default per-thread stream
            if (!success) {
                return false;
            }

            // If we got a frame back:
            if (frame_count >= 1) {
                break;
            }

            // If we got no frame and we are ignoring output:
            if (input.Mode == DecodeMode::IgnoreOutput) {
                return true;
            }
        }

        // If we expected output but got none:
        if (frame_count < 1) {
            return false;
        }

        const unsigned y_bytes = Params.Width * Params.Height;
        const unsigned uv_bytes = y_bytes / 4;
        unsigned copy_bytes = y_bytes;
        if (input.Mode == DecodeMode::YUV420) {
            copy_bytes += uv_bytes * 2;
        }

        decoded.resize(copy_bytes);
        memcpy(decoded.data(), frames[0], copy_bytes);
    }
    catch (NVENCException& /*ex*/) {
        return false;
    }

    return true;
}

void VideoCodec::CleanupCuda()
{
    CudaEncoder.reset();
    CudaDecoder.reset();
    Context.Destroy();
    if (NvStream) {
        cuStreamDestroy(NvStream);
        NvStream = 0;
    }
}

bool VideoCodec::CopyImageToFrame(const VideoInputImage& image, const NvEncInputFrame* frame)
{
    cuCtxPushCurrent(Context.Context);
    core::ScopedFunction ctx_scope([]() {
        cuCtxPopCurrent(nullptr);
    });

    if (!image.Y) {
        spdlog::error("No Y channel");
        return false;
    }

    const uint32_t chromaWidthInBytes = NvEncoder::GetChromaWidthInBytes(frame->bufferFormat, image.Width);
    const uint32_t chromaHeight = NvEncoder::GetChromaHeight(frame->bufferFormat, image.Height);

    // If this is YUV422 -> YUV420
    if (image.U && image.V &&
        image.ChromaHeight == image.Height &&
        image.ChromaWidth == image.Width / 2)
    {
        const Npp8u* pSrc[3] = {
            (Npp8u*)image.Y,
            (Npp8u*)image.U,
            (Npp8u*)image.V
        };
        int rSrcStep[3] = {
            image.Stride,
            image.ChromaStride,
            image.ChromaStride
        };
        Npp8u* pDst[3] = {
            (Npp8u*)frame->inputPtr,
            (Npp8u*)frame->inputPtr + frame->chromaOffsets[0],
            (Npp8u*)frame->inputPtr + frame->chromaOffsets[1]
        };
        int nDstStep[3] = {
            (int)frame->pitch,
            (int)chromaWidthInBytes,
            (int)chromaWidthInBytes
        };
        NppiSize roi;
        roi.width = image.Width;
        roi.height = image.Height;

        NppStatus result = nppiYCbCr422ToYCbCr420_8u_P3R_Ctx(
            pSrc,
            rSrcStep,
            pDst,
            nDstStep,
            roi,
            nppStreamContext);
        if (result != NPP_SUCCESS) {
            spdlog::error("nppiYCbCr422ToYCbCr420_8u_P3R_Ctx failed: {}", result);
            return false;
        }

        return true;
    }

    if (image.Y)
    {
        CUdeviceptr dstChromaPtrs[2] = {
            (CUdeviceptr)((uint8_t*)frame->inputPtr + frame->chromaOffsets[0]),
            (CUdeviceptr)((uint8_t*)frame->inputPtr + frame->chromaOffsets[1])
        };
        NvEncoderCuda::CopyToDeviceFrame(
            Context.Context,
            image.Y,
            image.Stride,
            image.U,
            image.V,
            frame->pitch,
            CUmemorytype::CU_MEMORYTYPE_DEVICE,
            image.Width,
            image.Height,
            CUmemorytype::CU_MEMORYTYPE_DEVICE,
            frame->bufferFormat,
            dstChromaPtrs,
            frame->chromaPitch);
#if 0
        cudaError_t error = cudaMemcpy2DAsync(frame->inputPtr, frame->pitch, image.Y, image.Stride, image.Width, image.Height, cudaMemcpyKind::cudaMemcpyDeviceToDevice, NvStream);
        if (error != cudaSuccess) {
            spdlog::error("cudaMalloc failed: {} {} {}",
                error, cudaGetErrorName(error), cudaGetErrorString(error));
            return nullptr;
        }
#endif
#if 0
        CUDA_MEMCPY2D m{};
        if (image.IsDevicePtr) {
            m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
            m.srcDevice = (CUdeviceptr)image.Y;
        } else {
            m.srcMemoryType = CU_MEMORYTYPE_HOST;
            m.srcHost = image.Y;
        }
        m.srcPitch = image.Stride;
        m.dstMemoryType = CU_MEMORYTYPE_DEVICE;
        m.dstDevice = (CUdeviceptr)frame->inputPtr;
        m.dstPitch = frame->pitch;
        m.WidthInBytes = image.Width;
        m.Height = image.Height;

        CUresult cu_result = cuMemcpy2DAsync(&m, NvStream);
        if (cu_result != CUDA_SUCCESS) {
            spdlog::error("Y: cuMemcpy2DAsync failed: {}[{}] {}",
                cu_result, CUresultToName(cu_result), CUresultToString(cu_result));
            return false;
        } 
#endif
    }

    // If this is YUV422 -> YUV420
    if (image.U && image.V &&
        image.ChromaHeight == image.Height &&
        image.ChromaWidth == image.Width / 2)
    {
        uint8_t* dest_uv = (uint8_t*)frame->inputPtr + frame->chromaOffsets[0];
        uint8_t* src_uv = (uint8_t*)image.U;

        for (int i = 0; i < image.ChromaHeight; ++i) {
            //cuMemcpyDtoDAsync((CUdeviceptr)dest_uv, (CUdeviceptr)src_uv, image.ChromaWidth, NvStream);
            cudaMemcpyAsync(dest_uv, src_uv, image.ChromaWidth, cudaMemcpyKind::cudaMemcpyDeviceToDevice, NvStream);
            dest_uv += frame->chromaPitch;
            src_uv += image.ChromaStride * 2;
        }

        dest_uv = (uint8_t*)frame->inputPtr + frame->chromaOffsets[1];
        src_uv = (uint8_t*)image.V;

        for (int i = 0; i < image.ChromaHeight; ++i) {
            //cuMemcpyDtoDAsync((CUdeviceptr)dest_uv, (CUdeviceptr)src_uv, image.ChromaWidth, NvStream);
            cudaMemcpyAsync(dest_uv, src_uv, image.ChromaWidth, cudaMemcpyKind::cudaMemcpyDeviceToDevice, NvStream);
            dest_uv += frame->chromaPitch;
            src_uv += image.ChromaStride * 2;
        }

        return true;
    }

    if (image.U)
    {
        if (frame->numChromaPlanes < 1) {
            spdlog::error("numChromaPlanes={}", frame->numChromaPlanes);
            return false;
        }
        if (chromaWidthInBytes != static_cast<uint32_t>( image.ChromaWidth )) {
            spdlog::error("Encoder chromaWidthInBytes != image.ChromaWidth", chromaWidthInBytes, image.ChromaWidth);
            return false;
        }
        if (chromaHeight != static_cast<uint32_t>( image.ChromaHeight )) {
            spdlog::error("Encoder chromaHeight != image.ChromaHeight", chromaHeight, image.ChromaHeight);
            return false;
        }

        uint8_t* dest_u = (uint8_t*)frame->inputPtr + frame->chromaOffsets[0];

        CUDA_MEMCPY2D m{};
        if (image.IsDevicePtr) {
            m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
            m.srcDevice = (CUdeviceptr)image.U;
        } else {
            m.srcMemoryType = CU_MEMORYTYPE_HOST;
            m.srcHost = image.U;
        }
        m.srcPitch = image.ChromaStride;
        m.dstMemoryType = CU_MEMORYTYPE_DEVICE;
        m.dstDevice = (CUdeviceptr)dest_u;
        m.dstPitch = frame->chromaPitch;
        m.WidthInBytes = chromaWidthInBytes;
        m.Height = chromaHeight;

        CUresult cu_result = cuMemcpy2DAsync(&m, NvStream);
        if (cu_result != CUDA_SUCCESS) {
            spdlog::error("U: cuMemcpy2DAsync failed: {}[{}] {}",
                cu_result, CUresultToName(cu_result), CUresultToString(cu_result));
            return false;
        } 
    }

    if (image.V)
    {
        if (frame->numChromaPlanes < 2) {
            spdlog::error("numChromaPlanes={}", frame->numChromaPlanes);
            return false;
        }
        if (chromaWidthInBytes != static_cast<uint32_t>( image.ChromaWidth )) {
            spdlog::error("Encoder chromaWidthInBytes != image.ChromaWidth", chromaWidthInBytes, image.ChromaWidth);
            return false;
        }
        if (chromaHeight != static_cast<uint32_t>( image.ChromaHeight )) {
            spdlog::error("Encoder chromaHeight != image.ChromaHeight", chromaHeight, image.ChromaHeight);
            return false;
        }

        uint8_t* dest_v = (uint8_t*)frame->inputPtr + frame->chromaOffsets[1];

        CUDA_MEMCPY2D m{};
        if (image.IsDevicePtr) {
            m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
            m.srcDevice = (CUdeviceptr)image.V;
        } else {
            m.srcMemoryType = CU_MEMORYTYPE_HOST;
            m.srcHost = image.V;
        }
        m.srcPitch = image.ChromaStride;
        m.dstMemoryType = CU_MEMORYTYPE_DEVICE;
        m.dstDevice = (CUdeviceptr)dest_v;
        m.dstPitch = frame->chromaPitch;
        m.WidthInBytes = chromaWidthInBytes;
        m.Height = chromaHeight;

        CUresult cu_result = cuMemcpy2DAsync(&m, NvStream);
        if (cu_result != CUDA_SUCCESS) {
            spdlog::error("U: cuMemcpy2DAsync failed: {}[{}] {}",
                cu_result, CUresultToName(cu_result), CUresultToString(cu_result));
            return false;
        } 
    }

    // TBD: Not sure if this is needed
    CUresult cuda_result = cuStreamSynchronize(NvStream);
    if (cuda_result != CUDA_SUCCESS) {
        spdlog::error("cuStreamSynchronize failed: {}[{}] {}",
            cuda_result, CUresultToName(cuda_result), CUresultToString(cuda_result));
        return false;
    } 

    return true;
}


} // namespace nvcuvid
