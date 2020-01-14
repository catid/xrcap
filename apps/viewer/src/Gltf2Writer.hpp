// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    glTF 2.0 File Serializer

    Format specification:
    https://github.com/KhronosGroup/glTF/tree/master/specification/2.0

    Format validators:
    https://github.khronos.org/glTF-Validator/
    https://developers.facebook.com/tools/3d/validation
    Maximum GLB size for Facebook = 3145728 bytes

    Code references:
    https://github.com/facebookincubator/FBX2glTF/

    Draco mesh compression extension spec:
    https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_draco_mesh_compression/README.md

    Known issues:
    + Images are not padded out to power-of-two sizes
*/

#pragma once

#include <capture_client.h>
#include <string>

namespace core {


//------------------------------------------------------------------------------
// GLTF Types

#pragma pack(push)
#pragma pack(1)

static const uint32_t kGlbMagic = 0x46546c67;
static const uint32_t kGlbVersion = 2;

struct GlbFileHeader
{
    uint32_t Magic = kGlbMagic;
    uint32_t Version = kGlbVersion;
    uint32_t Length = 0;
};

static const unsigned kGlbFileHeaderBytes = 12;

static const uint32_t kGlbChunkType_Json = 0x4e4f534a;
static const uint32_t kGlbChunkType_Bin = 0x004e4942;

struct GlbChunkHeader
{
    uint32_t Length = 0;
    uint32_t Type = kGlbChunkType_Bin;
};

static const unsigned kGlbChunkHeaderBytes = 8;

#pragma pack(pop)


//------------------------------------------------------------------------------
// GLTF Writer

struct GltfParams
{
    // Full path to .glb file
    std::string OutputFilePath;

    // Google Draco mesh compressor optional extension
    bool EnableDraco = false;

    // JPEG quality level 80..100
    int JpegQuality = 90;
};

// GLB is the binary version of glTF 2.0 that can contain textures and so on
bool WriteFrameToGlbFile(const XrcapFrame& frame, const GltfParams& params);


} // namespace core
