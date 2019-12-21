/** \file
    \brief Tonk Encryption
    \copyright Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of Tonk nor the names of its contributors may be
      used to endorse or promote products derived from this software without
      specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#include "TonkineseEncryption.h"

#include "SiameseSerializers.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC target("sse2")
#pragma GCC target("aes")
#include <x86intrin.h>
#endif

#if defined(_MSC_VER)
#include <wmmintrin.h>
#include <immintrin.h>
#endif

namespace tonk {


//------------------------------------------------------------------------------
// AES-NI round function

// Simpira v2 with b = 3 AES-based 384-bit permutation
// https://eprint.iacr.org/2016/122.pdf

#ifdef ENABLE_SIMPIRA384

#define SIMPIRA_F(C, B, X, Z)                                                \
    _mm_aesenc_si128(                                                        \
        _mm_aesenc_si128((X),                                                \
                         _mm_set_epi32(0x00 ^ (C) ^ (B), 0x10 ^ (C) ^ (B),   \
                                       0x20 ^ (C) ^ (B), 0x30 ^ (C) ^ (B))), \
        (Z))

static inline void _simpira384_round(__m128i state[3], __m128i z, unsigned R)
{
    state[(R + 1) % 3] = _mm_xor_si128(state[(R + 1) % 3], SIMPIRA_F(R + 1, 3, state[R % 3], z));
}

static inline void _simpira384_permute(__m128i state[3])
{
    // 8 round attack: https://eprint.iacr.org/2016/1161.pdf
    // 10 round attack: https://link.springer.com/chapter/10.1007/978-3-319-60055-0_20
    const __m128i z = _mm_setzero_si128();
    for (unsigned R = 0; R <= 20; ++R) {
        _simpira384_round(state, z, R);
    }
}

static void permute(uint8_t state_u8[48])
{
    __m128i state[3];
    state[0] = _mm_loadu_si128((const __m128i *)&state_u8[0]);
    state[1] = _mm_loadu_si128((const __m128i *)&state_u8[16]);
    state[2] = _mm_loadu_si128((const __m128i *)&state_u8[32]);
    _simpira384_permute(state);
    _mm_storeu_si128((__m128i *)&state_u8[0], state[0]);
    _mm_storeu_si128((__m128i *)&state_u8[16], state[1]);
    _mm_storeu_si128((__m128i *)&state_u8[32], state[2]);
}

#endif // ENABLE_SIMPIRA384


//------------------------------------------------------------------------------
// Charm

// Inspired by: https://github.com/jedisct1/charm
// Adds support for associated data (uc_tag)

static inline bool equals(const uint8_t a[16], const uint8_t b[16], size_t len)
{
    uint8_t d = 0;
    for (size_t i = 0; i < len; i++) {
        d |= a[i] ^ b[i];
    }
    return d == 0;
}

static inline void mem_cpy(uint8_t* dst, const uint8_t* src, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static inline void xor128(void* out, const void* a, const void* b)
{
    _mm_storeu_si128((__m128i*) out,
                     _mm_xor_si128(_mm_loadu_si128((const __m128i*) a),
                                   _mm_loadu_si128((const __m128i*) b)));
}

static inline void squeeze_permute(uint8_t st[48], unsigned char dst[16])
{
    memcpy(dst, st, 16);
    permute(st);
}

// key: 32 bytes
// iv: 16 bytes
static void uc_state_init(uint8_t st[48], const uint8_t* key, const uint8_t* iv)
{
    memcpy(&st[0], iv, 16);
    memcpy(&st[16], key, 32);
    permute(st);
}

static void uc_tag(
    uint8_t st[48],
    const uint8_t* src,
    size_t msg_len)
{
    uint8_t padded[16 + 1];
    size_t  off = 0;
    size_t  leftover;

    if (msg_len > 16) {
        for (; off < msg_len - 16; off += 16) {
            xor128(st, st, src + off);
            permute(st);
        }
    }
    leftover = msg_len - off;
    memset(padded, 0, 16);
    mem_cpy(padded, src + off, leftover);
    padded[leftover] = 0x80;
    xor128(st, st, padded);
    st[47] ^= (1UL | (uint32_t) leftover >> 4 << 1 | 1UL << 2);
    permute(st);
}

static void uc_encrypt(
    uint8_t st[48],
    uint8_t* dest,
    const uint8_t* src,
    size_t msg_len)
{
    uint8_t squeezed[16];
    uint8_t padded[16 + 1];
    size_t  off = 0;
    size_t  leftover;

    if (msg_len > 16) {
        for (; off < msg_len - 16; off += 16) {
            memcpy(squeezed, st, 16);
            xor128(st, st, src + off);
            xor128(dest + off, src + off, squeezed);
            permute(st);
        }
    }
    leftover = msg_len - off;
    memset(padded, 0, 16);
    mem_cpy(padded, src + off, leftover);
    padded[leftover] = 0x80;
    memcpy(squeezed, st, 16);
    xor128(st, st, padded);
    st[47] ^= (1UL | (uint32_t) leftover >> 4 << 1 | 1UL << 2);
    xor128(padded, padded, squeezed);
    mem_cpy(dest + off, padded, leftover);
    permute(st);
}

static void uc_decrypt(
    uint8_t st[48],
    uint8_t* dest,
    const uint8_t* src,
    size_t msg_len)
{
    uint8_t squeezed[16];
    uint8_t padded[16 + 1];
    size_t  off = 0;
    size_t  leftover;

    if (msg_len > 16) {
        for (; off < msg_len - 16; off += 16) {
            memcpy(squeezed, st, 16);
            xor128(dest + off, src + off, squeezed);
            xor128(st, st, dest + off);
            permute(st);
        }
    }
    leftover = msg_len - off;
    memset(padded, 0, 16);
    mem_cpy(padded, src + off, leftover);
    memset(squeezed, 0, 16);
    mem_cpy(squeezed, (const uint8_t*)st, leftover);
    xor128(padded, padded, squeezed);
    padded[leftover] = 0x80;
    xor128(st, st, padded);
    st[47] ^= (1UL | leftover >> 4 << 1 | 1UL << 2);
    mem_cpy(dest + off, padded, leftover);
    permute(st);
}


//------------------------------------------------------------------------------
// TonkEncryptionKey

TonkEncryptionKey::~TonkEncryptionKey()
{
    memset(Key, 0, sizeof(Key));
}

TonkEncryptionKey& TonkEncryptionKey::operator=(const TonkEncryptionKey& other)
{
    memcpy(Key, other.Key, sizeof(Key));
    return *this;
}

void TonkEncryptionKey::SetInsecureKey(uint64_t key)
{
    memset(Key, 0, sizeof(Key));
    siamese::WriteU64_LE(Key, key);
}

void TonkEncryptionKey::SetSecureKey(const uint8_t* key, unsigned bytes)
{
    memset(Key, 0, sizeof(Key));
    if (bytes > 32) {
        bytes = 32;
    }
    memcpy(Key, key, bytes);
}


//------------------------------------------------------------------------------
// TonkEncryption

TonkEncryption::~TonkEncryption()
{
    memset(State, 0, sizeof(State));
}

void TonkEncryption::Start(const TonkEncryptionKey& key, uint64_t iv_low)
{
    uint8_t iv[16] = {0};
    siamese::WriteU64_LE(iv, iv_low);
    uc_state_init(State, key.GetKey(), iv);
}

void TonkEncryption::Tag(const uint8_t* src, unsigned bytes)
{
    uc_tag(State, src, bytes);
}

void TonkEncryption::Encrypt(uint8_t* dest, const uint8_t* src, unsigned bytes)
{
    uc_encrypt(State, dest, src, bytes);
}

void TonkEncryption::EncryptFinalize(uint8_t* tag)
{
    squeeze_permute(State, tag);
}

void TonkEncryption::Decrypt(uint8_t* dest, const uint8_t* src, unsigned bytes)
{
    uc_decrypt(State, dest, src, bytes);
}

bool TonkEncryption::DecryptFinalize(const uint8_t* tag)
{
    uint8_t expected[16];
    squeeze_permute(State, expected);

    return equals(expected, tag, 16);
}


} // namespace tonk
