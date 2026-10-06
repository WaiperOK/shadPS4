// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>
#include "core/file_sys/guest_path.h"

namespace Core::FileSys {

// Normalize a guest path: collapse repeated slashes (evil games like Turok2 pass /app0//game.kpf)
// and resolve "." / ".." components lexically. Paths that try to climb above the root are
// rejected, so a guest can never address anything outside of the mount it is using.
std::optional<std::string> SanitizeGuestPath(std::string_view path) {
    if (path.length() > 255) {
        return std::nullopt;
    }
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string_view::npos) {
            end = path.size();
        }
        const std::string_view part = path.substr(start, end - start);
        start = end + 1;
        if (part.empty() || part == ".") {
            continue;
        }
        if (part == "..") {
            if (parts.empty()) {
                return std::nullopt;
            }
            parts.pop_back();
            continue;
        }
        // NUL truncates host paths, and on Windows backslashes and drive letters are treated
        // as path separators / roots by std::filesystem, which would bypass the checks above.
        if (part.find('\0') != std::string_view::npos) {
            return std::nullopt;
        }
#ifdef _WIN32
        if (part.find_first_of("\\:") != std::string_view::npos) {
            return std::nullopt;
        }
#endif
        parts.push_back(part);
    }

    std::string corrected;
    corrected.reserve(path.size());
    const bool absolute = path.starts_with('/');
    for (const auto part : parts) {
        if (absolute || !corrected.empty()) {
            corrected += '/';
        }
        corrected += part;
    }
    if (corrected.empty() && absolute) {
        corrected = "/";
    }
    if (!parts.empty() && path.ends_with('/')) {
        corrected += '/';
    }
    return corrected;
}

} // namespace Core::FileSys
