// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <cstdint>
#ifdef _WIN32
    #include <intrin.h> // __popcnt64
#endif

namespace core {


//------------------------------------------------------------------------------
// Portable Intrinsics

/// Returns number of bits set in the 64-bit value
inline unsigned PopCount64(uint64_t x)
{
#ifdef _MSC_VER
#ifdef _WIN64
    return (unsigned)__popcnt64(x);
#else
    return (unsigned)(__popcnt((uint32_t)x) + __popcnt((uint32_t)(x >> 32)));
#endif
#else // GCC
    return (unsigned)__builtin_popcountll(x);
#endif
}

/// Returns lowest bit index 0..63 where the first non-zero bit is found
/// Precondition: x != 0
inline unsigned TrailingZeros64(uint64_t x)
{
#ifdef _MSC_VER
#ifdef _WIN64
    unsigned long index;
    // Note: Ignoring result because x != 0
    _BitScanForward64(&index, x);
    return (unsigned)index;
#else
    unsigned long index;
    if (0 != _BitScanForward(&index, (uint32_t)x))
        return (unsigned)index;
    // Note: Ignoring result because x != 0
    _BitScanForward(&index, (uint32_t)(x >> 32));
    return (unsigned)index + 32;
#endif
#else
    // Note: Ignoring return value of 0 because x != 0
    return (unsigned)__builtin_ffsll(x) - 1;
#endif
}


//------------------------------------------------------------------------------
// CustomBitSet

/// Custom std::bitset implementation for speed
template<unsigned N>
struct CustomBitSet
{
    static const unsigned kValidBits = N;
    typedef uint64_t WordT;
    static const unsigned kWordBits = sizeof(WordT) * 8;
    static const unsigned kWords = (kValidBits + kWordBits - 1) / kWordBits;
    static const WordT kAllOnes = UINT64_C(0xffffffffffffffff);

    WordT Words[kWords];


    CustomBitSet()
    {
        ClearAll();
    }

    void ClearAll()
    {
        for (unsigned i = 0; i < kWords; ++i) {
            Words[i] = 0;
        }
    }
    void SetAll()
    {
        for (unsigned i = 0; i < kWords; ++i) {
            Words[i] = kAllOnes;
        }
    }
    void Set(unsigned bit)
    {
        const unsigned word = bit / kWordBits;
        const WordT mask = (WordT)1 << (bit % kWordBits);
        Words[word] |= mask;
    }
    void Clear(unsigned bit)
    {
        const unsigned word = bit / kWordBits;
        const WordT mask = (WordT)1 << (bit % kWordBits);
        Words[word] &= ~mask;
    }
    bool Check(unsigned bit) const
    {
        const unsigned word = bit / kWordBits;
        const WordT mask = (WordT)1 << (bit % kWordBits);
        return 0 != (Words[word] & mask);
    }

    /**
        Returns the popcount of the bits within the given range.

        bitStart < kValidBits: First bit to test
        bitEnd <= kValidBits: Bit to stop at (non-inclusive)
    */
    unsigned RangePopcount(unsigned bitStart, unsigned bitEnd)
    {
        static_assert(kWordBits == 64, "Update this");

        if (bitStart >= bitEnd) {
            return 0;
        }

        unsigned wordIndex = bitStart / kWordBits;
        const unsigned wordEnd = bitEnd / kWordBits;

        // Eliminate low bits of first word
        WordT word = Words[wordIndex] >> (bitStart % kWordBits);

        // Eliminate high bits of last word if there is just one word
        if (wordEnd == wordIndex) {
            return PopCount64(word << (kWordBits - (bitEnd - bitStart)));
        }

        // Count remainder of first word
        unsigned count = PopCount64(word);

        // Accumulate popcount of full words
        while (++wordIndex < wordEnd) {
            count += PopCount64(Words[wordIndex]);
        }

        // Eliminate high bits of last word if there is one
        const unsigned lastWordBits = bitEnd - wordIndex * kWordBits;
        if (lastWordBits > 0) {
            count += PopCount64(Words[wordIndex] << (kWordBits - lastWordBits));
        }

        return count;
    }

