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

#pragma once

#include "TonkineseTools.h"

namespace tonk {


//------------------------------------------------------------------------------
// Constants

//#define NATIVE_BIG_ENDIAN

#define ENABLE_SIMPIRA384 /* Fastest: Depends on AES-NI CPU instruction */


//------------------------------------------------------------------------------
// Tonk Encryption

/**
    This is a symmetric AEAD scheme based on jedisct1's `charm` library:
    https://github.com/jedisct1/charm

    Rationale:

    Applications have varied ways to establish session keys (PAKE, DH, etc) so
    that is left up to the application.  Instead we focus on how to secure the
    data and the framing of the data in the usual ways.

    Discussion:

    The encryption mainly serves to hide the message content from tools that
    can observe datagram contents, inject, and modify, but have no knowledge
    of the custom protocol.  This makes it much harder to attack the netcode,
    as the attacker would have to reverse-engineer the software.

    Tag validation can serve as another guard against accepting packets
    accidentally from previous or parallel connections that originate from
    the same host address.
*/

// Key that clears itself when it goes out of scope
class TonkEncryptionKey
{
public:
    ~TonkEncryptionKey();

    TonkEncryptionKey& operator=(const TonkEncryptionKey& other);

    /// Set a default insecure key
    void SetInsecureKey(uint64_t key);

    /// Set a secure key
    void SetSecureKey(const uint8_t* key, unsigned bytes);

    const uint8_t* GetKey() const
    {
        return Key;
    }

protected:
    uint8_t Key[32];
};

// Encryption session supporting multiple keys
// that clears state when it goes out of scope
class TonkEncryption
{
public:
    ~TonkEncryption();

    /// Start encrypting or decrypting with an IV
    void Start(const TonkEncryptionKey& key, uint64_t iv);

    /// Accumulate associated data without encryption/decryption
    void Tag(const uint8_t* src, unsigned bytes);


    /// Encrypt (in-place) and generate 16 byte tag
    void Encrypt(uint8_t* dest, const uint8_t* src, unsigned bytes);

    /// Finalize encryption, generating a 16 byte tag
    void EncryptFinalize(uint8_t* tag);


    /// Decrypt (in-place) and check 16 byte tag
    void Decrypt(uint8_t* dest, const uint8_t* src, unsigned bytes);

    /// Finalize decryption, checking 16 byte tag
    /// Returns true on success
    bool DecryptFinalize(const uint8_t* tag);

protected:
    uint8_t State[48];
};


} // namespace tonk
