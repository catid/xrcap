// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureDecoder.hpp"

#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// BackreferenceChecker

void BackreferenceChecker::Reset()
{
    NextIndex = 0;
    Count = 0;
}

bool BackreferenceChecker::Check(uint32_t frame_code, int32_t back_reference)
{
    // If there is a back reference to check:
    if (back_reference != 0)
    {
        bool found = false;
        uint32_t expected_frame_code = frame_code + back_reference;

        for (int i = 0; i < Count; ++i)
        {
            uint32_t accepted_code = Accepted[i];

            if (accepted_code == expected_frame_code) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    // Insert into ring buffer
    Accepted[NextIndex] = frame_code;
    if (NextIndex >= Count) {
        Count = NextIndex + 1;
    }
    if (++NextIndex >= kMaxAccepted) {
        NextIndex = 0;
    }

    return true;
}


//------------------------------------------------------------------------------
// DecodePipelineElement

void DecodePipelineElement::Initialize(
    std::shared_ptr<DecodePipelineElement> next_element,
    std::string element_name)
{
    NextElement = next_element;
    ElementName = element_name;

    Worker.Initialize(kMaxQueuedDecodes);
}

void DecodePipelineElement::Shutdown()
{
    Worker.Shutdown();
}

void DecodePipelineElement::Process(std::shared_ptr<DecodePipelineData> data)
{
    // Called from the worker thread of the previous element:

    const bool pushed = Worker.SubmitWork([this, data]()
    {
        const bool success = Run(data);

        // If operation failed:
        if (!success) {
            spdlog::warn("Operation failed for stage {}: Dropped frame {}",
                ElementName, data->Input->FrameHeader.FrameNumber);
            return;
        }

        // If there is another element in the pipe:
        if (NextElement) {
            NextElement->Process(data);
        } else {
            data->Callback(data->Output);
        }
    });

    // If the queue overflowed to the next element:
    if (!pushed) {
        spdlog::warn("Computer too slow for stage {}: Dropped frame {}",
            ElementName, data->Input->FrameHeader.FrameNumber);
    }
}


//------------------------------------------------------------------------------
// VideoDecoderElement

bool VideoDecoderElement::Run(std::shared_ptr<DecodePipelineData> data)
{
    auto& input = data->Input;
    auto& video_info = input->VideoInfo;

    if (Width != video_info->Width) {
        spdlog::info("Video decoder reset on resolution change {}x{}", video_info->Width, video_info->Height);
        IntelDecoder.reset();
    }

    if (!IntelDecoder) {
        if (data->Input->FrameHeader.BackReference != 0) {
            spdlog::warn("Video decoder cannot initialize on a P-frame: Waiting for next keyframe");
            return false;
        }

        IntelDecoder = std::make_unique<mfx::VideoDecoder>();
        bool success = IntelDecoder->Initialize(
            false, // prefer on CPU
            video_info->VideoType == protos::VideoType_H264 ? MFX_CODEC_AVC : MFX_CODEC_HEVC,
            input->StreamedImage.Data.data(),
            static_cast<int>( input->StreamedImage.Data.size() ));
        if (!success) {
            spdlog::error("Failed to initialize video decoder");
            IntelDecoder.reset();
            return false;
        }

        Width = video_info->Width;
        BackrefChecker.Reset();
    }

    auto& batch = input->BatchInfo;
    if (!BackrefChecker.Check(data->Input->FrameHeader.FrameNumber, data->Input->FrameHeader.BackReference)) {
#if 0
        spdlog::error("Reset video decoder on unsatisfied back-reference: frame={} ref={}",
            batch->FrameNumber, batch->BackReference);
        IntelDecoder.reset();
        return false;
#else
        spdlog::warn("Corrupted video: Unsatisfied back-reference: frame={} ref={}",
            data->Input->FrameHeader.FrameNumber, data->Input->FrameHeader.BackReference);
#endif
    }

    auto& image = input->StreamedImage.Data;
    auto& output = data->Output;

    output->FrameRef = IntelDecoder->Decode(
        image.data(),
        static_cast<int>( image.size() ));

    if (!output->FrameRef) {
        spdlog::error("Video decode failed: Resetting decoder");
        IntelDecoder.reset();
        return false;
    }

    output->Info = input;

    auto& raw = output->FrameRef->Raw;
    output->Y = raw->Surface.Data.Y;
    output->UV = raw->Surface.Data.UV;
    output->Width = raw->Surface.Info.Width;
    output->Height = raw->Surface.Info.Height;
    output->ChromaWidth = output->Width / 2;
    output->ChromaHeight = output->Height / 2;

    return true;
}


//------------------------------------------------------------------------------
// MeshDecompressorElement

bool MeshDecompressorElement::Run(std::shared_ptr<DecodePipelineData> data)
{
    data->Output = std::make_shared<DecodedFrame>();
    auto& output = data->Output;

    auto& depth_data = data->Input->StreamedDepth.Data;

    const bool lossless_depth = lossless::IsDepthFrame(
        depth_data.data(),
        static_cast<unsigned>( depth_data.size() ));
    const bool lossy_depth = lossy::IsDepthFrame(
        depth_data.data(),
        static_cast<unsigned>( depth_data.size() ));

    if (lossless_depth)
    {
        if (!LosslessDepth) {
            LosslessDepth = std::make_unique<lossless::DepthCompressor>();
        }
        LossyDepth.reset();

        lossless::DepthResult depth_result = LosslessDepth->Decompress(
            depth_data,
            output->DepthWidth,
            output->DepthHeight,
            output->Depth);
        if (depth_result != lossless::DepthResult::Success) {
            spdlog::warn("Depth decompression failed: {}", lossless::DepthResultString(depth_result));
            return false;
        }
    }
    else if (lossy_depth)
    {
        if (!LossyDepth) {
            LossyDepth = std::make_unique<lossy::DepthCompressor>();
        }
        LosslessDepth.reset();

        lossy::DepthResult depth_result = LossyDepth->Decompress(
            depth_data,
            output->DepthWidth,
            output->DepthHeight,
            output->Depth);
        if (depth_result != lossy::DepthResult::Success) {
            spdlog::warn("Depth decompression failed: {}", lossy::DepthResultString(depth_result));
            return false;
        }
    }
    else {
        spdlog::error("Depth data is corrupted");
        return false;
    }

    if (output->DepthWidth != data->Input->Calibration->Depth.Width ||
        output->DepthHeight != data->Input->Calibration->Depth.Height)
    {
        spdlog::error("Calibration resolution does not match depth resolution");
        return false;
    }

    // If depth mesh size changed:
    const int color_width = data->Input->Calibration->Color.Width;
    const int color_height = data->Input->Calibration->Color.Height;
    if (DepthWidth != output->DepthWidth || ColorWidth != color_width) {
        spdlog::debug("Resetting mesher on resolution change: depth={}x{} color={}x{}",
            output->DepthWidth, output->DepthHeight,
            color_width, color_height);
        Mesher.reset();
    }
    DepthWidth = output->DepthWidth;
    ColorWidth = color_width;

    if (!Mesher) {
        Mesher = std::make_unique<DepthMesher>();
        Mesher->Initialize(*data->Input->Calibration);
    }

    std::shared_ptr<protos::CameraExtrinsics> extrinsics = data->Input->Extrinsics;

    bool cull_depth = true;
    if (data->Input->CaptureMode == protos::Mode_Calibration) {
        // Disable post-processing on the mesh
        cull_depth = false;
    }

    TemporalFilter.Filter(output->Depth.data(), output->DepthWidth, output->DepthHeight);

    EdgeFilter.Filter(output->Depth.data(), output->DepthWidth, output->DepthHeight);

    const bool face_painting_fix = false; // Only do this on the server side

    Mesher->GenerateCoordinates(
        output->Depth.data(),
        nullptr,
        //nullptr,
        output->XyzuvVertices,
        face_painting_fix,
        cull_depth);
    output->FloatsCount = static_cast<int>( output->XyzuvVertices.size() );

    Mesher->GenerateTriangleIndices(
        output->Depth.data(),
        output->Indices);
    output->IndicesCount = static_cast<int>( output->Indices.size() );

    return true;
}


//------------------------------------------------------------------------------
// DecoderPipeline

DecoderPipeline::DecoderPipeline()
{
    VideoDecoder = std::make_shared<VideoDecoderElement>();
    VideoDecoder->Initialize(nullptr, "Video Decoder");

    MeshDecompressor = std::make_shared<MeshDecompressorElement>();
    MeshDecompressor->Initialize(VideoDecoder, "Mesh Decompressor");
}

DecoderPipeline::~DecoderPipeline()
{
    MeshDecompressor.reset();
    VideoDecoder.reset();
}


} // namespace core