    /**
        Returns the bit index where the first cleared bit is found.
        Returns kValidBits if all bits are set.

        bitStart < kValidBits: Index to start looking
    */
    unsigned FindFirstClear(const unsigned bitStart)
    {
        static_assert(kWordBits == 64, "Update this");

        const unsigned wordStart = bitStart / kWordBits;

        WordT word = ~Words[wordStart] >> (bitStart % kWordBits);
        if (word != 0)
        {
            unsigned offset = 0;
            if ((word & 1) == 0) {
                offset = TrailingZeros64(word);
            }
            return bitStart + offset;
        }

        for (unsigned i = wordStart + 1; i < kWords; ++i)
        {
            word = ~Words[i];
            if (word != 0) {
                return i * kWordBits + TrailingZeros64(word);
            }
        }

        return kValidBits;
    }

    /**
        Returns the bit index where the first set bit is found.
        Returns 'bitEnd' if all bits are clear.

        bitStart < kValidBits: Index to start looking
        bitEnd <= kValidBits: Index to stop looking at
    */
    unsigned FindFirstSet(unsigned bitStart, unsigned bitEnd = kValidBits)
    {
        static_assert(kWordBits == 64, "Update this");

        unsigned wordStart = bitStart / kWordBits;

        WordT word = Words[wordStart] >> (bitStart % kWordBits);
        if (word != 0)
        {
            unsigned offset = 0;
            if ((word & 1) == 0) {
                offset = TrailingZeros64(word);
            }
            return bitStart + offset;
        }

        const unsigned wordEnd = (bitEnd + kWordBits - 1) / kWordBits;

        for (unsigned i = wordStart + 1; i < wordEnd; ++i)
        {
            word = Words[i];
            if (word != 0) {
                return i * kWordBits + TrailingZeros64(word);
            }
        }

        return bitEnd;
    }

    /**
        Set a range of bits

        bitStart < kValidBits: Index at which to start setting
        bitEnd <= kValidBits: Bit to stop at (non-inclusive)
    */
    void SetRange(unsigned bitStart, unsigned bitEnd)
    {
        if (bitStart >= bitEnd) {
            return;
        }

        unsigned wordStart = bitStart / kWordBits;
        const unsigned wordEnd = bitEnd / kWordBits;

        bitStart %= kWordBits;

        if (wordEnd == wordStart)
        {
            // This implies x=(bitStart % kWordBits) and y=(bitEnd % kWordBits)
            // are in the same word.  Also: x < y, y < 64, y - x < 64.
            bitEnd %= kWordBits;
            WordT mask = ((WordT)1 << (bitEnd - bitStart)) - 1; // 1..63 bits
            mask <<= bitStart;
            Words[wordStart] |= mask;
            return;
        }

        // Set the end of the first word
        Words[wordStart] |= kAllOnes << bitStart;

        // Whole words at a time
        for (unsigned i = wordStart + 1; i < wordEnd; ++i) {
            Words[i] = kAllOnes;
        }

        // Set first few bits of the last word
        unsigned lastWordBits = bitEnd - wordEnd * kWordBits;
        if (lastWordBits > 0)
        {
            WordT mask = ((WordT)1 << lastWordBits) - 1; // 1..63 bits
            Words[wordEnd] |= mask;
        }
    }

    /**
        Clear a range of bits

        bitStart < kValidBits: Index at which to start clearing
        bitEnd <= kValidBits: Bit to stop at (non-inclusive)
    */
    void ClearRange(unsigned bitStart, unsigned bitEnd)
    {
        if (bitStart >= bitEnd) {
            return;
        }

        unsigned wordStart = bitStart / kWordBits;
        const unsigned wordEnd = bitEnd / kWordBits;

        bitStart %= kWordBits;

        if (wordEnd == wordStart)
        {
            // This implies x=(bitStart % kWordBits) and y=(bitEnd % kWordBits)
            // are in the same word.  Also: x < y, y < 64, y - x < 64.
            bitEnd %= kWordBits;
            WordT mask = ((WordT)1 << (bitEnd - bitStart)) - 1; // 1..63 bits
            mask <<= bitStart;
            Words[wordStart] &= ~mask;
            return;
        }

        // Clear the end of the first word
        Words[wordStart] &= ~(kAllOnes << bitStart);

        // Whole words at a time
        for (unsigned i = wordStart + 1; i < wordEnd; ++i) {
            Words[i] = 0;
        }

        // Clear first few bits of the last word
        unsigned lastWordBits = bitEnd - wordEnd * kWordBits;
        if (lastWordBits > 0)
        {
            WordT mask = ((WordT)1 << lastWordBits) - 1; // 1..63 bits
            Words[wordEnd] &= ~mask;
        }
    }
};


} // namespace core
