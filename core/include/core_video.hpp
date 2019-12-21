// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "core_serializer.hpp"

#include <functional>

namespace core {


//------------------------------------------------------------------------------
// Tools

// Parses buffer for 00 00 01 start codes
int FindAnnexBStart(const uint8_t* data, int bytes);

using NaluCallback = std::function<void(uint8_t* data, int bytes)>;

// Invoke the callback for each Annex B NALU for H.264/H.265 video
int EnumerateAnnexBNalus(
    uint8_t* data,
    int bytes,
    NaluCallback callback);

// Read ExpGolomb format from H.264/HEVC
unsigned ReadExpGolomb(ReadBitStream& bs);


//------------------------------------------------------------------------------
// VideoParser

struct CopyRange
{
    uint8_t* Ptr = nullptr;
    int Bytes = 0;

    CopyRange()
    {
    }
    CopyRange(uint8_t* ptr, int bytes)
        : Ptr(ptr)
        , Bytes(bytes)
    {
    }
};

static const int kMaxCopyRangesPerPicture = 16;

struct PictureRanges
{
    std::vector<CopyRange> Ranges;
    int RangeCount = 0;
    int TotalBytes = 0;
};

struct VideoParser
{
    int NalUnitCount = 0;

    // Parameter data
    std::vector<CopyRange> Parameters;
    int TotalParameterBytes = 0;

    // Picture data
    std::vector<PictureRanges> Pictures;

    // Updated by AppendSlice
    int WritePictureIndex = -1;

    void Reset();

    // Parse video into parameter/picture NAL units
    void ParseVideo(
        bool is_hevc_else_h264,
        uint8_t* data,
        int bytes);

protected:
    void ParseNalUnitH264(uint8_t* data, int bytes);
    void ParseNalUnitHEVC(uint8_t* data, int bytes);
    void AppendSlice(uint8_t* ptr, int bytes, bool new_picture);
};


} // namespace core
