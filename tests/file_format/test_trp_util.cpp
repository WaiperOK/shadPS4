// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include <gtest/gtest.h>

#include "core/file_format/trp_util.h"

namespace {

std::optional<std::string> Name(const char (&raw)[32]) {
    return TrpUtil::GetSafeEntryName(raw, sizeof(raw));
}

struct Field {
    char data[32]{};
    explicit Field(const char* text, size_t len) {
        std::memcpy(data, text, len);
    }
};

} // namespace

TEST(TrpUtil, AcceptsPlainNames) {
    EXPECT_EQ(Name(Field("ICON0.PNG", 9).data), "ICON0.PNG");
    EXPECT_EQ(Name(Field("TROP_00.ESFM", 12).data), "TROP_00.ESFM");
}

TEST(TrpUtil, RejectsPathsAndSpecialNames) {
    EXPECT_FALSE(Name(Field("../../x", 7).data).has_value());
    EXPECT_FALSE(Name(Field("a/b", 3).data).has_value());
    EXPECT_FALSE(Name(Field("/etc/x", 6).data).has_value());
    EXPECT_FALSE(Name(Field("..", 2).data).has_value());
    EXPECT_FALSE(Name(Field(".", 1).data).has_value());
    EXPECT_FALSE(Name(Field("a\\b", 3).data).has_value());
    EXPECT_FALSE(Name(Field("C:x", 3).data).has_value());
}

TEST(TrpUtil, RejectsEmptyName) {
    EXPECT_FALSE(Name(Field("", 0).data).has_value());
}

TEST(TrpUtil, ReadsAnUnterminatedFieldWithinItsCapacity) {
    char raw[32];
    std::memset(raw, 'A', sizeof(raw));
    const auto name = TrpUtil::GetSafeEntryName(raw, sizeof(raw));
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(name->size(), sizeof(raw));
}

TEST(TrpUtil, RangeCheckHandlesOverflow) {
    EXPECT_TRUE(TrpUtil::IsRangeInFile(0, 10, 10));
    EXPECT_TRUE(TrpUtil::IsRangeInFile(10, 0, 10));
    EXPECT_FALSE(TrpUtil::IsRangeInFile(1, 10, 10));
    EXPECT_FALSE(TrpUtil::IsRangeInFile(11, 0, 10));
    EXPECT_FALSE(TrpUtil::IsRangeInFile(~0ull, 2, 10));
    EXPECT_FALSE(TrpUtil::IsRangeInFile(2, ~0ull, 10));
}
