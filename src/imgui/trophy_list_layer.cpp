// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "core/libraries/np/trophy_list.h"
#include "imgui/imgui_layer.h"
#include "imgui/trophy_list_layer.h"

namespace ImGui::Trophies {

namespace TL = Libraries::Np::TrophyList;

namespace {

bool g_open = false;

struct State {
    TL::Source source;
    TL::List list;
    TL::Filter filter;
    std::array<char, 64> search{};
    bool loaded = false;
    std::filesystem::file_time_type progress_time{};
    double last_check = 0.0;
};
State g_state;

ImVec4 GradeColor(TL::Grade grade) {
    switch (grade) {
    case TL::Grade::Platinum:
        return ImVec4(0.65f, 0.80f, 1.00f, 1.0f);
    case TL::Grade::Gold:
        return ImVec4(1.00f, 0.84f, 0.00f, 1.0f);
    case TL::Grade::Silver:
        return ImVec4(0.78f, 0.78f, 0.82f, 1.0f);
    case TL::Grade::Bronze:
        return ImVec4(0.80f, 0.50f, 0.20f, 1.0f);
    default:
        return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    }
}

void TextView(std::string_view text) {
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

std::filesystem::file_time_type ProgressTime(const TL::Source& source) {
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(source.progress_xml, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

void Reload() {
    g_state.loaded =
        TL::Load(g_state.source.definition_xml, g_state.source.progress_xml, g_state.list);
    g_state.progress_time = ProgressTime(g_state.source);
}

// Reloads when the title registered another trophy set, when the progress file changed (a trophy
// was unlocked while the window is open) or when asked to.
void Refresh(bool force) {
    const TL::Source source = TL::GetActiveSource();
    const bool changed = source.definition_xml != g_state.source.definition_xml ||
                         source.progress_xml != g_state.source.progress_xml;
    g_state.source = source;
    if (!source.valid()) {
        g_state.loaded = false;
        g_state.list = {};
        return;
    }

    const double now = ImGui::GetTime();
    const bool due = now - g_state.last_check >= 1.0;
    if (changed || force || !g_state.loaded) {
        g_state.last_check = now;
        Reload();
    } else if (due) {
        g_state.last_check = now;
        if (ProgressTime(source) != g_state.progress_time) {
            Reload();
        }
    }
}

void DrawSummary(const TL::Summary& summary) {
    char overlay[64];
    std::snprintf(overlay, sizeof(overlay), "%u / %u  (%u%%)", summary.unlocked, summary.total,
                  summary.Percent());
    ImGui::ProgressBar(static_cast<float>(summary.Percent()) / 100.0f, ImVec2(-1.0f, 0.0f),
                       overlay);

    constexpr std::array<TL::Grade, TL::NumGrades> grades = {TL::Grade::Platinum, TL::Grade::Gold,
                                                             TL::Grade::Silver, TL::Grade::Bronze};
    for (size_t i = 0; i < grades.size(); ++i) {
        if (i != 0) {
            ImGui::SameLine();
        }
        ImGui::TextColored(GradeColor(grades[i]), "%s %u/%u", TL::GradeName(grades[i]).data(),
                           summary.unlocked_by_grade[i], summary.total_by_grade[i]);
    }
}

void DrawFilters() {
    TL::Filter& filter = g_state.filter;
    ImGui::Checkbox("Earned", &filter.show_unlocked);
    ImGui::SameLine();
    ImGui::Checkbox("Not earned", &filter.show_locked);
    for (size_t i = 0; i < TL::NumGrades; ++i) {
        const auto grade = static_cast<TL::Grade>(i);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, GradeColor(grade));
        ImGui::Checkbox(TL::GradeName(grade).data(), &filter.grades[i]);
        ImGui::PopStyleColor();
    }

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputTextWithHint("##trophy_search", "Search", g_state.search.data(),
                                 g_state.search.size())) {
        filter.text = g_state.search.data();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        Refresh(true);
    }
}

void DrawTable() {
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV;
    if (!ImGui::BeginTable("##trophy_table", 3, flags, ImVec2(0.0f, 0.0f))) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Grade", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Trophy", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Earned", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableHeadersRow();

    for (const TL::Entry& entry : g_state.list.entries) {
        if (!TL::Matches(entry, g_state.filter)) {
            continue;
        }
        ImGui::TableNextRow();
        ImGui::PushID(entry.id);

        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(GradeColor(entry.grade), "%s", TL::GradeName(entry.grade).data());

        ImGui::TableSetColumnIndex(1);
        if (entry.unlocked) {
            TextView(TL::DisplayName(entry));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            TextView(TL::DisplayName(entry));
            ImGui::PopStyleColor();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushTextWrapPos(0.0f);
        TextView(TL::DisplayDetail(entry));
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(2);
        if (entry.unlocked) {
            TextView(entry.timestamp != 0 ? TL::FormatTimestampUtc(entry.timestamp) : "Earned");
        } else {
            ImGui::TextDisabled("Not earned");
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

class TrophyListLayer final : public ImGui::Layer {
public:
    void Draw() override;
    bool ShouldKeepDrawing() override {
        return g_open;
    }
};
TrophyListLayer g_layer;

void TrophyListLayer::Draw() {
    if (!g_open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Trophies###shadps4_trophies", &g_open)) {
        ImGui::End();
        return;
    }

    Refresh(false);
    if (!g_state.source.valid()) {
        ImGui::TextWrapped("No trophy set is registered yet. The list is available once the game "
                           "has initialised its trophy support.");
    } else if (!g_state.loaded) {
        ImGui::TextWrapped("The trophy data could not be read:\n%s",
                           g_state.source.definition_xml.string().c_str());
    } else {
        if (!g_state.list.title.empty()) {
            TextView(g_state.list.title);
        }
        DrawSummary(TL::Summarize(g_state.list));
        ImGui::Separator();
        DrawFilters();
        ImGui::Separator();
        DrawTable();
    }
    ImGui::End();
}

} // namespace

void Register() {
    ImGui::Layer::AddLayer(&g_layer);
}
void Unregister() {
    ImGui::Layer::RemoveLayer(&g_layer);
}
void Toggle() {
    g_open = !g_open;
}
void Open() {
    g_open = true;
}
bool IsOpen() {
    return g_open;
}

} // namespace ImGui::Trophies
