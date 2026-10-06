// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <mutex>
#include <unordered_map>

#include <pugixml.hpp>

#include "core/libraries/np/trophy_list.h"

namespace Libraries::Np::TrophyList {

namespace {

constexpr std::string_view HiddenName = "Hidden trophy";
constexpr std::string_view HiddenDetail = "The details are revealed when the trophy is earned.";

struct Progress {
    bool unlocked = false;
    u64 timestamp = 0;
};

size_t GradeIndex(Grade grade) {
    return static_cast<size_t>(grade);
}

bool ContainsIgnoreCase(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                [](char a, char b) {
                                    return std::tolower(static_cast<unsigned char>(a)) ==
                                           std::tolower(static_cast<unsigned char>(b));
                                });
    return it != haystack.end();
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::mutex g_source_mutex;
Source g_source;

} // namespace

Grade GradeFromChar(char type) {
    switch (std::toupper(static_cast<unsigned char>(type))) {
    case 'P':
        return Grade::Platinum;
    case 'G':
        return Grade::Gold;
    case 'S':
        return Grade::Silver;
    case 'B':
        return Grade::Bronze;
    default:
        return Grade::Unknown;
    }
}

std::string_view GradeName(Grade grade) {
    switch (grade) {
    case Grade::Platinum:
        return "Platinum";
    case Grade::Gold:
        return "Gold";
    case Grade::Silver:
        return "Silver";
    case Grade::Bronze:
        return "Bronze";
    default:
        return "Unknown";
    }
}

bool Build(std::string_view definition_xml, std::string_view progress_xml, List& out) {
    out = {};

    pugi::xml_document definition;
    if (!definition.load_buffer(definition_xml.data(), definition_xml.size())) {
        return false;
    }
    const pugi::xml_node conf = definition.child("trophyconf");
    if (!conf) {
        return false;
    }

    std::unordered_map<s32, Progress> progress;
    if (!progress_xml.empty()) {
        pugi::xml_document progress_doc;
        if (progress_doc.load_buffer(progress_xml.data(), progress_xml.size())) {
            for (const pugi::xml_node& node : progress_doc.child("trophyconf").children("trophy")) {
                const s32 id = node.attribute("id").as_int(-1);
                if (id < 0) {
                    continue;
                }
                progress[id] = Progress{
                    .unlocked = node.attribute("unlockstate").as_bool(),
                    .timestamp = node.attribute("timestamp").as_ullong(),
                };
            }
        }
    }

    out.title = conf.child("title-name").text().as_string();
    for (const pugi::xml_node& node : conf.children("trophy")) {
        Entry entry;
        entry.id = node.attribute("id").as_int(-1);
        if (entry.id < 0) {
            continue;
        }
        entry.group_id = node.attribute("gid").as_int(-1);
        const std::string_view type = node.attribute("ttype").value();
        entry.grade = type.empty() ? Grade::Unknown : GradeFromChar(type.front());
        entry.hidden = node.attribute("hidden").as_bool();
        entry.name = node.child("name").text().as_string();
        entry.detail = node.child("detail").text().as_string();

        entry.unlocked = node.attribute("unlockstate").as_bool();
        entry.timestamp = node.attribute("timestamp").as_ullong();
        if (const auto it = progress.find(entry.id); it != progress.end()) {
            entry.unlocked = it->second.unlocked;
            entry.timestamp = it->second.timestamp;
        }
        if (!entry.unlocked) {
            entry.timestamp = 0;
        }
        out.entries.push_back(std::move(entry));
    }

    std::stable_sort(out.entries.begin(), out.entries.end(),
                     [](const Entry& a, const Entry& b) { return a.id < b.id; });
    return true;
}

bool Load(const std::filesystem::path& definition_xml, const std::filesystem::path& progress_xml,
          List& out) {
    const std::string definition = ReadFile(definition_xml);
    if (definition.empty()) {
        out = {};
        return false;
    }
    const std::string progress = progress_xml.empty() ? std::string{} : ReadFile(progress_xml);
    return Build(definition, progress, out);
}

Summary Summarize(const List& list) {
    Summary summary;
    for (const Entry& entry : list.entries) {
        ++summary.total;
        if (entry.unlocked) {
            ++summary.unlocked;
        }
        if (entry.grade == Grade::Unknown) {
            continue;
        }
        ++summary.total_by_grade[GradeIndex(entry.grade)];
        if (entry.unlocked) {
            ++summary.unlocked_by_grade[GradeIndex(entry.grade)];
        }
    }
    return summary;
}

std::string_view DisplayName(const Entry& entry) {
    return entry.hidden && !entry.unlocked ? HiddenName : std::string_view{entry.name};
}

std::string_view DisplayDetail(const Entry& entry) {
    return entry.hidden && !entry.unlocked ? HiddenDetail : std::string_view{entry.detail};
}

bool Matches(const Entry& entry, const Filter& filter) {
    if (entry.unlocked ? !filter.show_unlocked : !filter.show_locked) {
        return false;
    }
    if (entry.grade != Grade::Unknown && !filter.grades[GradeIndex(entry.grade)]) {
        return false;
    }
    return ContainsIgnoreCase(DisplayName(entry), filter.text) ||
           ContainsIgnoreCase(DisplayDetail(entry), filter.text);
}

std::string FormatTimestampUtc(u64 seconds) {
    const s64 days = static_cast<s64>(seconds / 86400);
    const u32 secs_of_day = static_cast<u32>(seconds % 86400);

    const s64 z = days + 719468;
    const s64 era = (z >= 0 ? z : z - 146096) / 146097;
    const u32 doe = static_cast<u32>(z - era * 146097);
    const u32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const s64 year_base = static_cast<s64>(yoe) + era * 400;
    const u32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const u32 mp = (5 * doy + 2) / 153;
    const u32 day = doy - (153 * mp + 2) / 5 + 1;
    const u32 month = mp < 10 ? mp + 3 : mp - 9;
    const s64 year = year_base + (month <= 2 ? 1 : 0);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04lld-%02u-%02u %02u:%02u UTC",
                  static_cast<long long>(year), month, day, secs_of_day / 3600,
                  (secs_of_day % 3600) / 60);
    return buffer;
}

