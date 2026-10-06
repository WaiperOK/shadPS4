// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "core/file_format/npbind.h"
#include "core/file_format/playgo_chunk.h"
#include "core/file_format/psf.h"

namespace {

// ---------------------------------------------------------------------------
// PSF
// ---------------------------------------------------------------------------

std::vector<u8> MakePsf() {
    PSF psf;
    psf.AddString("TITLE", "Hello World");
    psf.AddString("TITLE_ID", "CUSA00001");
    psf.AddInteger("APP_VER", 5);
    psf.AddBinary("BIN", std::vector<u8>{1, 2, 3, 4, 5, 6, 7, 8});
    return psf.Encode();
}

// Layout of the encoded file: 20 byte header followed by 16 byte index entries.
constexpr size_t PSF_HEADER_SIZE = 20;
constexpr size_t PSF_ENTRY_SIZE = 16;

void Put32(std::vector<u8>& buf, size_t offset, u32 value) {
    std::memcpy(buf.data() + offset, &value, sizeof(value));
}

} // namespace

TEST(Psf, RoundTrip) {
    PSF psf;
    ASSERT_TRUE(psf.Open(MakePsf()));
    EXPECT_EQ(psf.GetString("TITLE"), "Hello World");
    EXPECT_EQ(psf.GetString("TITLE_ID"), "CUSA00001");
    EXPECT_EQ(psf.GetInteger("APP_VER"), 5);
    ASSERT_TRUE(psf.GetBinary("BIN").has_value());
    EXPECT_EQ(psf.GetBinary("BIN")->size(), 8u);
}

TEST(Psf, GettersReturnNothingOnFormatMismatch) {
    PSF psf;
    ASSERT_TRUE(psf.Open(MakePsf()));
    EXPECT_FALSE(psf.GetInteger("TITLE").has_value());
    EXPECT_FALSE(psf.GetString("APP_VER").has_value());
    EXPECT_FALSE(psf.GetBinary("TITLE").has_value());
}

TEST(Psf, RejectsEmptyAndTruncatedInput) {
    const auto good = MakePsf();
    PSF empty;
    EXPECT_FALSE(empty.Open(std::vector<u8>{}));
    // No prefix of a valid file may crash the parser.
    for (size_t len = 0; len < good.size(); ++len) {
        PSF psf;
        psf.Open(std::vector<u8>(good.begin(), good.begin() + len));
    }
}

TEST(Psf, RejectsBadMagic) {
    auto buf = MakePsf();
    buf[0] ^= 0xFF;
    PSF psf;
    EXPECT_FALSE(psf.Open(buf));
}

TEST(Psf, RejectsHugeIndexTable) {
    auto buf = MakePsf();
    Put32(buf, 16, 0xFFFFFFFF); // index_table_entries
    PSF psf;
    EXPECT_FALSE(psf.Open(buf));
}

TEST(Psf, RejectsTablesOutsideTheFile) {
    auto buf = MakePsf();
    Put32(buf, 8, 0x7FFFFFFF); // key_table_offset
    PSF key_psf;
    EXPECT_FALSE(key_psf.Open(buf));

    buf = MakePsf();
    Put32(buf, 12, 0x7FFFFFFF); // data_table_offset
    PSF data_psf;
    EXPECT_FALSE(data_psf.Open(buf));
}

TEST(Psf, RejectsOutOfBoundsEntryData) {
    auto buf = MakePsf();
    // First entry: param_len (offset 4) and data_offset (offset 12).
    Put32(buf, PSF_HEADER_SIZE + 12, 0x0FFFFFFF);
    PSF psf;
    EXPECT_FALSE(psf.Open(buf));
}

TEST(Psf, RejectsIntegerWithWrongSize) {
    auto buf = MakePsf();
    // The third entry is the integer: break its param_len.
    Put32(buf, PSF_HEADER_SIZE + 2 * PSF_ENTRY_SIZE + 4, 2);
    PSF psf;
    EXPECT_FALSE(psf.Open(buf));
}

TEST(Psf, RejectsUnknownFormat) {
    auto buf = MakePsf();
    buf[PSF_HEADER_SIZE + 2] = 0x7F; // param_fmt (big endian)
    PSF psf;
    EXPECT_FALSE(psf.Open(buf));
}

TEST(Psf, RejectsUnterminatedKey) {
    auto buf = MakePsf();
    // Overwrite everything from the key table to the end with non-zero bytes.
    u32 key_table_offset;
    std::memcpy(&key_table_offset, buf.data() + 8, sizeof(key_table_offset));
    std::fill(buf.begin() + key_table_offset, buf.end(), u8{'A'});
    PSF psf;
    EXPECT_FALSE(psf.Open(buf));
}

// ---------------------------------------------------------------------------
// NPBIND
// ---------------------------------------------------------------------------

namespace {

std::vector<u8> MakeNpBind(u64 num_entries = 1) {
    std::vector<u8> buf(sizeof(NpBindHeader), 0);
    const u32 magic = NPBIND_MAGIC;
    buf[0] = static_cast<u8>(magic >> 24);
    buf[1] = static_cast<u8>(magic >> 16);
    buf[2] = static_cast<u8>(magic >> 8);
    buf[3] = static_cast<u8>(magic);
    for (int i = 0; i < 8; ++i) { // num_entries is a big endian u64 at offset 24
        buf[24 + i] = static_cast<u8>(num_entries >> (8 * (7 - i)));
    }
    const auto entry = [&](u16 type, u16 size) {
        buf.push_back(static_cast<u8>(type >> 8));
        buf.push_back(static_cast<u8>(type));
        buf.push_back(static_cast<u8>(size >> 8));
        buf.push_back(static_cast<u8>(size));
        buf.insert(buf.end(), size, 'N');
    };
    entry(0x10, 12);
    entry(0x11, 12);
    entry(0x12, 176);
    entry(0x13, 16);
    buf.insert(buf.end(), 0x98 + 20, 0);
    return buf;
}

} // namespace

