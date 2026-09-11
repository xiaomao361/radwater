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
    uint32_t offsets[FormCount] = {};
    uint16_t order[FormCount] = {};
    Storage* storage = nullptr;
    uint32_t typesSeen = 0, fishSeen = 0;
    static bool test(const uint8_t* map, uint32_t i) { return (map[i / 8] & (1u << (i % 8))) != 0; }
    static void mark(uint8_t* map, uint32_t i) { map[i / 8] |= uint8_t(1u << (i % 8)); }
public:
    SaveState state = SaveState::Missing;
    uint32_t records = 0, discoveries = 0;
    uint32_t knownFishSpecies() const { return fishSeen; }
    uint32_t knownObjectTypes() const { return typesSeen; }
    bool known(const Catch& c) const { return c.index() < FormCount && test(seen, c.index()); }
    void load(Storage& io) {
        storage = &io; records = discoveries = typesSeen = fishSeen = 0; std::memset(seen, 0, sizeof seen);
        uint32_t bytes = 0;
        if (!io.size(bytes)) { state = SaveState::Missing; return; }
        state = SaveState::Ready;
        uint8_t b[RecordBytes]; Catch c;
        for (uint32_t pos = 0; uint64_t(pos) + RecordBytes <= bytes; pos += RecordBytes) {
            if (!io.read(pos, b, RecordBytes) || !decode(b, records + 1, c)) { state = SaveState::Corrupt; return; }
            if (!known(c)) { offsets[c.index()]=records*RecordBytes;order[discoveries]=uint16_t(c.index());mark(seen, c.index()); ++discoveries; }
            if (c.object()) typesSeen |= 1u << c.objectType(); else fishSeen |= 1u << c.species();
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
        if (!known(c)) { offsets[c.index()]=records*RecordBytes;order[discoveries]=uint16_t(c.index());mark(seen, c.index()); ++discoveries; }
        if (c.object()) typesSeen |= 1u << c.objectType(); else fishSeen |= 1u << c.species();
        ++records; return true;
    }
    bool discovery(uint32_t ordinal, Catch& out) {
        if(!storage||ordinal>=discoveries)return false;
        const unsigned id=order[ordinal];const uint32_t offset=offsets[id];uint8_t b[RecordBytes];Catch c;
        if(!storage->read(offset,b,RecordBytes)||!decode(b,offset/RecordBytes+1,c)||c.index()!=id){state=SaveState::Corrupt;return false;}
        out=c;return true;
    }
    bool find(uint32_t id,uint32_t& ordinal,Catch& out){
        for(unsigned i=0;i<discoveries;++i)if(order[i]==id){if(!discovery(i,out))return false;ordinal=i;return true;}
        return false;
    }
};
}
