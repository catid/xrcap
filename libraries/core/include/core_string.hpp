// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "core.hpp"
#include "core_bit_math.hpp" // NonzeroLowestBitIndex

#include <string.h>
#include <string>
#include <sstream> // android to_string

namespace core {


//------------------------------------------------------------------------------
// String Conversion

/// Convert buffer to hex string
std::string HexDump(const uint8_t* data, int bytes);

/// Convert value to hex string
std::string HexString(uint64_t value);


//------------------------------------------------------------------------------
// Conversion to Base64

/// Get number of characters required to represent the given number of bytes.
/// Input of 0 will return 0.
int GetBase64LengthFromByteCount(int bytes);

/// Returns number of bytes (ASCII characters) written, or 0 for error.
/// Note that to disambiguate high zeros with padding bits, we use A to
/// represent high zero bits and = to pad out the output to a multiple of
/// 4 bytes.
int WriteBase64(
    const void* buffer,
    int bytes,
    char* encoded_buffer,
    int encoded_bytes);

/// This version writes a null-terminator.
int WriteBase64Str(
    const void* buffer,
    int bytes,
    char* encoded_buffer,
    int encoded_bytes);


//------------------------------------------------------------------------------
// Conversion from Base64

/// Get number of original bytes represented by the encoded buffer.
/// This will be the number of data bytes written by ReadBase64().
int GetByteCountFromBase64(
    const char* encoded_buffer,
    int bytes);

/// Returns number of bytes written to decoded buffer.
/// Returns 0 on error.
/// Precondition: `decoded_buffer` contains enough bytes to represent the
/// original data.
/// At least GetByteCountFromBase64(encoded_buffer, encoded_bytes) bytes.
int ReadBase64(
    const char* encoded_buffer,
    int encoded_bytes,
    void* decoded_buffer);


//------------------------------------------------------------------------------
// Copy Strings

CORE_INLINE void SafeCopyCStr(char* dest, size_t destBytes, const char* src)
{
#if defined(_MSC_VER)
    ::strncpy_s(dest, destBytes, src, _TRUNCATE);
#else // _MSC_VER
    ::strncpy(dest, src, destBytes);
#endif // _MSC_VER
    // No null-character is implicitly appended at the end of destination
    dest[destBytes - 1] = '\0';
}


//------------------------------------------------------------------------------
// Compare Strings

// Portable version of stristr()
char* StrIStr(const char* s1, const char* s2);

/// Case-insensitive string comparison
/// Returns < 0 if a < b (lexographically ignoring case)
/// Returns 0 if a == b (case-insensitive)
/// Returns > 0 if a > b (lexographically ignoring case)
CORE_INLINE int StrCaseCompare(const char* a, const char* b)
{
#if defined(_WIN32)
# if defined(_MSC_VER) && (_MSC_VER >= 1400)
    return ::_stricmp(a, b);
# else
    return ::stricmp(a, b);
# endif
#else
    return ::strcasecmp(a, b);
#endif
}

// Portable version of strnicmp()
CORE_INLINE int StrNCaseCompare(const char* a, const char* b, size_t count)
{
#if defined(_MSC_VER)
#if (_MSC_VER >= 1400)
    return ::_strnicmp(a, b, count);
#else
    return ::strnicmp(a, b, count);
#endif
#else
    return strncasecmp(a, b, count);
#endif
}


} // namespace core


namespace std {


//------------------------------------------------------------------------------
// Android Portability

#if defined(ANDROID) && !defined(DEFINED_TO_STRING)
# define DEFINED_TO_STRING

/// Polyfill for to_string() on Android
template<typename T> string to_string(T value)
{
    ostringstream os;
    os << value;
    return os.str();
}

#endif // DEFINED_TO_STRING


} // namespace std
