// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core_video.hpp"
#include "core_logging.hpp"

namespace core {


//------------------------------------------------------------------------------
// Tools

// Parses buffer for 00 00 01 start codes
int FindAnnexBStart(const uint8_t* data, int bytes)
{
    bytes -= 2;
    for (int i = 0; i < bytes; ++i)
    {
        if (data[i] == 0) {
            if (data[i + 1] == 0) {
                if (data[i + 2] == 1) {
                    return i;
                }
            }
        }
    }
    return -1;
}

static const int kAnnexBPrefixBytes = 3;

int EnumerateAnnexBNalus(uint8_t* data, int bytes, NaluCallback callback)
{
    int nalu_count = 0;
    int last_offset = -kAnnexBPrefixBytes;

    for (;;)
    {
        const int next_start = last_offset + kAnnexBPrefixBytes;
        int nal_offset = FindAnnexBStart(data + next_start, bytes - next_start);

        if (nal_offset < 0) {
            break;
        }
        nal_offset += next_start;

        if (last_offset >= 0)
        {
            uint8_t* nal_data = data + last_offset + kAnnexBPrefixBytes;
            int nal_bytes = nal_offset - last_offset - kAnnexBPrefixBytes;

            // Check for extra 00
            if (nal_data[nal_bytes - 1] == 0) {
                nal_bytes--;
            }

            callback(nal_data, nal_bytes);
            ++nalu_count;
        }

        last_offset = nal_offset;
    }

    if (last_offset >= 0)
    {
        uint8_t* nal_data = data + last_offset + kAnnexBPrefixBytes;
        const int nal_bytes = bytes - last_offset - kAnnexBPrefixBytes;

        callback(nal_data, nal_bytes);
        ++nalu_count;
    }

    return nalu_count;
}

unsigned ReadExpGolomb(ReadBitStream& bs)
{
    // Count number of leading zeroes
    unsigned read_count = 0;
    for (read_count = 0; read_count < 128; ++read_count) {
        if (bs.Read(1) != 0) {
            break;
        }
    }

    if (read_count <= 0 || read_count > 32) {
        return 0;
    }

    return bs.Read(read_count) + (1 << read_count) - 1;
}


//------------------------------------------------------------------------------
// VideoParser

void VideoParser::Reset()
{
    NalUnitCount = 0;

    Parameters.reserve(3);
    Parameters.clear();
    TotalParameterBytes = 0;

    Pictures.reserve(1);
    Pictures.clear();
    WritePictureIndex = -1;
}

void VideoParser::AppendSlice(uint8_t* ptr, int bytes, bool new_picture)
{
    if (new_picture) {
        ++WritePictureIndex;
    }
    if (WritePictureIndex >= Pictures.size()) {
        Pictures.resize(WritePictureIndex + 1);
    }
    if (WritePictureIndex < 0) {
        spdlog::warn("Dropping dangling NAL unit from encoder before start of picture");
        return;
    }

    auto& picture = Pictures[WritePictureIndex];

    if (picture.RangeCount >= kMaxCopyRangesPerPicture) {
        spdlog::error("FIXME: RangeCount exceeded kMaxCopyRangesPerPicture");
        return;
    }

    picture.Ranges.emplace_back(ptr, bytes);
    picture.TotalBytes += bytes;
}

void VideoParser::ParseVideo(
    bool is_hevc_else_h264,
    uint8_t* data,
    int bytes)
{
    NaluCallback nalu_callback;
    if (is_hevc_else_h264) {
        nalu_callback = [this](uint8_t* data, int bytes) {
            ParseNalUnitHEVC(data, bytes);
        };
    }
    else {
        nalu_callback = [this](uint8_t* data, int bytes) {
            ParseNalUnitH264(data, bytes);
        };
    }

    NalUnitCount += EnumerateAnnexBNalus(
        data,
        bytes,
        nalu_callback);
}

void VideoParser::ParseNalUnitH264(uint8_t* data, int bytes)
{
    if (bytes < 1) {
        spdlog::error("Encoder produced invalid truncated NALU");
        return; // Invalid
    }
    const uint8_t header = data[0];

    if ((header & 0x80) != 0) {
        spdlog::error("Encoder produced invalid highbit NALU");
        return; // Invalid
    }

    //const unsigned nal_ref_idc = (header >> 5) & 3;
    const unsigned nal_unit_type = header & 0x1f;
    //spdlog::info("NALU {} bytes nal_unit_type={} nal_ref_idc={}", bytes, nal_unit_type, nal_ref_idc);

    switch (nal_unit_type)
    {
    case 7: // Fall-thru
    case 8: // Fall-thru
        Parameters.emplace_back(data - 3, bytes + 3);
        TotalParameterBytes += bytes + 3;
        break;
    case 5: // Fall-thru
    case 1: // Fall-thru
        {
            ReadBitStream bs(data + 1, bytes - 1);
            unsigned first_mb_in_slice = ReadExpGolomb(bs);
            const bool FirstSlice = (first_mb_in_slice == 0);
            //unsigned slice_type = ReadExpGolomb(bs);
            //spdlog::info("first_mb_in_slice={} slice_type={}", first_mb_in_slice, slice_type);
            // We are at the start of a new image when first_mb_in_slice = 0

            AppendSlice(data - 3, bytes + 3, FirstSlice);
        }
        break;
    case 9: // Ignoring AUD
        break;
    case 6: // Stripping out SEI
        // We strip SEI because this is used for the decoder to buffer up a
        // number of frames so no I-frames are needed.  However we put parameter
        // sets in front of real I-frames so SEI is not needed.
        break;
    default:
        spdlog::warn("Unhandled AVC NAL unit {} in encoder output ignored", nal_unit_type);
        break;
    }
}

void VideoParser::ParseNalUnitHEVC(uint8_t* data, int bytes)
{
    if (bytes < 2) {
        spdlog::error("Encoder produced invalid truncated NALU");
        return; // Invalid
    }
    const uint16_t header = ReadU16_BE(data);

    if ((header & 0x8000) != 0) {
        spdlog::error("Encoder produced invalid highbit NALU");
        return; // Invalid
    }

    const unsigned nal_unit_type = (header >> 9) & 0x3f;
    //const unsigned nuh_layer_id = (header >> 3) & 0x3f;
    //const unsigned nul_temporal_id = header & 7;
    //spdlog::info("NALU: {} bytes nal_unit_type={}", bytes, nal_unit_type);

    switch (nal_unit_type)
    {
    case 32: // Fall-thru
    case 33: // Fall-thru
    case 34: // Fall-thru
        Parameters.emplace_back(data - 3, bytes + 3);
        TotalParameterBytes += bytes + 3;
        break;
    case 19: // Fall-thru
    case 1: // Fall-thru
    case 21: // Fall-thru
        {
            ReadBitStream bs(data + 2, bytes - 2);
            const bool FirstSlice = (bs.Read(1) != 0);
            //spdlog::info("FirstSlice = {}", FirstSlice);

            AppendSlice(data - 3, bytes + 3, FirstSlice);
        }
        break;
    case 35: // Ignoring AUD
        break;
    case 39: // Stripping out SEI
        // We strip SEI because this is used for the decoder to buffer up a
        // number of frames so no I-frames are needed.  However we put parameter
        // sets in front of real I-frames so SEI is not needed.
        break;
    default:
        spdlog::warn("Unhandled HEVC NAL unit {} in encoder output ignored", nal_unit_type);
        break;
    }
}


} // namespace core
