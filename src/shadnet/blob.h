// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string>
#include "common/types.h"

namespace ShadNet {

// Reads a u32 little endian length prefixed blob starting at `pos` in [data, data + size).
// Returns an empty string when the prefix or the blob does not fit.
// The length comes from the server, so everything is computed on size_t: casting a u32 length to
// int made values from 2 GiB up negative, which passed the old bounds check and read past the end
// of the buffer.
inline std::string ExtractBlobBytes(const u8* data, size_t size, size_t pos) {
    if (pos > size || size - pos < 4) {
        return {};
    }
    const u32 len = static_cast<u32>(data[pos]) | (static_cast<u32>(data[pos + 1]) << 8) |
                    (static_cast<u32>(data[pos + 2]) << 16) |
                    (static_cast<u32>(data[pos + 3]) << 24);
    const size_t data_start = pos + 4;
    if (len > size - data_start) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(data + data_start), len);
}

} // namespace ShadNet
