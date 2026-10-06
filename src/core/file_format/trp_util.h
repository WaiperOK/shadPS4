// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include "common/types.h"

namespace TrpUtil {

// Entry names come straight from the file, so they must be a plain file name: no separators,
// no "." / ".." and no drive prefixes, otherwise they could point outside the output directory.
// The name is read with a bounded length because the field is not guaranteed to be NUL
// terminated.
inline std::optional<std::string> GetSafeEntryName(const char* raw, size_t capacity) {
    const size_t len = strnlen(raw, capacity);
    if (len == 0) {
        return std::nullopt;
    }
    std::string name(raw, len);
    if (name == "." || name == ".." || name.find_first_of("/\\:") != std::string::npos) {
        return std::nullopt;
    }
    return name;
}

// Checks that [pos, pos + len) lies inside the file without overflowing.
inline bool IsRangeInFile(u64 pos, u64 len, u64 file_size) {
    return pos <= file_size && len <= file_size - pos;
}

} // namespace TrpUtil