TEST(NpBind, ParsesValidFile) {
    NPBindFile file;
    ASSERT_TRUE(file.Load(std::span<const u8>(MakeNpBind())));
    EXPECT_EQ(file.GetNpCommIds().size(), 1u);
}

TEST(NpBind, RejectsEntryCountLargerThanTheFile) {
    NPBindFile file;
    EXPECT_FALSE(file.Load(std::span<const u8>(MakeNpBind(0xFFFFFFFFFFFFFFFFull))));
    EXPECT_FALSE(file.Load(std::span<const u8>(MakeNpBind(1000000))));
}

TEST(NpBind, TruncatedInputNeverCrashes) {
    const auto good = MakeNpBind();
    for (size_t len = 0; len < good.size(); ++len) {
        NPBindFile file;
        file.Load(std::span<const u8>(good.data(), len));
    }
}

// ---------------------------------------------------------------------------
// PlayGo
// ---------------------------------------------------------------------------

namespace {

std::vector<u8> MakePlaygo() {
    PlaygoHeader header{};
    header.magic = PLAYGO_MAGIC;
    header.chunk_count = 2;
    header.mchunk_count = 2;

    std::vector<u8> attrs(2 * sizeof(playgo_chunk_attr_entry_t), 0);
    std::vector<u8> mchunks(8, 0);
    std::vector<u8> labels(16, 0);
    std::vector<u8> mchunk_attrs(2 * sizeof(playgo_mchunk_attr_entry_t), 0);

    playgo_chunk_attr_entry_t attr{};
    attr.mchunk_count = 2;
    std::memcpy(attrs.data(), &attr, sizeof(attr));
    attr.mchunk_count = 0;
    attr.label_offset = 4;
    std::memcpy(attrs.data() + sizeof(attr), &attr, sizeof(attr));
    labels[0] = 'a';
    labels[4] = 'b';
    mchunks[2] = 1;

    size_t offset = sizeof(PlaygoHeader);
    const auto place = [&](chunk_t& chunk, const std::vector<u8>& data) {
        chunk.offset = static_cast<u32>(offset);
        chunk.length = static_cast<u32>(data.size());
        offset += data.size();
    };
    place(header.chunk_attrs, attrs);
    place(header.chunk_mchunks, mchunks);
    place(header.chunk_labels, labels);
    place(header.mchunk_attrs, mchunk_attrs);

    std::vector<u8> buf(offset, 0);
    std::memcpy(buf.data(), &header, sizeof(header));
    const auto copy = [&](const chunk_t& chunk, const std::vector<u8>& data) {
        std::memcpy(buf.data() + chunk.offset, data.data(), data.size());
    };
    copy(header.chunk_attrs, attrs);
    copy(header.chunk_mchunks, mchunks);
    copy(header.chunk_labels, labels);
    copy(header.mchunk_attrs, mchunk_attrs);
    return buf;
}

} // namespace

TEST(PlayGo, ParsesValidFile) {
    PlaygoFile file;
    ASSERT_TRUE(file.Open(std::span<const u8>(MakePlaygo())));
    ASSERT_EQ(file.chunks.size(), 2u);
    EXPECT_EQ(file.chunks[0].label_name, "a");
    EXPECT_EQ(file.chunks[1].label_name, "b");
}

TEST(PlayGo, RejectsSectionOffsetOverflow) {
    auto buf = MakePlaygo();
    PlaygoHeader header;
    std::memcpy(&header, buf.data(), sizeof(header));
    // offset + length wraps around 32 bits to a small value.
    header.chunk_labels.offset = 0xFFFFFFF0u;
    header.chunk_labels.length = 0x100;
    std::memcpy(buf.data(), &header, sizeof(header));
    PlaygoFile file;
    EXPECT_FALSE(file.Open(std::span<const u8>(buf)));
}

TEST(PlayGo, RejectsChunkCountLargerThanTheAttributeTable) {
    auto buf = MakePlaygo();
    PlaygoHeader header;
    std::memcpy(&header, buf.data(), sizeof(header));
    header.chunk_count = 60000;
    std::memcpy(buf.data(), &header, sizeof(header));
    PlaygoFile file;
    EXPECT_FALSE(file.Open(std::span<const u8>(buf)));
}

TEST(PlayGo, RejectsLabelOffsetOutsideTheLabelSection) {
    auto buf = MakePlaygo();
    PlaygoHeader header;
    std::memcpy(&header, buf.data(), sizeof(header));
    playgo_chunk_attr_entry_t attr;
    std::memcpy(&attr, buf.data() + header.chunk_attrs.offset, sizeof(attr));
    attr.label_offset = 0x10000;
    std::memcpy(buf.data() + header.chunk_attrs.offset, &attr, sizeof(attr));
    PlaygoFile file;
    EXPECT_FALSE(file.Open(std::span<const u8>(buf)));
}

TEST(PlayGo, RejectsMchunkIndexOutsideTheAttributeTable) {
    auto buf = MakePlaygo();
    PlaygoHeader header;
    std::memcpy(&header, buf.data(), sizeof(header));
    buf[header.chunk_mchunks.offset] = 0xFF;
    buf[header.chunk_mchunks.offset + 1] = 0xFF;
    PlaygoFile file;
    EXPECT_FALSE(file.Open(std::span<const u8>(buf)));
}

TEST(PlayGo, TruncatedInputNeverCrashes) {
    const auto good = MakePlaygo();
    for (size_t len = 0; len < good.size(); ++len) {
        PlaygoFile file;
        file.Open(std::span<const u8>(good.data(), len));
    }
}
