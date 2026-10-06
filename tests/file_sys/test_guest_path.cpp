// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <gtest/gtest.h>

#include "core/file_sys/guest_path.h"

using Core::FileSys::SanitizeGuestPath;

TEST(GuestPath, KeepsPlainPaths) {
    EXPECT_EQ(SanitizeGuestPath("/app0/game.kpf"), "/app0/game.kpf");
    EXPECT_EQ(SanitizeGuestPath("/app0"), "/app0");
    EXPECT_EQ(SanitizeGuestPath("/"), "/");
    EXPECT_EQ(SanitizeGuestPath(""), "");
}

TEST(GuestPath, KeepsTrailingSlash) {
    EXPECT_EQ(SanitizeGuestPath("/app0/dir/"), "/app0/dir/");
}

TEST(GuestPath, CollapsesRepeatedSlashes) {
    EXPECT_EQ(SanitizeGuestPath("/app0//game.kpf"), "/app0/game.kpf");
    EXPECT_EQ(SanitizeGuestPath("//app0///a//b"), "/app0/a/b");
}

TEST(GuestPath, ResolvesDotComponents) {
    EXPECT_EQ(SanitizeGuestPath("/app0/./a/./b"), "/app0/a/b");
    EXPECT_EQ(SanitizeGuestPath("/app0/dir/../file"), "/app0/file");
    EXPECT_EQ(SanitizeGuestPath("/app0/../savedata0/x"), "/savedata0/x");
}

TEST(GuestPath, RejectsPathsEscapingTheRoot) {
    EXPECT_FALSE(SanitizeGuestPath("/..").has_value());
    EXPECT_FALSE(SanitizeGuestPath("/../etc/passwd").has_value());
    EXPECT_FALSE(SanitizeGuestPath("../x").has_value());
    EXPECT_FALSE(SanitizeGuestPath("/app0/../../etc/passwd").has_value());
    EXPECT_FALSE(SanitizeGuestPath("/app0/a/../../../etc/passwd").has_value());
}

TEST(GuestPath, KeepsRelativePathsRelative) {
    EXPECT_EQ(SanitizeGuestPath("app0/x"), "app0/x");
}

TEST(GuestPath, RejectsEmbeddedNul) {
    EXPECT_FALSE(SanitizeGuestPath(std::string("/app0/a\0b", 9)).has_value());
}

TEST(GuestPath, RejectsTooLongPaths) {
    EXPECT_TRUE(SanitizeGuestPath("/" + std::string(254, 'a')).has_value());
    EXPECT_FALSE(SanitizeGuestPath("/" + std::string(255, 'a')).has_value());
}
