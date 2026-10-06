// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>

#include <gtest/gtest.h>

#include "shadnet/blob.h"

namespace {

std::vector<u8> Packet(u32 len, size_t extra) {
    std::vector<u8> v = {static_cast<u8>(len), static_cast<u8>(len >> 8),
                         static_cast<u8>(len >> 16), static_cast<u8>(len >> 24)};
    v.insert(v.end(), extra, 'x');
    return v;
}

std::string Extract(const std::vector<u8>& v, size_t pos) {
    return ShadNet::ExtractBlobBytes(v.data(), v.size(), pos);
}

} // namespace

TEST(ShadNetBlob, ReadsAValidBlob) {
    EXPECT_EQ(Extract(Packet(3, 3), 0), "xxx");
    EXPECT_EQ(Extract(Packet(0, 0), 0), "");
}

TEST(ShadNetBlob, ReadsAtAnOffset) {
    auto v = Packet(2, 2);
    v.insert(v.begin(), {'a', 'b'});
    EXPECT_EQ(Extract(v, 2), "xx");
}

TEST(ShadNetBlob, RejectsALengthLargerThanThePayload) {
    EXPECT_EQ(Extract(Packet(4, 3), 0), "");
}

TEST(ShadNetBlob, RejectsLengthsThatWrapAsSignedInts) {
    // Lengths from 2 GiB up were negative after the old int cast and passed the bounds check.
    EXPECT_EQ(Extract(Packet(0x80000000u, 16), 0), "");
    EXPECT_EQ(Extract(Packet(0xFFFFFFFFu, 16), 0), "");
    EXPECT_EQ(Extract(Packet(0x7FFFFFFFu, 16), 0), "");
}

TEST(ShadNetBlob, RejectsPositionsOutsideThePayload) {
    const auto v = Packet(1, 1);
    EXPECT_EQ(Extract(v, v.size()), "");
    EXPECT_EQ(Extract(v, v.size() + 100), "");
    EXPECT_EQ(Extract(v, v.size() - 3), "");
    EXPECT_EQ(Extract(v, ~size_t{0}), "");
}

TEST(ShadNetBlob, EmptyBufferIsSafe) {
    const std::vector<u8> empty;
    EXPECT_EQ(Extract(empty, 0), "");
}
