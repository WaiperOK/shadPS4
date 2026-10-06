// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <gtest/gtest.h>

#include "shader_recompiler/frontend/code_slice.h"

using Shader::Gcn::GcnCodeSlice;

TEST(GcnCodeSlice, ReadsInOrder) {
    const std::array<u32, 4> code = {1, 2, 3, 4};
    GcnCodeSlice slice(code.data(), code.data() + code.size());
    EXPECT_EQ(slice.at(0), 1u);
    EXPECT_EQ(slice.at(3), 4u);
    EXPECT_EQ(slice.readu32(), 1u);
    EXPECT_EQ(slice.at(0), 2u);
    EXPECT_FALSE(slice.atEnd());
    EXPECT_EQ(slice.readu64(), (u64{3} << 32) | 2u);
    EXPECT_FALSE(slice.atEnd());
    EXPECT_EQ(slice.readu32(), 4u);
    EXPECT_TRUE(slice.atEnd());
}

TEST(GcnCodeSlice, AtPastTheEndReturnsZero) {
    const std::array<u32, 2> code = {7, 8};
    GcnCodeSlice slice(code.data(), code.data() + code.size());
    EXPECT_EQ(slice.at(1), 8u);
    EXPECT_EQ(slice.at(2), 0u);
    EXPECT_EQ(slice.at(0xFFFFFFFFu), 0u);
}

TEST(GcnCodeSlice, ReadU64OnTheLastDwordStopsAtTheEnd) {
    // A two dword instruction starting on the last dword used to step past the end, after which
    // atEnd() (an equality test) never became true and decoding ran off the buffer.
    const std::array<u32, 3> code = {1, 2, 3};
    GcnCodeSlice slice(code.data(), code.data() + code.size());
    slice.readu32();
    slice.readu32();
    EXPECT_FALSE(slice.atEnd());
    EXPECT_EQ(slice.readu64(), 0u);
    EXPECT_TRUE(slice.atEnd());
    EXPECT_EQ(slice.readu32(), 0u);
    EXPECT_TRUE(slice.atEnd());
}

TEST(GcnCodeSlice, EmptySliceIsAtEnd) {
    const std::array<u32, 1> code = {0};
    GcnCodeSlice slice(code.data(), code.data());
    EXPECT_TRUE(slice.atEnd());
    EXPECT_EQ(slice.readu32(), 0u);
    EXPECT_EQ(slice.readu64(), 0u);
    EXPECT_EQ(slice.at(0), 0u);
}
