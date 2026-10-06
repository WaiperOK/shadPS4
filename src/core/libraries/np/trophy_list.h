// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "common/types.h"

// Data model behind the in-game trophy list. It only knows about the trophy XML files, so it can
// be tested without the emulator and used by any front end.
namespace Libraries::Np::TrophyList {

enum class Grade : u8 {
    Platinum,
    Gold,
    Silver,
    Bronze,
    Unknown,
};
constexpr size_t NumGrades = 4; // Platinum, Gold, Silver, Bronze

struct Entry {
    s32 id = -1;
    s32 group_id = -1;
    Grade grade = Grade::Unknown;
    bool hidden = false;
    bool unlocked = false;
    u64 timestamp = 0; // Unix time in seconds, 0 when locked
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

    // Completion in percent, 0 when there are no trophies.
    u32 Percent() const {
        return total == 0 ? 0 : static_cast<u32>(static_cast<u64>(unlocked) * 100 / total);
    }
};

struct Filter {
    bool show_locked = true;
    bool show_unlocked = true;
    std::array<bool, NumGrades> grades{true, true, true, true};
    std::string text; // case insensitive, matched against what the list displays
};

Grade GradeFromChar(char type);
std::string_view GradeName(Grade grade);

// Builds the list from the trophy definition (the TROP XML: titles, names, details, grades) and
// the user's progress file (unlock state and time). The progress is optional and may be empty.
// Returns false when the definition cannot be parsed.
bool Build(std::string_view definition_xml, std::string_view progress_xml, List& out);

// Same as Build, reading the two files. A missing progress file means nothing is unlocked.
bool Load(const std::filesystem::path& definition_xml, const std::filesystem::path& progress_xml,
          List& out);

Summary Summarize(const List& list);

// A hidden trophy that is still locked must not reveal its name or description.
std::string_view DisplayName(const Entry& entry);
std::string_view DisplayDetail(const Entry& entry);

bool Matches(const Entry& entry, const Filter& filter);

// "YYYY-MM-DD HH:MM UTC" for a Unix time in seconds.
std::string FormatTimestampUtc(u64 seconds);

// The trophy files of the running title. libSceNpTrophy sets them when the title registers its
// trophy set; the overlay reads them. Safe to call from any thread.
struct Source {
    std::filesystem::path definition_xml;
    std::filesystem::path progress_xml;
    bool valid() const {
        return !definition_xml.empty();
    }
};
void SetActiveSource(Source source);
Source GetActiveSource();

} // namespace Libraries::Np::TrophyList
