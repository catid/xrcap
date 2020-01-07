// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

// Format specification:
// https://github.com/KhronosGroup/glTF/tree/master/specification/2.0

#pragma once

#include <capture_client.h>

namespace core {


//------------------------------------------------------------------------------
// GLTF Types

struct GlbFileHeader
{
    uint32_t Magic;
    uint32_t Version;
    uint32_t Length;
};

struct GlbChunkHeader
{
    uint32_t Length;
    uint32_t Type;
};


//------------------------------------------------------------------------------
// GLTF Writer

// GLB is the binary version of glTF 2.0 that can contain textures and so on
bool WriteFrameToGlbFile(const XrcapFrame& frame, const char* file_path);


} // namespace core
