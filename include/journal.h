#pragma once
#include "game.h"
#include <cstring>
namespace pond {
// All offsets and lengths are explicit; absent media is not an empty healthy save.
class Storage {
public:
    virtual ~Storage() = default;
    virtual bool size(uint32_t& bytes) = 0;
    virtual bool read(uint32_t offset, uint8_t* out, size_t count) = 0;
    virtual bool append(const uint8_t* bytes, size_t count) = 0;
};
enum class SaveState { Missing, Ready, Corrupt, WriteFailed };
class Journal {
    static constexpr unsigned MapBytes = (FormCount + 7) / 8;
    uint8_t seen[MapBytes] = {};
    uint8_t scratch[MapBytes] = {};
    Storage* storage = nullptr;
    static bool test(const uint8_t* map, uint32_t i) { return (map[i / 8] & (1u << (i % 8))) != 0; }
    static void mark(uint8_t* map, uint32_t i) { map[i / 8] |= uint8_t(1u << (i % 8)); }
public:
    SaveState state = SaveState::Missing;
    uint32_t records = 0, discoveries = 0;
    bool known(const Catch& c) const { return c.index() < FormCount && test(seen, c.index()); }
    void load(Storage& io) {
        storage = &io; records = discoveries = 0; std::memset(seen, 0, sizeof seen);
        uint32_t bytes = 0;
        if (!io.size(bytes)) { state = SaveState::Missing; return; }
        state = SaveState::Ready;
        uint8_t b[RecordBytes]; Catch c;
        for (uint32_t pos = 0; uint64_t(pos) + RecordBytes <= bytes; pos += RecordBytes) {
            if (!io.read(pos, b, RecordBytes) || !decode(b, records + 1, c)) { state = SaveState::Corrupt; return; }
            if (!known(c)) { mark(seen, c.index()); ++discoveries; }
            ++records;
        }
        if (bytes % RecordBytes) state = SaveState::Corrupt;
    }
    bool save(const Catch& c) {
        if (state != SaveState::Ready || !storage) return false;
        uint8_t b[RecordBytes], verify[RecordBytes]; Catch parsed;
        if (records >= UINT32_MAX / RecordBytes - 1) { state = SaveState::WriteFailed; return false; }
        encode(c, records + 1, b);
        uint32_t before = 0, after = 0;
        if (!decode(b, records + 1, parsed) || !storage->size(before) || before != records * RecordBytes ||
            !storage->append(b, RecordBytes) || !storage->size(after) || after != before + RecordBytes ||
            !storage->read(before, verify, RecordBytes) || std::memcmp(b, verify, RecordBytes) != 0) {
            state = SaveState::WriteFailed; return false;
        }
        if (!known(c)) { mark(seen, c.index()); ++discoveries; }
        ++records; return true;
    }
    bool discovery(uint32_t ordinal, Catch& out) {
        if (!storage || ordinal >= discoveries) return false;
        std::memset(scratch, 0, sizeof scratch);
        uint8_t b[RecordBytes]; Catch c; uint32_t unique = 0;
        for (uint32_t i = 0; i < records; ++i) {
            if (!storage->read(i * RecordBytes, b, RecordBytes) || !decode(b, i + 1, c)) {
                state = SaveState::Corrupt; return false;
            }
            if (test(scratch, c.index())) continue;
            mark(scratch, c.index());
            if (unique++ == ordinal) { out = c; return true; }
        }
        state = SaveState::Corrupt; return false;
    }
};
}
