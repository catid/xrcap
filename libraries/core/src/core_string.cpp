// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core_string.hpp"
#include "core_serializer.hpp"

#include <fstream> // ofstream
#include <iomanip> // setw, setfill
#include <cctype>

namespace core {


//------------------------------------------------------------------------------
// String Conversion

static const char* HEX_ASCII = "0123456789abcdef";

std::string HexString(uint64_t value)
{
    char hex[16 + 1];
    hex[16] = '\0';

    char* hexWrite = &hex[14];
    for (unsigned i = 0; i < 8; ++i)
    {
        hexWrite[1] = HEX_ASCII[value & 15];
        hexWrite[0] = HEX_ASCII[(value >> 4) & 15];

        value >>= 8;
        if (value == 0)
            return hexWrite;
        hexWrite -= 2;
    }

    return hex;
}

std::string HexDump(const uint8_t* data, int bytes)
{
    std::ostringstream oss;

    char hex[8 * 3];

    while (bytes > 8)
    {
        const uint64_t word = ReadU64_LE(data);

        for (unsigned i = 0; i < 8; ++i)
        {
            const uint8_t value = static_cast<uint8_t>(word >> (i * 8));
            hex[i * 3] = HEX_ASCII[(value >> 4) & 15];
            hex[i * 3 + 1] = HEX_ASCII[value & 15];
            hex[i * 3 + 2] = ' ';
        }

        oss.write(hex, 8 * 3);

        data += 8;
        bytes -= 8;
    }

    if (bytes > 0) {
        const uint64_t word = ReadBytes64_LE(data, bytes);

        for (int i = 0; i < bytes; ++i)
        {
            const uint8_t value = static_cast<uint8_t>(word >> (i * 8));
            hex[i * 3] = HEX_ASCII[(value >> 4) & 15];
            hex[i * 3 + 1] = HEX_ASCII[value & 15];
            hex[i * 3 + 2] = ' ';
        }

        oss.write(hex, bytes * 3);
    }

    return oss.str();
}


//------------------------------------------------------------------------------
// Conversion to Base64

static const char TO_BASE64[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};


int GetBase64LengthFromByteCount(int bytes)
{
    if (bytes <= 0) {
        return 0;
    }

    return ((bytes + 2) / 3) * 4;
}

int WriteBase64(
    const void* buffer,
    int bytes,
    char* encoded_buffer,
    int encoded_bytes)
{
    int written_bytes = ((bytes + 2) / 3) * 4;

    if (bytes <= 0 || encoded_bytes < written_bytes) {
        return 0;
    }

    const uint8_t *data = reinterpret_cast<const uint8_t*>( buffer );

    int ii, jj, end;
    for (ii = 0, jj = 0, end = bytes - 2; ii < end; ii += 3, jj += 4)
    {
        encoded_buffer[jj] = TO_BASE64[data[ii] >> 2];
        encoded_buffer[jj+1] = TO_BASE64[((data[ii] << 4) | (data[ii+1] >> 4)) & 0x3f];
        encoded_buffer[jj+2] = TO_BASE64[((data[ii+1] << 2) | (data[ii+2] >> 6)) & 0x3f];
        encoded_buffer[jj+3] = TO_BASE64[data[ii+2] & 0x3f];
    }

    switch (ii - end)
    {
        default:
        case 2: // Nothing to write
            break;

        case 1: // Need to write final 1 byte
            encoded_buffer[jj] = TO_BASE64[data[bytes-1] >> 2];
            encoded_buffer[jj+1] = TO_BASE64[(data[bytes-1] << 4) & 0x3f];
            encoded_buffer[jj+2] = '=';
            encoded_buffer[jj+3] = '=';
            break;

        case 0: // Need to write final 2 bytes
            encoded_buffer[jj] = TO_BASE64[data[bytes-2] >> 2];
            encoded_buffer[jj+1] = TO_BASE64[((data[bytes-2] << 4) | (data[bytes-1] >> 4)) & 0x3f];
            encoded_buffer[jj+2] = TO_BASE64[(data[bytes-1] << 2) & 0x3f];
            encoded_buffer[jj+3] = '=';
            break;
    }
    return written_bytes;
}

// This version writes a C string null-terminator
int WriteBase64Str(
    const void* buffer,
    int bytes,
    char* encoded_buffer,
    int encoded_bytes)
{
    const int written = WriteBase64(
        buffer,
        bytes,
        encoded_buffer,
        encoded_bytes - 1);

    encoded_buffer[written] = '\0';

    return written;
}


//------------------------------------------------------------------------------
// Conversion from Base64

#define DC 0

static const uint8_t FROM_BASE64[256] = {
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, // 0-15
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, // 16-31
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, 62, DC, DC, DC, 63, // 32-47
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, DC, DC, DC, DC, DC, DC, // 48-63
    DC,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 64-79
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, DC, DC, DC, DC, DC, // 80-95
    DC, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, // 96-111
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, DC, DC, DC, DC, DC, // 112-127
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, // 128-
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, // Extended
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, // ASCII
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC,
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC,
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC,
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC,
    DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC, DC
};

#undef DC

int GetByteCountFromBase64(const char* encoded_buffer, int bytes)
{
    if (bytes <= 0) return 0;

    // Skip characters from end until one is a valid BASE64 character
    while (bytes >= 1) {
        unsigned char ch = encoded_buffer[bytes - 1];

        if (FROM_BASE64[ch] != 0 || ch == 'A') {
            break;
        }

        --bytes;
    }

    // Round down because we pad out the high bits with zeros
    return (bytes * 6) / 8;
}

int ReadBase64(
    const char* encoded_buffer,
    int encoded_bytes,
    void* decoded_buffer)
{
    // Skip characters from end until one is a valid BASE64 character
    while (encoded_bytes >= 1) {
        unsigned char ch = encoded_buffer[encoded_bytes - 1];

        if (FROM_BASE64[ch] != 0 || ch == 'A') {
            break;
        }

        --encoded_bytes;
    }

    if (encoded_bytes <= 0) {
        return 0;
    }

    const uint8_t* from = reinterpret_cast<const uint8_t*>( encoded_buffer );
    uint8_t* to = reinterpret_cast<uint8_t*>( decoded_buffer );

    int ii, jj, ii_end = encoded_bytes - 3;
    for (ii = 0, jj = 0; ii < ii_end; ii += 4, jj += 3)
    {
        const uint8_t a = FROM_BASE64[from[ii]];
        const uint8_t b = FROM_BASE64[from[ii+1]];
        const uint8_t c = FROM_BASE64[from[ii+2]];
        const uint8_t d = FROM_BASE64[from[ii+3]];

        to[jj] = (a << 2) | (b >> 4);
        to[jj+1] = (b << 4) | (c >> 2);
        to[jj+2] = (c << 6) | d;
    }

    switch (encoded_bytes & 3)
    {
    case 3: // 3 characters left
        {
            const uint8_t a = FROM_BASE64[from[ii]];
            const uint8_t b = FROM_BASE64[from[ii+1]];
            const uint8_t c = FROM_BASE64[from[ii+2]];

            to[jj] = (a << 2) | (b >> 4);
            to[jj+1] = (b << 4) | (c >> 2);
            return jj + 2;
        }
    case 2: // 2 characters left
        {
            const uint8_t a = FROM_BASE64[from[ii]];
            const uint8_t b = FROM_BASE64[from[ii+1]];

            to[jj] = (a << 2) | (b >> 4);
            return jj + 1;
        }
    }

    return jj;
}


//------------------------------------------------------------------------------
// String Helpers

// Portable version of stristr()
char* StrIStr(const char* s1, const char* s2)
{
    const char* cp = s1;

    if (!*s2)
        return const_cast<char*>(s1);

    while (*cp) {
        const char* s = cp;
        const char* t = s2;

        while (*s && *t && (std::tolower((uint8_t)*s) == std::tolower((uint8_t)*t)))
            ++s, ++t;

        if (*t == 0)
            return const_cast<char*>(cp);
        ++cp;
    }

    return nullptr;
}


} // namespace core
