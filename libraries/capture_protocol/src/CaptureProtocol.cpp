// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureProtocol.hpp"

namespace protos {


//------------------------------------------------------------------------------
// Tools

std::string SanitizeString(const char* buffer, size_t bytes)
{
    std::string result(bytes + 1, '\0');
    char* data = result.data();
    int used = 0;

    for (size_t i = 0; i < bytes; ++i) {
        char ch = buffer[i];
        if (ch == '\0') {
            break;
        }
        if (ch >= ' ' && ch <= '~') {
            data[used] = ch;
            ++used;
        }
    }
    data[used] = '\0';

    result.resize(used);
    return std::move(result);
}

static inline bool FloatsNotEqual(float a, float b, const float eps = 0.000001f)
{
    return std::fabs(a - b) > eps;
}

bool CameraExtrinsics::operator==(const CameraExtrinsics& rhs) const
{
    if (IsIdentity != rhs.IsIdentity) {
        return false;
    }

    for (int i = 0; i < 16; ++i) {
        if (FloatsNotEqual(Transform[i], rhs.Transform[i])) {
            return false;
        }
    }

    return true;
}


} // namespace protos
