// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "FrameInfo.hpp"

#include <MfxVideoDecoder.hpp> // mfx_codecs
#ifdef ZDEPTH_NVCUVID
#include <NvVideoCodec.hpp> // nvcuvid_codecs
#endif
#include <zdepth_lossless.hpp> // zdepth
#include <zdepth_lossy.hpp> // zdepth
#include <core.hpp> // core
#include <DepthMesh.hpp> // depth_mesh
#include <tbb/tbb.h> // tbb

#include <functional>

namespace core {


//------------------------------------------------------------------------------
// Constants

static const int kMaxQueuedDecodes = 60;


//------------------------------------------------------------------------------
// DecodedFrame

struct DecodedFrame
{
    std::shared_ptr<FrameInfo> Info;

    // Decoded frame data in system memory
    mfx::frameref_t FrameRef;

    // NV12 picture
    uint8_t* Y = nullptr;
    uint8_t* UV = nullptr;
    int Width = 0;
    int Height = 0;
    int ChromaWidth = 0;
    int ChromaHeight = 0;

    // Depth data
    int DepthWidth = 0;
    int DepthHeight = 0;
    std::vector<uint16_t> Depth;

    // Recovered mesh
    int FloatsCount = 0;
    std::vector<float> XyzuvVertices;
    int IndicesCount = 0;
    std::vector<uint32_t> Indices;
};


//------------------------------------------------------------------------------
// BackreferenceChecker

class BackreferenceChecker
{
public:
    void Reset();

    // Returns true if the back-reference is satisfied
    bool Check(uint32_t frame_code, int32_t back_reference);

protected:
    static const int kMaxAccepted = 4;

    // Ring buffer of accepted frame codes
    uint32_t Accepted[kMaxAccepted];
    int NextIndex = 0;
    int Count = 0;
};


//------------------------------------------------------------------------------
// DecodePipelineData

using DecodePipelineCallback = std::function<void(std::shared_ptr<DecodedFrame>)>;

struct DecodePipelineData
{
    // Inputs
    DecodePipelineCallback Callback;
    std::shared_ptr<FrameInfo> Input;

    // Outputs
    std::shared_ptr<DecodedFrame> Output;
};


//------------------------------------------------------------------------------
// DecodePipelineElement

/*
    Processing pipeline:

    (1) Decompress and generate mesh
    (2) Decompress texture
*/

class DecodePipelineElement
{
public:
    void Initialize(
        std::shared_ptr<DecodePipelineElement> next_element,
        std::string element_name);
    ~DecodePipelineElement()
    {
        Shutdown();
    }
    void Shutdown();

    // Returns false if the queue overflowed
    void Process(std::shared_ptr<DecodePipelineData> data);

protected:
    std::shared_ptr<DecodePipelineElement> NextElement;
    std::string ElementName;
    DecodePipelineCallback Callback;

    core::WorkerQueue Worker;

    virtual bool Run(std::shared_ptr<DecodePipelineData> data) = 0;
};


//------------------------------------------------------------------------------
// Element State

struct VideoDecoderElement : public DecodePipelineElement
{
    ~VideoDecoderElement()
    {
        Shutdown();
    }

    unsigned Width = 0;

    std::unique_ptr<mfx::VideoDecoder> IntelDecoder;

    BackreferenceChecker BackrefChecker;

    bool Run(std::shared_ptr<DecodePipelineData> data) override;
};

struct MeshDecompressorElement : public DecodePipelineElement
{
    ~MeshDecompressorElement()
    {
        Shutdown();
    }

    int DepthWidth = 0;
    int ColorWidth = 0;

    // Depth decompressor
    std::unique_ptr<lossless::DepthCompressor> LosslessDepth;
    std::unique_ptr<lossy::DepthCompressor> LossyDepth;

    // Mesh from depth
    std::unique_ptr<DepthMesher> Mesher;
    TemporalDepthFilter TemporalFilter;
    DepthEdgeFilter EdgeFilter;

    bool Run(std::shared_ptr<DecodePipelineData> data) override;
};


//------------------------------------------------------------------------------
// DecoderPipeline

// Processing pipeline for one camera
struct DecoderPipeline
{
    DecoderPipeline();
    ~DecoderPipeline();

    // Set callback before call
    inline void Process(std::shared_ptr<DecodePipelineData> data)
    {
        MeshDecompressor->Process(data);
    }

    std::shared_ptr<VideoDecoderElement> VideoDecoder;
    std::shared_ptr<MeshDecompressorElement> MeshDecompressor;
};


} // namespace core
