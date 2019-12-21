// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "core.hpp"
#include "core_bit_math.hpp" // NonzeroLowestBitIndex
#include <string.h>

namespace core {


//------------------------------------------------------------------------------
// Byte Order

// Swaps byte order in a 16-bit word
CORE_INLINE uint16_t ByteSwap16(uint16_t word)
{
    return (word >> 8) | (word << 8);
}

// Swaps byte order in a 32-bit word
CORE_INLINE uint32_t ByteSwap32(uint32_t word)
{
    const uint16_t swapped_old_hi = ByteSwap16(static_cast<uint16_t>(word >> 16));
    const uint16_t swapped_old_lo = ByteSwap16(static_cast<uint16_t>(word));
    return (static_cast<uint32_t>(swapped_old_lo) << 16) | swapped_old_hi;
}

// Swaps byte order in a 64-bit word
CORE_INLINE uint64_t ByteSwap64(uint64_t word)
{
    const uint32_t swapped_old_hi = ByteSwap32(static_cast<uint32_t>(word >> 32));
    const uint32_t swapped_old_lo = ByteSwap32(static_cast<uint32_t>(word));
    return (static_cast<uint64_t>(swapped_old_lo) << 32) | swapped_old_hi;
}


//------------------------------------------------------------------------------
// POD Serialization

/**
 * array[2] = { 0, 1 }
 *
 * Little Endian: word = 0x0100 <- first byte is least-significant
 * Big Endian:    word = 0x0001 <- first byte is  most-significant
**/

/**
 * word = 0x0102
 *
 * Little Endian: array[2] = { 0x02, 0x01 }
 * Big Endian:    array[2] = { 0x01, 0x02 }
**/

// Little-endian 16-bit read
CORE_INLINE uint16_t ReadU16_LE(const void* data)
{
#ifdef CORE_ALIGNED_ACCESSES
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);
    return ((uint16_t)u8p[1] << 8) | u8p[0];
#else
    const uint16_t* word_ptr = reinterpret_cast<const uint16_t*>(data);
    return *word_ptr;
#endif
}

// Big-endian 16-bit read
CORE_INLINE uint16_t ReadU16_BE(const void* data)
{
    return ByteSwap16(ReadU16_LE(data));
}

// Little-endian 24-bit read
CORE_INLINE uint32_t ReadU24_LE(const void* data)
{
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);
    return ((uint32_t)u8p[2] << 16) | ((uint32_t)u8p[1] << 8) | u8p[0];
}

// Big-endian 24-bit read
CORE_INLINE uint32_t ReadU24_BE(const void* data)
{
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);
    return ((uint32_t)u8p[0] << 16) | ((uint32_t)u8p[1] << 8) | u8p[2];
}

// Little-endian 32-bit read
CORE_INLINE uint32_t ReadU32_LE(const void* data)
{
#ifdef CORE_ALIGNED_ACCESSES
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);
    return ((uint32_t)u8p[3] << 24) | ((uint32_t)u8p[2] << 16) | ((uint32_t)u8p[1] << 8) | u8p[0];
#else
    const uint32_t* u32p = reinterpret_cast<const uint32_t*>(data);
    return *u32p;
#endif
}

// Big-endian 32-bit read
CORE_INLINE uint32_t ReadU32_BE(const void* data)
{
#ifdef CORE_ALIGNED_ACCESSES
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);
    return ((uint32_t)u8p[0] << 24) | ((uint32_t)u8p[1] << 16) | ((uint32_t)u8p[2] << 8) | u8p[3];
#else
    return ByteSwap32(ReadU32_LE(data));
#endif
}

// Little-endian 64-bit read
CORE_INLINE uint64_t ReadU64_LE(const void* data)
{
#ifdef CORE_ALIGNED_ACCESSES
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);
    return ((uint64_t)u8p[7] << 56) | ((uint64_t)u8p[6] << 48) | ((uint64_t)u8p[5] << 40) |
           ((uint64_t)u8p[4] << 32) | ((uint64_t)u8p[3] << 24) | ((uint64_t)u8p[2] << 16) |
           ((uint64_t)u8p[1] << 8) | u8p[0];
