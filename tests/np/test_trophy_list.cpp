// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "core/libraries/np/trophy_list.h"

using namespace Libraries::Np::TrophyList;

namespace {

// Shape of a real TROP.XML (names and details) ...
constexpr std::string_view Definition = R"(<?xml version="1.0" encoding="utf-8"?>
<trophyconf version="1.1">
  <title-name>Test Game</title-name>
  <title-detail>Trophies for tests</title-detail>
  <group id="000"><name>Base</name><detail>Base game</detail></group>
  <trophy id="000" hidden="no" ttype="B" pid="-1" gid="000">
    <name>First Steps</name><detail>Take the first step.</detail>
  </trophy>
  <trophy id="001" hidden="yes" ttype="S" pid="-1" gid="000">
    <name>Secret Room</name><detail>Find the hidden room.</detail>
  </trophy>
  <trophy id="002" hidden="no" ttype="G" pid="-1" gid="000">
    <name>Veteran</name><detail>Finish the game.</detail>
  </trophy>
  <trophy id="003" hidden="no" ttype="P" pid="-1" gid="000">
    <name>All Done</name><detail>Earn every trophy.</detail>
  </trophy>
</trophyconf>)";

// ... and of the per-user progress file, which is a copy with the unlock state added.
constexpr std::string_view Progress = R"(<?xml version="1.0" encoding="utf-8"?>
<trophyconf version="1.1">
  <trophy id="000" hidden="no" ttype="B" unlockstate="true" timestamp="1700000000"/>
  <trophy id="001" hidden="yes" ttype="S" unlockstate="false"/>
  <trophy id="002" hidden="no" ttype="G" unlockstate="true" timestamp="1700003600"/>
</trophyconf>)";

List MakeList() {
    List list;
    EXPECT_TRUE(Build(Definition, Progress, list));
    return list;
}

} // namespace

TEST(TrophyList, BuildsEntriesFromDefinitionAndProgress) {
    const List list = MakeList();
    EXPECT_EQ(list.title, "Test Game");
    ASSERT_EQ(list.entries.size(), 4u);

    EXPECT_EQ(list.entries[0].id, 0);
    EXPECT_EQ(list.entries[0].name, "First Steps");
    EXPECT_EQ(list.entries[0].grade, Grade::Bronze);
    EXPECT_TRUE(list.entries[0].unlocked);
    EXPECT_EQ(list.entries[0].timestamp, 1700000000u);

    EXPECT_EQ(list.entries[1].grade, Grade::Silver);
    EXPECT_TRUE(list.entries[1].hidden);
    EXPECT_FALSE(list.entries[1].unlocked);
    EXPECT_EQ(list.entries[1].timestamp, 0u);

    EXPECT_EQ(list.entries[3].grade, Grade::Platinum);
    EXPECT_FALSE(list.entries[3].unlocked); // not in the progress file
}

TEST(TrophyList, WorksWithoutAProgressFile) {
    List list;
    ASSERT_TRUE(Build(Definition, {}, list));
    ASSERT_EQ(list.entries.size(), 4u);
    for (const Entry& entry : list.entries) {
        EXPECT_FALSE(entry.unlocked);
    }
}

TEST(TrophyList, RejectsInvalidDefinitions) {
    List list;
    EXPECT_FALSE(Build("", {}, list));
    EXPECT_FALSE(Build("not xml at all <<<", {}, list));
    EXPECT_FALSE(Build("<other/>", {}, list));
    EXPECT_TRUE(list.entries.empty());
}

TEST(TrophyList, IgnoresBrokenProgressAndBadEntries) {
    List list;
    ASSERT_TRUE(Build(Definition, "<<< broken", list));
    EXPECT_EQ(list.entries.size(), 4u);

    constexpr std::string_view odd = R"(<trophyconf>
      <trophy ttype="G"><name>No id</name></trophy>
      <trophy id="5" ttype="?"><name>Odd grade</name></trophy>
      <trophy id="2"><name>Out of order</name></trophy>
    </trophyconf>)";
    ASSERT_TRUE(Build(odd, {}, list));
    ASSERT_EQ(list.entries.size(), 2u); // the entry without an id is dropped
    EXPECT_EQ(list.entries[0].id, 2);   // sorted by id
    EXPECT_EQ(list.entries[0].grade, Grade::Unknown);
    EXPECT_EQ(list.entries[1].id, 5);
}

