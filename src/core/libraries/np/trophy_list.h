// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "common/types.h"

namespace Libraries::Np::TrophyList {

enum class Grade : u8 {
    Platinum,
    Gold,
    Silver,
    Bronze,
    Unknown,
};
constexpr size_t NumGrades = 4;

struct Entry {
    s32 id = -1;
    s32 group_id = -1;
    Grade grade = Grade::Unknown;
    bool hidden = false;
    bool unlocked = false;
    u64 timestamp = 0;
    std::string name;
    std::string detail;
};

struct List {
    std::string title;
    std::vector<Entry> entries;
};

struct Summary {
    u32 total = 0;
    u32 unlocked = 0;
    std::array<u32, NumGrades> total_by_grade{};
    std::array<u32, NumGrades> unlocked_by_grade{};

    u32 Percent() const {
        return total == 0 ? 0 : static_cast<u32>(static_cast<u64>(unlocked) * 100 / total);
    }
};

struct Filter {
    bool show_locked = true;
    bool show_unlocked = true;
    std::array<bool, NumGrades> grades{true, true, true, true};
    std::string text;
};

Grade GradeFromChar(char type);
std::string_view GradeName(Grade grade);

bool Build(std::string_view definition_xml, std::string_view progress_xml, List& out);

bool Load(const std::filesystem::path& definition_xml, const std::filesystem::path& progress_xml,
          List& out);

Summary Summarize(const List& list);

std::string_view DisplayName(const Entry& entry);
std::string_view DisplayDetail(const Entry& entry);

bool Matches(const Entry& entry, const Filter& filter);

std::string FormatTimestampUtc(u64 seconds);

struct Source {
    std::filesystem::path definition_xml;
    std::filesystem::path progress_xml;
    bool valid() const {
        return !definition_xml.empty();
    }
};

struct TitleProgress {
    std::string id;
    std::string title;
    Summary summary;
    u64 last_earned = 0;
    Source source;
};

std::vector<TitleProgress> ScanTitles(const std::filesystem::path& trophy_root,
                                      const std::filesystem::path& progress_dir);

void SetActiveSource(Source source);
Source GetActiveSource();

} // namespace Libraries::Np::TrophyList