#else
    const uint64_t* word_ptr = reinterpret_cast<const uint64_t*>(data);
    return *word_ptr;
#endif
}

// Big-endian 64-bit read
CORE_INLINE uint64_t ReadU64_BE(const void* data)
{
#ifdef CORE_ALIGNED_ACCESSES
    const uint8_t* u8p = reinterpret_cast<const uint8_t*>(data);
    return ((uint64_t)u8p[0] << 56) | ((uint64_t)u8p[1] << 48) | ((uint64_t)u8p[2] << 40) |
           ((uint64_t)u8p[3] << 32) | ((uint64_t)u8p[4] << 24) | ((uint64_t)u8p[5] << 16) |
           ((uint64_t)u8p[6] << 8) | u8p[7];
#else
    return ByteSwap64(ReadU64_LE(data));
#endif
}

// Little-endian 16-bit write
CORE_INLINE void WriteU16_LE(void* data, uint16_t value)
{
#ifdef CORE_ALIGNED_ACCESSES
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[1] = static_cast<uint8_t>(value >> 8);
    u8p[0] = static_cast<uint8_t>(value);
#else
    uint16_t* word_ptr = reinterpret_cast<uint16_t*>(data);
    *word_ptr = value;
#endif
}

// Big-endian 16-bit write
CORE_INLINE void WriteU16_BE(void* data, uint16_t value)
{
#ifdef CORE_ALIGNED_ACCESSES
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[0] = static_cast<uint8_t>(value >> 8);
    u8p[1] = static_cast<uint8_t>(value);
#else
    uint16_t* word_ptr = reinterpret_cast<uint16_t*>(data);
    *word_ptr = ByteSwap16(value);
#endif
}

// Little-endian 24-bit write
CORE_INLINE void WriteU24_LE(void* data, uint32_t value)
{
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[2] = static_cast<uint8_t>(value >> 16);
    WriteU16_LE(u8p, static_cast<uint16_t>(value));
}

// Big-endian 24-bit write
CORE_INLINE void WriteU24_BE(void* data, uint32_t value)
{
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[0] = static_cast<uint8_t>(value >> 16);
    WriteU16_BE(u8p + 1, static_cast<uint16_t>(value));
}

// Little-endian 32-bit write
CORE_INLINE void WriteU32_LE(void* data, uint32_t value)
{
#ifdef CORE_ALIGNED_ACCESSES
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[3] = (uint8_t)(value >> 24);
    u8p[2] = static_cast<uint8_t>(value >> 16);
    u8p[1] = static_cast<uint8_t>(value >> 8);
    u8p[0] = static_cast<uint8_t>(value);
#else
    uint32_t* word_ptr = reinterpret_cast<uint32_t*>(data);
    *word_ptr = value;
#endif
}

// Big-endian 32-bit write
CORE_INLINE void WriteU32_BE(void* data, uint32_t value)
{
#ifdef CORE_ALIGNED_ACCESSES
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[0] = (uint8_t)(value >> 24);
    u8p[1] = static_cast<uint8_t>(value >> 16);
    u8p[2] = static_cast<uint8_t>(value >> 8);
    u8p[3] = static_cast<uint8_t>(value);
#else
    uint32_t* word_ptr = reinterpret_cast<uint32_t*>(data);
    *word_ptr = ByteSwap32(value);
#endif
}

// Little-endian 64-bit write
CORE_INLINE void WriteU64_LE(void* data, uint64_t value)
{
#ifdef CORE_ALIGNED_ACCESSES
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[7] = static_cast<uint8_t>(value >> 56);
    u8p[6] = static_cast<uint8_t>(value >> 48);
    u8p[5] = static_cast<uint8_t>(value >> 40);
    u8p[4] = static_cast<uint8_t>(value >> 32);
    u8p[3] = static_cast<uint8_t>(value >> 24);
    u8p[2] = static_cast<uint8_t>(value >> 16);
    u8p[1] = static_cast<uint8_t>(value >> 8);
    u8p[0] = static_cast<uint8_t>(value);
#else
    uint64_t* word_ptr = reinterpret_cast<uint64_t*>(data);
    *word_ptr = value;
#endif
}