TEST(TrophyList, SummarizesProgress) {
    const Summary summary = Summarize(MakeList());
    EXPECT_EQ(summary.total, 4u);
    EXPECT_EQ(summary.unlocked, 2u);
    EXPECT_EQ(summary.Percent(), 50u);
    EXPECT_EQ(summary.total_by_grade[static_cast<size_t>(Grade::Bronze)], 1u);
    EXPECT_EQ(summary.unlocked_by_grade[static_cast<size_t>(Grade::Bronze)], 1u);
    EXPECT_EQ(summary.unlocked_by_grade[static_cast<size_t>(Grade::Silver)], 0u);
    EXPECT_EQ(summary.unlocked_by_grade[static_cast<size_t>(Grade::Gold)], 1u);
    EXPECT_EQ(Summary{}.Percent(), 0u);
}

TEST(TrophyList, HiddenLockedTrophiesDoNotRevealThemselves) {
    const List list = MakeList();
    const Entry& secret = list.entries[1];
    EXPECT_NE(DisplayName(secret), "Secret Room");
    EXPECT_NE(DisplayDetail(secret), "Find the hidden room.");

    Entry earned = secret;
    earned.unlocked = true;
    EXPECT_EQ(DisplayName(earned), "Secret Room");
    EXPECT_EQ(DisplayDetail(earned), "Find the hidden room.");
}

TEST(TrophyList, FilterByStateAndGrade) {
    const List list = MakeList();
    Filter filter;
    filter.show_unlocked = false;
    EXPECT_FALSE(Matches(list.entries[0], filter));
    EXPECT_TRUE(Matches(list.entries[1], filter));

    filter = {};
    filter.grades[static_cast<size_t>(Grade::Gold)] = false;
    EXPECT_TRUE(Matches(list.entries[0], filter));
    EXPECT_FALSE(Matches(list.entries[2], filter));
}

TEST(TrophyList, TextFilterIsCaseInsensitiveAndRespectsHiddenTrophies) {
    const List list = MakeList();
    Filter filter;
    filter.text = "vETER";
    EXPECT_TRUE(Matches(list.entries[2], filter));
    EXPECT_FALSE(Matches(list.entries[0], filter));

    // The real name of a locked hidden trophy must not be findable.
    filter.text = "secret room";
    EXPECT_FALSE(Matches(list.entries[1], filter));
    filter.text = "hidden";
    EXPECT_TRUE(Matches(list.entries[1], filter));
}

TEST(TrophyList, GradeFromCharAndName) {
    EXPECT_EQ(GradeFromChar('P'), Grade::Platinum);
    EXPECT_EQ(GradeFromChar('g'), Grade::Gold);
    EXPECT_EQ(GradeFromChar('S'), Grade::Silver);
    EXPECT_EQ(GradeFromChar('B'), Grade::Bronze);
    EXPECT_EQ(GradeFromChar('x'), Grade::Unknown);
    EXPECT_EQ(GradeName(Grade::Gold), "Gold");
}

TEST(TrophyList, FormatsUtcTimestamps) {
    EXPECT_EQ(FormatTimestampUtc(0), "1970-01-01 00:00 UTC");
    EXPECT_EQ(FormatTimestampUtc(1700000000), "2023-11-14 22:13 UTC");
    EXPECT_EQ(FormatTimestampUtc(1709164800), "2024-02-29 00:00 UTC"); // leap day
    EXPECT_EQ(FormatTimestampUtc(951782400), "2000-02-29 00:00 UTC");
    EXPECT_EQ(FormatTimestampUtc(4102444800), "2100-01-01 00:00 UTC");
}