std::vector<TitleProgress> ScanTitles(const std::filesystem::path& trophy_root,
                                      const std::filesystem::path& progress_dir) {
    std::vector<TitleProgress> titles;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(trophy_root, ec), end; !ec && it != end;
         it.increment(ec)) {
        std::error_code entry_ec;
        if (!it->is_directory(entry_ec) || entry_ec) {
            continue;
        }
        TitleProgress title;
        title.id = it->path().filename().string();
        title.source.definition_xml = it->path() / "Xml" / "TROP.XML";
        title.source.progress_xml = progress_dir / (title.id + ".xml");

        List list;
        if (!Load(title.source.definition_xml, title.source.progress_xml, list)) {
            continue;
        }
        title.title = list.title.empty() ? title.id : list.title;
        title.summary = Summarize(list);
        for (const Entry& entry : list.entries) {
            if (entry.unlocked) {
                title.last_earned = std::max(title.last_earned, entry.timestamp);
            }
        }
        titles.push_back(std::move(title));
    }

    std::sort(titles.begin(), titles.end(), [](const TitleProgress& a, const TitleProgress& b) {
        if (a.last_earned != b.last_earned) {
            return a.last_earned > b.last_earned;
        }
        return a.title < b.title;
    });
    return titles;
}

void SetActiveSource(Source source) {
    std::lock_guard lock{g_source_mutex};
    g_source = std::move(source);
}

Source GetActiveSource() {
    std::lock_guard lock{g_source_mutex};
    return g_source;
}

} // namespace Libraries::Np::TrophyList