// Big-endian 64-bit write
CORE_INLINE void WriteU64_BE(void* data, uint64_t value)
{
#ifdef CORE_ALIGNED_ACCESSES
    uint8_t* u8p = reinterpret_cast<uint8_t*>(data);
    u8p[0] = static_cast<uint8_t>(value >> 56);
    u8p[1] = static_cast<uint8_t>(value >> 48);
    u8p[2] = static_cast<uint8_t>(value >> 40);
    u8p[3] = static_cast<uint8_t>(value >> 32);
    u8p[4] = static_cast<uint8_t>(value >> 24);
    u8p[5] = static_cast<uint8_t>(value >> 16);
    u8p[6] = static_cast<uint8_t>(value >> 8);
    u8p[7] = static_cast<uint8_t>(value);
#else
    uint64_t* word_ptr = reinterpret_cast<uint64_t*>(data);
    *word_ptr = ByteSwap64(value);
#endif
}

/**
 * ReadBytes64_LE()
 *
 * Little-endian variable-bit read up to 8 bytes into a 64-bit unsigned integer.
 *
 * If bytes > 8, the left-most bytes are taken (truncating the MSBs).
 *
 * Returns the value, truncated to 64 bits.
 */
uint64_t ReadBytes64_LE(const void* data, int bytes);

/**
 * WriteBytes64_BE()
 *
 * Little-endian variable-bit write up to 8 bytes from a 64-bit unsigned integer.
 *
 * WARNING: Does not support more than eight bytes.
 *
 * Precondition: 0 <= bytes <= 8
 * If 0 bytes is specified, no writes are performed.
 */
void WriteBytes64_LE(void* data, int bytes, uint64_t value);

/**
 * ReadBytes64_BE()
 *
 * Big-endian variable-bit read up to 8 bytes into a 64-bit unsigned integer.
 *
 * If bytes > 8, the right-most bytes are taken (truncating the MSBs).
 *
 * Returns the value, truncated to 64 bits.
 */
uint64_t ReadBytes64_BE(const void* data, int bytes);

/**
 * WriteBytes64_BE()
 *
 * Big-endian variable-bit write up to 8 bytes from a 64-bit unsigned integer.
 *
 * WARNING: Does not support more than eight bytes.
 *
 * Precondition: 0 <= bytes <= 8
 * If 0 bytes is specified, no writes are performed.
 */
void WriteBytes64_BE(void* data, int bytes, uint64_t value);


//------------------------------------------------------------------------------
// WriteByteStream

/// Helper class to serialize POD types to a byte buffer
struct WriteByteStream
{
    /// Wrapped data pointer
    uint8_t* Data = nullptr;

    /// Number of wrapped buffer bytes
    int BufferBytes = 0;

    /// Number of bytes written so far by Write*() functions
    int WrittenBytes = 0;

    CORE_INLINE WriteByteStream()
    {
    }
    CORE_INLINE WriteByteStream(const WriteByteStream& other)
        : Data(other.Data)
        , BufferBytes(other.BufferBytes)
        , WrittenBytes(other.WrittenBytes)
    {
    }
    CORE_INLINE WriteByteStream(void* data, int bytes)
        : Data(reinterpret_cast<uint8_t*>(data))
        , BufferBytes(bytes)
    {
        CORE_DEBUG_ASSERT(data != nullptr && bytes > 0);
    }

    CORE_INLINE uint8_t* Peek()
    {
        CORE_DEBUG_ASSERT(WrittenBytes <= BufferBytes);
        return Data + WrittenBytes;
    }
    CORE_INLINE int Remaining()
    {
        CORE_DEBUG_ASSERT(WrittenBytes <= BufferBytes);
        return BufferBytes - WrittenBytes;
    }

