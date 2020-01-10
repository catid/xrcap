// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core_serializer.hpp"

namespace core {


//------------------------------------------------------------------------------
// POD Serialization

uint64_t ReadBytes64_LE(const void* data, int bytes)
{
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);

    uint64_t value = 0;

    // Read the specified number of bytes
    switch (bytes)
    {
    default:
        // Longer values are truncated.
        // The MSB is on the right, so we can read from the left.
        // fall-thru
    case 8:
        return ReadU64_LE(u8p);

    case 7:
        value |= static_cast<uint64_t>(u8p[6]) << 48;
        // fall-thru
    case 6:
        value |= static_cast<uint64_t>(ReadU16_LE(u8p + 4)) << 32;
        return value | ReadU32_LE(u8p);

    case 5:
        value |= static_cast<uint64_t>(u8p[4]) << 32;
        // fall-thru
    case 4:
        return value | ReadU32_LE(u8p);

    case 3:
        value |= static_cast<uint64_t>(u8p[2]) << 16;
        // fall-thru
    case 2:
        return value | ReadU16_LE(u8p);

    case 1:
        return u8p[0];

    case 0:
        break; // Weird encoding but valid
    }

    return 0;
}

void WriteBytes64_LE(void* data, int bytes, uint64_t value)
{
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);

    // Write the specified number of bytes
    switch (bytes)
    {
    default:
        CORE_DEBUG_BREAK(); // Invalid input
        // fall-thru
    case 8:
        WriteU64_LE(u8p, value);
        break;

    case 7:
        u8p[6] = static_cast<uint8_t>(value >> 48);
        // fall-thru
    case 6:
        WriteU16_LE(u8p + 4, static_cast<uint16_t>(value >> 32));
        WriteU32_LE(u8p, static_cast<uint32_t>(value));
        break;

    case 5:
        u8p[4] = static_cast<uint8_t>(value >> 32);
        // fall-thru
    case 4:
        WriteU32_LE(u8p, static_cast<uint32_t>(value));
        break;

    case 3:
        u8p[2] = static_cast<uint8_t>(value >> 16);
        // fall-thru
    case 2:
        WriteU16_LE(u8p, static_cast<uint16_t>(value));
        break;

    case 1:
        u8p[0] = static_cast<uint8_t>(value);
        break;

    case 0:
        // Value remains 0
        break; // Weird encoding but valid
    }
}

uint64_t ReadBytes64_BE(const void* data, int bytes)
{
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);

    uint64_t value = 0;

    // Read the specified number of bytes
    switch (bytes)
    {
    default:
        // Longer values are truncated.
        // The MSB is on the left, so we need to align to the right.
        u8p += bytes - 8;
        // fall-thru
    case 8:
        return ReadU64_BE(u8p);

    case 7:
        value |= static_cast<uint64_t>(u8p[0]) << 48;
        ++u8p;
        // fall-thru
    case 6:
        value |= static_cast<uint64_t>(ReadU16_BE(u8p)) << 32;
        return value | ReadU32_BE(u8p + 2);

    case 5:
        value |= static_cast<uint64_t>(u8p[0]) << 32;
        ++u8p;
        // fall-thru
    case 4:
        return value | ReadU32_BE(u8p);

    case 3:
        value |= static_cast<uint64_t>(u8p[0]) << 16;
        ++u8p;
        // fall-thru
    case 2:
        return value | ReadU16_BE(u8p);

    case 1:
        return u8p[0];

    case 0:
        break; // Weird encoding but valid
    }

    return 0;
}

void WriteBytes64_BE(void* data, int bytes, uint64_t value)
{
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);

    // Write the specified number of bytes
    switch (bytes)
    {
    default:
        CORE_DEBUG_BREAK(); // Invalid input
        // fall-thru
    case 8:
        WriteU64_BE(u8p, value);
        break;

    case 7:
        u8p[0] = static_cast<uint8_t>(value >> 48);
        ++u8p;
        // fall-thru
    case 6:
        WriteU16_BE(u8p, static_cast<uint16_t>(value >> 32));
        WriteU32_BE(u8p + 2, static_cast<uint32_t>(value));
        break;

    case 5:
        u8p[0] = static_cast<uint8_t>(value >> 32);
        ++u8p;
        // fall-thru
    case 4:
        WriteU32_BE(u8p, static_cast<uint32_t>(value));
        break;

    case 3:
        u8p[0] = static_cast<uint8_t>(value >> 16);
        ++u8p;
        // fall-thru
    case 2:
        WriteU16_BE(u8p, static_cast<uint16_t>(value));
        break;

    case 1:
        u8p[0] = static_cast<uint8_t>(value);
        break;

    case 0:
        // Value remains 0
        break; // Weird encoding but valid
    }
}


//------------------------------------------------------------------------------
// ReadBitStream

uint32_t ReadBitStream::Read(int bits)
{
    CORE_DEBUG_ASSERT(bits <= 32 && bits > 0);

    if (bits <= WorkspaceRemaining)
    {
        WorkspaceRemaining -= bits;

        // Grab bits from top of workspace
        const uint32_t result = static_cast<uint32_t>(Workspace >> (64 - bits));

        // Eat bits
        Workspace <<= bits;

        return result;
    }

    const int old_offset = WorkspaceRemaining;

    // Read next word
    uint64_t word;
    const int reader_remaining = Reader.Remaining();
    if (reader_remaining >= 8) {
        word = Reader.Read64_BE();
        WorkspaceRemaining += 64;
    } else if (reader_remaining > 0) {
        word = ReadBytes64_BE(Reader.Read(reader_remaining), reader_remaining);

        // Align to top of word
        word <<= (8 - reader_remaining) * 8;

        WorkspaceRemaining += 8 * reader_remaining;
    } else {
        //CORE_DEBUG_BREAK(); // Buffer overrun
        return 0;
    }

    CORE_DEBUG_ASSERT(WorkspaceRemaining >= bits);

    // Combine old and new bits
    const uint64_t combined = Workspace | (word >> old_offset);

    WorkspaceRemaining -= bits;

    const uint32_t result = static_cast<uint32_t>(combined >> (64 - bits));

    CORE_DEBUG_ASSERT(bits > old_offset);

    Workspace = word << (bits - old_offset);

    return result;
}


} // namespace core
