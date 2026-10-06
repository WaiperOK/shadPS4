// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstring>
#include "common/types.h"

namespace Shader::Gcn {

// A read cursor over guest shader code. The length of the code comes from the shader binary
// header, which the guest controls, and a multi dword instruction (or literal constant) can start
// on the last dword. Reads therefore never go past the end: they return zero and park the cursor
// at the end instead of walking into unrelated memory.
class GcnCodeSlice {
public:
    GcnCodeSlice(const u32* ptr, const u32* end) : m_ptr(ptr), m_end(end) {}
    GcnCodeSlice(const GcnCodeSlice& other) = default;
    ~GcnCodeSlice() = default;

    u32 at(u32 id) const {
        return id < Remaining() ? m_ptr[id] : 0;
    }

    u32 readu32() {
        if (Remaining() < 1) {
            return 0;
        }
        return *(m_ptr++);
    }

    u64 readu64() {
        if (Remaining() < 2) {
            m_ptr = m_end;
            return 0;
        }
        u64 value;
        std::memcpy(&value, m_ptr, sizeof(value));
        m_ptr += 2;
        return value;
    }

    // `>=` rather than `==`: the cursor must stop even if it ever ended up past the end.
    bool atEnd() const {
        return m_ptr >= m_end;
    }

private:
    size_t Remaining() const {
        return m_ptr < m_end ? static_cast<size_t>(m_end - m_ptr) : 0;
    }

    const u32* m_ptr{};
    const u32* m_end{};
};

} // namespace Shader::Gcn