    CORE_INLINE WriteByteStream& Write8(uint8_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 1 <= BufferBytes);
        Data[WrittenBytes] = value;
        WrittenBytes++;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write16_LE(uint16_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 2 <= BufferBytes);
        WriteU16_LE(Data + WrittenBytes, value);
        WrittenBytes += 2;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write16_BE(uint16_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 2 <= BufferBytes);
        WriteU16_BE(Data + WrittenBytes, value);
        WrittenBytes += 2;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write24_LE(uint32_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 3 <= BufferBytes);
        WriteU24_LE(Data + WrittenBytes, value);
        WrittenBytes += 3;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write24_BE(uint32_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 3 <= BufferBytes);
        WriteU24_BE(Data + WrittenBytes, value);
        WrittenBytes += 3;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write32_LE(uint32_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 4 <= BufferBytes);
        WriteU32_LE(Data + WrittenBytes, value);
        WrittenBytes += 4;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write32_BE(uint32_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 4 <= BufferBytes);
        WriteU32_BE(Data + WrittenBytes, value);
        WrittenBytes += 4;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write64_LE(uint64_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 8 <= BufferBytes);
        WriteU64_LE(Data + WrittenBytes, value);
        WrittenBytes += 8;
        return *this;
    }
    CORE_INLINE WriteByteStream& Write64_BE(uint64_t value)
    {
        CORE_DEBUG_ASSERT(WrittenBytes + 8 <= BufferBytes);
        WriteU64_BE(Data + WrittenBytes, value);
        WrittenBytes += 8;
        return *this;
    }
    CORE_INLINE WriteByteStream& WriteBuffer(const void* source, int bytes)
    {
        CORE_DEBUG_ASSERT(source != nullptr || bytes == 0);
        CORE_DEBUG_ASSERT(WrittenBytes + bytes <= BufferBytes);
        memcpy(Data + WrittenBytes, source, bytes);
        WrittenBytes += bytes;
        return *this;
    }
};


//------------------------------------------------------------------------------
// ReadByteStream

/// Helper class to deserialize POD types from a byte buffer
struct ReadByteStream
{
    /// Wrapped data pointer
    const uint8_t* const Data;

    /// Number of wrapped buffer bytes
    const int BufferBytes;

    /// Number of bytes read so far by Read*() functions
    int BytesRead = 0;


    ReadByteStream(const void* data, int bytes)
        : Data(reinterpret_cast<const uint8_t*>(data))
        , BufferBytes(bytes)
    {
        CORE_DEBUG_ASSERT(data != nullptr);
    }

    CORE_INLINE const uint8_t* Peek()
    {
        CORE_DEBUG_ASSERT(BytesRead <= BufferBytes);
        return Data + BytesRead;
    }
    CORE_INLINE int Remaining()
    {
        CORE_DEBUG_ASSERT(BytesRead <= BufferBytes);
        return BufferBytes - BytesRead;
    }
    CORE_INLINE void Skip(int bytes)
    {
        CORE_DEBUG_ASSERT(BytesRead + bytes <= BufferBytes);
        BytesRead += bytes;
    }

    CORE_INLINE const uint8_t* Read(int bytes)
    {
        const uint8_t* data = Peek();
        Skip(bytes);
        return data;
    }
    CORE_INLINE uint8_t Read8()
    {
        CORE_DEBUG_ASSERT(BytesRead + 1 <= BufferBytes);
        uint8_t value = *Peek();
        BytesRead++;
        return value;
    }
    CORE_INLINE uint16_t Read16_LE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 2 <= BufferBytes);
        uint16_t value = ReadU16_LE(Peek());
        BytesRead += 2;
        return value;
    }
    CORE_INLINE uint16_t Read16_BE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 2 <= BufferBytes);
        uint16_t value = ReadU16_BE(Peek());
        BytesRead += 2;
        return value;
    }
    CORE_INLINE uint32_t Read24_LE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 3 <= BufferBytes);
        uint32_t value = ReadU24_LE(Peek());
        BytesRead += 3;
        return value;
    }
    CORE_INLINE uint32_t Read24_BE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 3 <= BufferBytes);
        uint32_t value = ReadU24_BE(Peek());
        BytesRead += 3;
        return value;
    }
    CORE_INLINE uint32_t Read32_LE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 4 <= BufferBytes);
        uint32_t value = ReadU32_LE(Peek());
        BytesRead += 4;
        return value;
    }
    CORE_INLINE uint32_t Read32_BE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 4 <= BufferBytes);
        uint32_t value = ReadU32_BE(Peek());
        BytesRead += 4;
        return value;
    }
    CORE_INLINE uint64_t Read64_LE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 8 <= BufferBytes);
        uint64_t value = ReadU64_LE(Peek());
        BytesRead += 8;
        return value;
    }
    CORE_INLINE uint64_t Read64_BE()
    {
        CORE_DEBUG_ASSERT(BytesRead + 8 <= BufferBytes);
        uint64_t value = ReadU64_BE(Peek());
        BytesRead += 8;
        return value;
    }
};


