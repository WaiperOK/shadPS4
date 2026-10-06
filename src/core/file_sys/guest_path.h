// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Core::FileSys {

// Normalizes a guest path (repeated slashes, "." and ".." components). Returns nullopt when the
// path is too long, contains characters that are unsafe on the host, or escapes the root.
std::optional<std::string> SanitizeGuestPath(std::string_view path);

} // namespace Core::FileSys
