#pragma once
#include "journal.h"
#include "lore.h"
namespace pond {
// Independent append-only snapshots: no rewrite/migration of the irreplaceable catch journal.
// Version 2: 32 little-endian words, final CRC. Version 1 is read-only. Retain validated prefix, lock on any failed append.
struct ReadingState {
    uint32_t legacyRead=0;
    uint32_t fishRead=0, objectRead=0, annotationsRead=0, fishNotesRead=0;
    uint32_t bookmark=FormCount, page=0, count=0;
    uint32_t events[12]={};
    uint64_t objectPages[3]={};
    static unsigned storyId(const Catch& c){return c.objectType()<8?c.objectType()*4+((c.form>>6)&3):c.objectType()+24;}
    bool pageRead(const Catch& c,unsigned p)const{return p<3 && bool(objectPages[p]&(uint64_t(1)<<storyId(c)));}
    bool started(const Catch& c)const{return c.object()?(pageRead(c,0)||pageRead(c,1)||pageRead(c,2)):mainRead(c);}
    bool mainRead(const Catch& c)const {
        if(!c.object())return fishRead&(1u<<c.species());
        return pageRead(c,0)&&pageRead(c,1)&&pageRead(c,2);
    }
    bool noteUnread(const Catch& c,uint32_t known)const {
        return c.object()?bool(annotationBit(c,known)&~annotationsRead):fishAnnotation(c,known)&&!(fishNotesRead&(1u<<c.species()));
    }
    bool unread(const Catch& c,uint32_t known)const{return !mainRead(c)||noteUnread(c,known);}
    unsigned entryPage(const Catch& c,uint32_t known)const{
        if(mainRead(c)&&noteUnread(c,known))return c.object()?3:1;
        if(c.object())for(unsigned p=0;p<3;++p)if(!pageRead(c,p))return p;
        return 0;
    }
    void read(const Catch& c,unsigned p,uint32_t known,bool saved){
        if(c.object()){
            if(p<3)objectPages[p]|=uint64_t(1)<<storyId(c);
            if(p==0){if(c.objectType()<8)legacyRead|=1u<<(c.objectType()*4+((c.form>>6)&3));else objectRead|=1u<<c.objectType();}
            if(p==3)annotationsRead|=annotationBit(c,known);
        }else{if(p==0)fishRead|=1u<<c.species();else fishNotesRead|=1u<<c.species();}
        if(saved){bookmark=c.index();page=p;}
    }
    void addEvent(uint32_t code) {
        if(count && (events[0]&255)==(code&255) && (code&(1u<<16))) {events[0]=code;return;}
        for(unsigned i=11;i>0;--i)events[i]=events[i-1];events[0]=code;
        if(count<12)++count;
    }
};
class Notebook {
    Storage* io=nullptr;
    uint32_t records=0;
    bool legacyMode=false;
    ReadingState persisted;
    static uint32_t get(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
    static void put(uint8_t* p,uint32_t x){for(unsigned i=0;i<4;++i)p[i]=uint8_t(x>>(8*i));}
    static bool parseLegacy(const uint8_t* b,uint32_t seq,ReadingState& s){
        if(get(b)!=0x314e4650||get(b+4)!=seq||get(b+92)!=crc32(b,92))return false;
        if(get(b+8)>65535||get(b+12)>0xffffff||get(b+16)>4095||get(b+20)>65535||get(b+24)>FormCount||get(b+28)>3||get(b+32)>12)return false;
        if(get(b+88))return false;
        ReadingState next;next.legacyRead=get(b+84);next.fishRead=get(b+8);next.objectRead=get(b+12);next.annotationsRead=get(b+16);next.fishNotesRead=get(b+20);
        next.bookmark=get(b+24);next.page=get(b+28);next.count=get(b+32);
        for(unsigned i=0;i<12;++i){uint32_t code=get(b+36+i*4);if(i<next.count){if((code&255)<1||(code&255)>EventCount||(code&~0x103ffu)||((code&(1u<<16))&&(code&255)!=unsigned(Anomaly::Knock)))return false;}else if(code)return false;next.events[i]=code;}
        s=next;return true;
    }
    static bool parse(const uint8_t* b,uint32_t seq,ReadingState& s){
        if(get(b)!=0x324e4650||get(b+124)!=crc32(b,124)||get(b+116)||get(b+120))return false;
        uint8_t old[96];std::memcpy(old,b,92);put(old,0x314e4650);put(old+92,crc32(old,92));
        ReadingState next;if(!parseLegacy(old,seq,next))return false;
        for(unsigned p=0;p<3;++p){next.objectPages[p]=uint64_t(get(b+92+p*8))|(uint64_t(get(b+96+p*8))<<32);if(next.objectPages[p]>>48)return false;}
        s=next;return true;
    }

public:
    ReadingState data;
    SaveState state=SaveState::Missing;
    void load(Storage& storage,bool legacy=false){
        legacyMode=legacy;const unsigned stride=legacy?96:128;
        io=&storage;records=0;data=ReadingState{};uint32_t bytes=0;
        if(!io->size(bytes)){state=SaveState::Missing;return;}state=SaveState::Ready;
        uint8_t b[128];
        for(uint32_t off=0;uint64_t(off)+stride<=bytes;off+=stride){
            ReadingState next;if(!io->read(off,b,stride)||!(legacy?parseLegacy(b,records+1,next):parse(b,records+1,next))){state=SaveState::Corrupt;return;}
            data=next;persisted=next;++records;
        }
        if(bytes%stride)state=SaveState::Corrupt;
    }
    void importLegacy(const Notebook& old){
        if(state!=SaveState::Ready||records)return;
        data=old.data;
        // Old flags prove page zero was opened, never that the other pages were read.
        data.objectPages[0]=data.legacyRead | (uint64_t(data.objectRead>>8)<<32);
        if(old.state!=SaveState::Ready)state=old.state;
    }
    bool save(const ReadingState& next){
        if(state!=SaveState::Ready||!io||legacyMode)return false;
        uint8_t b[128]={},check[128];put(b,0x324e4650);put(b+4,records+1);
        put(b+8,next.fishRead);put(b+12,next.objectRead);put(b+16,next.annotationsRead);put(b+20,next.fishNotesRead);
        put(b+24,next.bookmark);put(b+28,next.page);put(b+32,next.count);
        for(unsigned i=0;i<12;++i)put(b+36+i*4,next.events[i]);put(b+84,next.legacyRead);
        for(unsigned p=0;p<3;++p){put(b+92+p*8,uint32_t(next.objectPages[p]));put(b+96+p*8,uint32_t(next.objectPages[p]>>32));}
        put(b+124,crc32(b,124));
        ReadingState parsed;uint32_t before=0,after=0;
        if(!parse(b,records+1,parsed)){state=SaveState::WriteFailed;return false;}
        bool same=next.legacyRead==persisted.legacyRead&&next.fishRead==persisted.fishRead&&next.objectRead==persisted.objectRead&&next.annotationsRead==persisted.annotationsRead&&next.fishNotesRead==persisted.fishNotesRead&&next.bookmark==persisted.bookmark&&next.page==persisted.page&&next.count==persisted.count&& !std::memcmp(next.events,persisted.events,sizeof next.events)&& !std::memcmp(next.objectPages,persisted.objectPages,sizeof next.objectPages);
        if(records&&same)return true;
        if(records>=UINT32_MAX/128-1||!parse(b,records+1,parsed)||!io->size(before)||before!=records*128||!io->append(b,128)||!io->size(after)||after!=before+128||!io->read(before,check,128)||std::memcmp(b,check,128)) {state=SaveState::WriteFailed;return false;}
        data=next;persisted=next;++records;return true;
    }
};
}
