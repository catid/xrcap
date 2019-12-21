// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "FileFormat.hpp"

namespace core {


//------------------------------------------------------------------------------
// Constants

const char* FileChunkTypeToString(unsigned chunk_type)
{
    static_assert(FileChunk_Count == 5, "Update this");
    switch (chunk_type)
    {
    case FileChunk_Calibration: return "Calibration";
    case FileChunk_Extrinsics: return "Extrinsics";
    case FileChunk_VideoInfo: return "VideoInfo";
    case FileChunk_BatchInfo: return "BatchInfo";
    case FileChunk_Frame: return "Frame";
    default: break;
    }
    return "(Invalid FileChunkType)";
}

const char* FileChunkLensToString(unsigned chunk_lens)
{
    static_assert(ChunkLens_Count == 5, "Update this");
    switch (chunk_lens)
    {
    case ChunkLens_Unknown: return "Unknown";
    case ChunkLens_Theta: return "Theta";
    case ChunkLens_Polynomial_3K: return "Polynomial 3K";
    case ChunkLens_Rational_6KT: return "Rational 6KT";
    case ChunkLens_Brown_Conrady: return "Brown Conrady";
    default: break;
    }
    return "(Invalid ChunkLensType)";
}

const char* FileChunkVideoToString(unsigned chunk_video)
{
    static_assert(ChunkVideo_Count == 3, "Update this");
    switch (chunk_video)
    {
    case ChunkVideo_Lossless: return "Lossless";
    case ChunkVideo_H264: return "H.264";
    case ChunkVideo_H265: return "H.265";
    default: break;
    }
    return "(Invalid ChunkVideoType)";
}


} // namespace core
