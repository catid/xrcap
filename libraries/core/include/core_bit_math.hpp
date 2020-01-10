// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "core.hpp"

#include <cstdint>


namespace core {


//------------------------------------------------------------------------------
// Bit Tools

// Preconditions: 0 <= k < 16
CORE_INLINE uint16_t rotl16(const uint16_t x, int k) {
    return (x << k) | (x >> (16 - k));
}
CORE_INLINE uint16_t rotr16(const uint16_t x, int k) {
    return (x >> k) | (x << (16 - k));
}

// Preconditions: 0 <= k < 32
CORE_INLINE uint32_t rotl32(const uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
}
CORE_INLINE uint32_t rotr32(const uint32_t x, int k) {
    return (x >> k) | (x << (32 - k));
}

// Preconditions: 0 <= k < 64
CORE_INLINE uint64_t rotl64(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}
CORE_INLINE uint64_t rotr64(const uint64_t x, int k) {
    return (x >> k) | (x << (64 - k));
}


//------------------------------------------------------------------------------
// Intrinsics Wrappers

/// Returns first position with set bit (LSB = 0)
/// Precondition: x != 0
CORE_INLINE unsigned NonzeroLowestBitIndex(uint32_t x)
{
#ifdef _MSC_VER
    unsigned long index;
    // Note: Ignoring result because x != 0
    _BitScanReverse(&index, x);
    return (unsigned)index;
#else
    // Note: Ignoring return value of 0 because x != 0
    return 31 - (unsigned)__builtin_clz(x);
#endif
}


//------------------------------------------------------------------------------
// Integer Hash Functions

// 32-bit integer hash
// exact bias: 0.020888578919738908
CORE_INLINE uint32_t wellons_triple32(uint32_t x)
{
    x ^= x >> 17;
    x *= UINT32_C(0xed5ad4bb);
    x ^= x >> 11;
    x *= UINT32_C(0xac4c1b51);
    x ^= x >> 15;
    x *= UINT32_C(0x31848bab);
    x ^= x >> 14;
    return x;
}


} // namespace core