TEST(TrophyList, ActiveSourceRoundTrips) {
    SetActiveSource({});
    EXPECT_FALSE(GetActiveSource().valid());
    SetActiveSource({"defs.xml", "progress.xml"});
    const Source source = GetActiveSource();
    EXPECT_TRUE(source.valid());
    EXPECT_EQ(source.progress_xml, "progress.xml");
    SetActiveSource({});
}

namespace {

// Builds <root>/<id>/Xml/TROP.XML and <progress>/<id>.xml on disk.
class ScanFixture : public ::testing::Test {
protected:
    void SetUp() override {
        root = std::filesystem::temp_directory_path() /
               ("shadps4_trophy_scan_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "trophy_root");
        std::filesystem::create_directories(root / "progress");
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    void Title(const std::string& id, const std::string& name, std::string_view progress) {
        const auto xml_dir = root / "trophy_root" / id / "Xml";
        std::filesystem::create_directories(xml_dir);
        std::ofstream(xml_dir / "TROP.XML")
            << "<trophyconf><title-name>" << name << "</title-name>"
            << R"(<trophy id="0" ttype="B"><name>a</name><detail>b</detail></trophy>
                  <trophy id="1" ttype="G"><name>c</name><detail>d</detail></trophy></trophyconf>)";
        if (!progress.empty()) {
            std::ofstream(root / "progress" / (id + ".xml")) << progress;
        }
    }
    std::vector<TitleProgress> Scan() {
        return ScanTitles(root / "trophy_root", root / "progress");
    }
    std::filesystem::path root;
};

} // namespace

TEST_F(ScanFixture, ListsTitlesByRecentActivityThenName) {
    Title("NPWR00001_00", "Zebra Game",
          R"(<trophyconf><trophy id="0" unlockstate="true" timestamp="1000"/></trophyconf>)");
    Title("NPWR00002_00", "Alpha Game",
          R"(<trophyconf><trophy id="0" unlockstate="true" timestamp="2000"/>
             <trophy id="1" unlockstate="true" timestamp="3000"/></trophyconf>)");
    Title("NPWR00003_00", "Beta Game", "");
    Title("NPWR00004_00", "Aardvark Game", "");

    const auto titles = Scan();
    ASSERT_EQ(titles.size(), 4u);
    EXPECT_EQ(titles[0].title, "Alpha Game"); // earned most recently
    EXPECT_EQ(titles[0].last_earned, 3000u);
    EXPECT_EQ(titles[0].summary.unlocked, 2u);
    EXPECT_EQ(titles[0].summary.Percent(), 100u);
    EXPECT_EQ(titles[1].title, "Zebra Game");
    EXPECT_EQ(titles[1].summary.Percent(), 50u);
    EXPECT_EQ(titles[2].title, "Aardvark Game"); // no activity: by name
    EXPECT_EQ(titles[3].title, "Beta Game");
    EXPECT_EQ(titles[3].last_earned, 0u);
    EXPECT_EQ(titles[0].id, "NPWR00002_00");
    EXPECT_TRUE(titles[0].source.valid());
}

TEST_F(ScanFixture, SkipsFoldersWithoutUsableTrophyData) {
    Title("NPWR00001_00", "Good", "");
    std::filesystem::create_directories(root / "trophy_root" / "NPWR_EMPTY");
    std::filesystem::create_directories(root / "trophy_root" / "NPWR_BROKEN" / "Xml");
    std::ofstream(root / "trophy_root" / "NPWR_BROKEN" / "Xml" / "TROP.XML") << "<<< not xml";
    std::ofstream(root / "trophy_root" / "stray_file.txt") << "x";

    const auto titles = Scan();
    ASSERT_EQ(titles.size(), 1u);
    EXPECT_EQ(titles[0].title, "Good");
}

TEST_F(ScanFixture, MissingRootGivesAnEmptyList) {
    EXPECT_TRUE(ScanTitles(root / "does_not_exist", root / "progress").empty());
}