//------------------------------------------------------------------------------
// Integer Compression

/// Represent a 32-bit integer with 16 bits using fixed point.
/// 5 bits of exponent, representing offsets of 0..20.
/// 11 bits of mantissa, providing a precision of 1/2048 = 0.048828125%.
/// The return value will decompress within 0.1% of the input word
CORE_INLINE uint16_t FixedPointCompress32to16(uint32_t word)
{
    if (word == 0) {
        return 0;
    }

    const unsigned nonzeroBits = NonzeroLowestBitIndex(word) + 1;
    CORE_DEBUG_ASSERT(nonzeroBits >= 1 && nonzeroBits <= 32);

    if (nonzeroBits <= 11) {
        CORE_DEBUG_ASSERT(word < 2048);
        return (uint16_t)word;
    }

    const unsigned shift = nonzeroBits - 11;
    CORE_DEBUG_ASSERT(shift < 32);

    CORE_DEBUG_ASSERT((word >> shift) < 2048);

    return (uint16_t)((word >> shift) | (shift << 11));
}

CORE_INLINE uint32_t FixedPointDecompress16to32(uint16_t fpword)
{
    return (uint32_t)(((uint32_t)fpword & 2047) << ((uint32_t)fpword >> 11));
}

/// Represent a 16-bit integer with 8 bits using fixed point.
/// 4 bits of exponent, representing offsets of 0..15.
/// 4 bits of mantissa, providing a precision of 1/16 = 6.25%.
/// The return value will decompress within 13% of the input word
CORE_INLINE uint8_t FixedPointCompress16to8(uint16_t word)
{
    if (word == 0) {
        return 0;
    }

    const unsigned nonzeroBits = NonzeroLowestBitIndex(word) + 1;
    CORE_DEBUG_ASSERT(nonzeroBits >= 1 && nonzeroBits <= 16);

    if (nonzeroBits <= 4) {
        CORE_DEBUG_ASSERT(word < 16);
        return (uint8_t)word;
    }

    const unsigned shift = nonzeroBits - 4;
    CORE_DEBUG_ASSERT(shift < 16);

    CORE_DEBUG_ASSERT((word >> shift) < 16);

    return (uint8_t)((word >> shift) | (shift << 4));
}

CORE_INLINE uint16_t FixedPointDecompress8to16(uint8_t fpword)
{
    return (uint16_t)(((uint16_t)fpword & 15) << ((uint16_t)fpword >> 4));
}


//------------------------------------------------------------------------------
// ReadBitStream

/// Helper class to deserialize POD types from a bit buffer
struct ReadBitStream
{
    /// Based on ReadByteStream
    ReadByteStream Reader;

    /// Current workspace for reading
    uint64_t Workspace = 0;

    /// Number of unread bits in the workspace
    int WorkspaceRemaining = 0;


    ReadBitStream(const void* data, int bytes)
        : Reader(data, bytes)
    {
    }

    /// Read up to 32 bits.
    /// Precondition: bits > 0, bits <= 32
    uint32_t Read(int bits);
};


} // namespace core
