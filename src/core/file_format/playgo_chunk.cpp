// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "playgo_chunk.h"

// All offsets and counts below come straight from the file, so they are validated against the
// size of the section they index into before being used.
static bool BuildChunks(const PlaygoHeader& header, const std::string& chunk_attrs_data,
                        const std::string& chunk_mchunks_data, const std::string& chunk_labels_data,
                        const std::string& mchunk_attrs_data, std::vector<PlaygoChunk>& chunks) {
    const u64 chunk_count = header.chunk_count;
    if (chunk_count * sizeof(playgo_chunk_attr_entry_t) > chunk_attrs_data.size()) {
        return false;
    }

    std::vector<PlaygoChunk> result(chunk_count);
    for (u64 i = 0; i < chunk_count; i++) {
        playgo_chunk_attr_entry_t attr;
        std::memcpy(&attr, chunk_attrs_data.data() + i * sizeof(attr), sizeof(attr));

        result[i].req_locus = attr.req_locus;
        result[i].language_mask = attr.language_mask;

        // The label is a NULL terminated string inside the labels section.
        if (attr.label_offset >= chunk_labels_data.size()) {
            return false;
        }
        const char* label = chunk_labels_data.data() + attr.label_offset;
        const size_t max_len = chunk_labels_data.size() - attr.label_offset;
        const size_t label_len = strnlen(label, max_len);
        if (label_len == max_len) {
            return false;
        }
        result[i].label_name.assign(label, label_len);

        u64 total_size = 0;
        const u64 mchunk_count = attr.mchunk_count;
        if (mchunk_count != 0) {
            if (u64{attr.mchunks_offset} + mchunk_count * sizeof(u16) > chunk_mchunks_data.size()) {
                return false;
            }
            for (u64 j = 0; j < mchunk_count; j++) {
                u16 mchunk_id;
                std::memcpy(&mchunk_id, chunk_mchunks_data.data() + attr.mchunks_offset + j * 2,
                            sizeof(mchunk_id));
                if ((u64{mchunk_id} + 1) * sizeof(playgo_mchunk_attr_entry_t) >
                    mchunk_attrs_data.size()) {
                    return false;
                }
                playgo_mchunk_attr_entry_t mchunk;
                std::memcpy(&mchunk, mchunk_attrs_data.data() + mchunk_id * sizeof(mchunk),
                            sizeof(mchunk));
                total_size += mchunk.size.size;
            }
        }
        result[i].total_size = total_size;
    }
    chunks = std::move(result);
    return true;
}

bool PlaygoFile::Open(const std::filesystem::path& filepath) {
    Common::FS::IOFile file(filepath, Common::FS::FileAccessMode::Read);
    if (file.IsOpen()) {
        if (file.GetSize() < sizeof(PlaygoHeader) || file.Read(playgoHeader) != 1) {
            return false;
        }
        if (LoadChunks(file)) {
            return true;
        }
    }
    return false;
}

bool PlaygoFile::Open(std::span<const u8> data) {
    if (data.size() < sizeof(PlaygoHeader)) {
        return false;
    }
    std::memcpy(&playgoHeader, data.data(), sizeof(PlaygoHeader));
    if (playgoHeader.magic != PLAYGO_MAGIC) {
        return false;
    }

    const auto load_chunk = [&](const chunk_t chunk, std::string& out) -> bool {
        // Sum in 64 bits: the 32-bit sum of two attacker controlled values can wrap around.
        if (u64{chunk.offset} + chunk.length > data.size()) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(data.data() + chunk.offset), chunk.length);
        return true;
    };

    std::string chunk_attrs_data, chunk_mchunks_data, chunk_labels_data, mchunk_attrs_data;
    if (!load_chunk(playgoHeader.chunk_attrs, chunk_attrs_data))
        return false;
    if (!load_chunk(playgoHeader.chunk_mchunks, chunk_mchunks_data))
        return false;
    if (!load_chunk(playgoHeader.chunk_labels, chunk_labels_data))
        return false;
    if (!load_chunk(playgoHeader.mchunk_attrs, mchunk_attrs_data))
        return false;

    return BuildChunks(playgoHeader, chunk_attrs_data, chunk_mchunks_data, chunk_labels_data,
                       mchunk_attrs_data, chunks);
}

bool PlaygoFile::LoadChunks(const Common::FS::IOFile& file) {
    if (file.IsOpen()) {
        if (playgoHeader.magic == PLAYGO_MAGIC) {
            bool ret = true;

            std::string chunk_attrs_data, chunk_mchunks_data, chunk_labels_data, mchunk_attrs_data;
            ret = ret && load_chunk_data(file, playgoHeader.chunk_attrs, chunk_attrs_data);
            ret = ret && load_chunk_data(file, playgoHeader.chunk_mchunks, chunk_mchunks_data);
            ret = ret && load_chunk_data(file, playgoHeader.chunk_labels, chunk_labels_data);
            ret = ret && load_chunk_data(file, playgoHeader.mchunk_attrs, mchunk_attrs_data);

            return ret && BuildChunks(playgoHeader, chunk_attrs_data, chunk_mchunks_data,
                                      chunk_labels_data, mchunk_attrs_data, chunks);
        }
    }
    return false;
}

bool PlaygoFile::load_chunk_data(const Common::FS::IOFile& file, const chunk_t chunk,
                                 std::string& data) {
    if (file.IsOpen()) {
        // Never allocate more than the file can actually provide.
        if (u64{chunk.offset} + chunk.length > file.GetSize()) {
            return false;
        }
        if (file.Seek(chunk.offset)) {
            data.resize(chunk.length);
            return file.ReadRaw<char>(&data[0], chunk.length) == chunk.length;
        }
    }
    return false;
}
