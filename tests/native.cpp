#include "game.h"
#include "journal.h"
#include "view.h"
#include "lore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
using namespace pond;
struct MemoryStorage : Storage {
    std::vector<uint8_t> bytes;
    bool present=true, failRead=false, failWrite=false, partial=false;
    bool size(uint32_t& n) override {n=uint32_t(bytes.size());return present;}
    bool read(uint32_t off,uint8_t* b,size_t n) override {
        if(failRead||!present||uint64_t(off)+n>bytes.size())return false;
        std::memcpy(b,bytes.data()+off,n);return true;
    }
    bool append(const uint8_t* b,size_t n) override {
        if(failWrite||!present)return false;
        bytes.insert(bytes.end(),b,b+(partial?n/2:n));return !partial;
    }
};
static void tick(Game& g,Input i={},unsigned frames=1) {for(unsigned n=0;n<frames;++n)g.tick(1.0f/60,i);}
static void hook(Game& g) {
    tick(g,{true});tick(g,{});
    for(unsigned n=0;n<600&&g.stage==Stage::Waiting;++n)tick(g);
    assert(g.stage==Stage::Bite);tick(g,{true});assert(g.stage==Stage::Fight);
}
static void writePPM(const char* path,const uint16_t* buffer) {
    std::ofstream out(path,std::ios::binary);assert(out.good());out<<"P6\n240 135\n255\n";
    for(int i=0;i<Width*Height;++i) {uint16_t p=buffer[i];char rgb8[]={char(((p>>11)&31)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};out.write(rgb8,3);}
}
static void snapshot(const char* name,const Game& g,const ViewState& v,uint32_t ms=1234) {
    uint16_t pixels[Width*Height];Canvas c(pixels);draw(c,g,v,ms);
    char path[160];std::snprintf(path,sizeof path,"build/%s.ppm",name);writePPM(path,pixels);
}
static void tests() {
    assert(crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);
    // Fixture captured from unmodified v0.1.1 before this content/control update.
    // New dossiers supplement the identity; neither the generator nor saved bytes may drift.
    MemoryStorage legacy;
    std::ifstream fixture("tests/fixtures/legacy-v1.pfj",std::ios::binary);
    assert(fixture.good());legacy.bytes.assign(std::istreambuf_iterator<char>(fixture),{});
    assert(legacy.bytes.size()==32*RecordBytes);
    for(unsigned i=0;i<32;++i) {
        uint8_t expected[RecordBytes];encode(generate(i*31337,i%3,1),i+1,expected);
        assert(std::memcmp(expected,legacy.bytes.data()+i*RecordBytes,RecordBytes)==0);
    }
    auto originalBytes=legacy.bytes;Journal oldBook;oldBook.load(legacy);
    assert(oldBook.records==32&&oldBook.state==SaveState::Ready&&legacy.bytes==originalBytes);
    for(unsigned i=0;i<oldBook.discoveries;++i){Catch c;assert(oldBook.discovery(i,c));assert(dossier(c,0).title[0]);}
    assert(legacy.bytes==originalBytes);
    // Append v2 to a v1 journal, preserving the exact prefix and distinct extended IDs.
    Catch legacyBoot;legacyBoot.form=ObjectFlag;legacyBoot.millimetres=99;legacyBoot.generator=1;
    Catch cap=legacyBoot;cap.form=ObjectFlag+256;cap.generator=2;
    Catch photo=cap;photo.form=ObjectFlag+512;
    unsigned beforeDiscoveries=oldBook.discoveries;
    assert(oldBook.save(legacyBoot));unsigned afterBoot=oldBook.discoveries;
    assert(afterBoot>=beforeDiscoveries);
    assert(oldBook.save(cap)&&oldBook.save(photo));assert(oldBook.discoveries==afterBoot+2);
    assert(std::memcmp(legacy.bytes.data(),originalBytes.data(),originalBytes.size())==0);
    Journal mixed;mixed.load(legacy);assert(mixed.state==SaveState::Ready&&mixed.records==35&&mixed.discoveries==oldBook.discoveries);
    Catch last;assert(mixed.discovery(mixed.discoveries-1,last)&&last.form==photo.form&&last.generator==2);
    std::ofstream mixedFile("build/mixed-v1-v2.pfj",std::ios::binary);mixedFile.write(reinterpret_cast<const char*>(legacy.bytes.data()),legacy.bytes.size());mixedFile.close();
    for(auto badCatch:{cap,photo}){badCatch.generator=1;uint8_t bytes[RecordBytes];encode(badCatch,1,bytes);Catch out;assert(!decode(bytes,1,out));}
    for(unsigned version:{0u,3u,UINT32_MAX}){Catch invalid=photo;invalid.generator=version;uint8_t bytes[RecordBytes];encode(invalid,1,bytes);Catch out;assert(!decode(bytes,1,out));}
    for(unsigned f:{ObjectFlag+ObjectForms,ObjectFlag+1024,UINT32_MAX}){Catch invalid=photo;invalid.form=f;uint8_t bytes[RecordBytes];encode(invalid,1,bytes);Catch out;assert(!decode(bytes,1,out));assert(!mixed.known(invalid));}
    std::set<uint32_t> objectIDs;
    std::set<uint32_t> identities;
    unsigned fishN=0,objectN=0;unsigned traits[7][8]={};
    for(unsigned i=0;i<100000;++i) {
        Catch c=generate(i,i%3),d=generate(i,i%3);
        assert(c.form==d.form&&c.millimetres==d.millimetres);
        assert(c.index()<FormCount&&c.millimetres>0);
        uint8_t encoded[RecordBytes];encode(c,i+1,encoded);Catch restored;
        assert(decode(encoded,i+1,restored));assert(restored.form==c.form&&restored.seed==c.seed);
        assert(!decode(encoded,i+2,restored));
        encoded[17]^=1;assert(!decode(encoded,i+1,restored));
        identities.insert(c.index());
        Catch old=generate(i,i%3,1);assert(old.object()==c.object());
        if(c.object()){++objectN;objectIDs.insert(c.form);assert(c.objectType()<ObjectTypes);}
        else {assert(old.form==c.form&&old.millimetres==c.millimetres);++fishN;unsigned t[]={c.body(),c.tail(),c.fin(),c.palette(),c.pattern(),c.face(),c.ornament()};for(int j=0;j<7;++j)++traits[j][t[j]];}
    }
    assert(objectN>10000&&objectN<12500&&identities.size()>40000);
    assert(objectIDs.size()==ObjectForms);
    std::set<unsigned> trail;unsigned next=0;
    for(unsigned i=0;i<ObjectTypes;++i){assert(next<ObjectTypes&&trail.insert(next).second);Catch c;c.form=ObjectFlag|(next&7)|((next&24)<<5);assert(c.objectType()==next);unsigned ref=evidenceNext(next);assert(ref<ObjectTypes);assert(std::strstr(dossier(c,1).lines[5],objectName(ref)));next=ref;}
    assert(next==0&&trail.size()==ObjectTypes);
    unsigned limits[]={8,4,4,8,8,4,4};
    for(int j=0;j<7;++j)for(unsigned k=0;k<limits[j];++k)assert(traits[j][k]);
    MemoryStorage io;Journal j;j.load(io);assert(j.state==SaveState::Ready&&j.records==0);
    Catch a=generate(10,0),b=a;b.millimetres+=1;b.seed+=1;
    assert(j.save(a)&&j.save(b));assert(j.records==2&&j.discoveries==1);
    Catch object=a;object.form=ObjectFlag+(a.form&255);assert(j.save(object));assert(j.discoveries==2);
    Catch read;assert(j.discovery(0,read)&&read.form==a.form);assert(j.discovery(1,read)&&read.object());assert(!j.discovery(2,read));
    Journal restart;restart.load(io);assert(restart.records==3&&restart.discoveries==2);
    // Partial append is visible immediately, locks further writes, retains valid prefix on restart.
    io.partial=true;assert(!j.save(a));assert(j.state==SaveState::WriteFailed&&j.records==3);
    size_t damagedSize=io.bytes.size();assert(!j.save(a));assert(io.bytes.size()==damagedSize);
    restart.load(io);assert(restart.state==SaveState::Corrupt&&restart.records==3&&restart.discoveries==2);
    assert(restart.discovery(0,read));assert(io.bytes.size()==damagedSize);
    MemoryStorage missing;missing.present=false;Journal absent;absent.load(missing);assert(absent.state==SaveState::Missing&&!absent.save(a));
    MemoryStorage bad;bad.bytes=io.bytes;bad.bytes[5]^=4;Journal corrupt;corrupt.load(bad);assert(corrupt.state==SaveState::Corrupt&&corrupt.records==0);
    MemoryStorage diskFull;Journal full;full.load(diskFull);diskFull.failWrite=true;assert(!full.save(a)&&full.state==SaveState::WriteFailed&&full.records==0);
    Game idle(20);tick(idle,{},5000);assert(idle.landed==0&&idle.stage==Stage::Shore);
    Game held(20);tick(held,{true},2000);assert(held.landed==0&&held.stage==Stage::Lost);
    Game early(20);tick(early,{true});tick(early,{});tick(early,{true});assert(early.stage==Stage::Lost&&early.loss==Loss::Early);
    Game unattended(42);hook(unattended);tick(unattended,{},2500);assert(unattended.stage==Stage::Lost&&unattended.landed==0);
    Game forced(42);hook(forced);tick(forced,{true},2500);assert(forced.stage==Stage::Lost&&forced.loss==Loss::Broken);
    Game paused(5);hook(paused);Input p;p.pause=true;tick(paused,p);float t=paused.fightAge;tick(paused,{},1200);assert(paused.fightAge==t&&paused.paused);tick(paused,p);assert(!paused.paused);
    Game help(8);hook(help);Input h;h.help=true;tick(help,h);t=help.fightAge;tick(help,{},1200);assert(help.stage==Stage::Help&&help.fightAge==t);tick(help,h);assert(help.stage==Stage::Fight);
    unsigned wins=0;float totalTime=0;
    for(unsigned seed=1;seed<=300;++seed) {
        Game g(seed);hook(g);bool reel=true;
        for(unsigned n=0;n<3000&&g.stage==Stage::Fight;++n) {
            Input in;
            in.left=g.rod>g.fish+0.015f;in.right=g.rod<g.fish-0.015f;
            if(g.tension>0.7f||g.surging())reel=false;
            if(g.tension<0.25f&&!g.surging())reel=true;
            in.action=reel&&g.aligned();tick(g,in);
        }
        if(g.stage==Stage::Caught){++wins;totalTime+=g.fightAge;assert(g.newCatch&&g.landed==1);tick(g,{},10);assert(g.landed==1);}
    }
    assert(wins>=290);
    unsigned assistedWins=0,heldLosses=0;float assistedTime=0;
    for(unsigned seed=1;seed<=300;++seed) {
        Game g(seed);hook(g);bool reel=true;
        for(unsigned n=0;n<3000&&g.stage==Stage::Fight;++n) {
            // Deliberately no A/D and decisions only every 200ms, using tension alone.
            if(n%12==0){if(g.tension>.68f)reel=false;else if(g.tension<.25f)reel=true;}
            Input in;in.action=reel;tick(g,in);
        }
        if(g.stage==Stage::Caught){++assistedWins;assistedTime+=g.fightAge;}
        Game heldFight(seed);hook(heldFight);tick(heldFight,{true},2500);
        if(heldFight.stage==Stage::Lost&&heldFight.loss==Loss::Broken)++heldLosses;
    }
    assert(assistedWins>=290&&heldLosses==300);
    Game reading(9);reading.stage=Stage::Caught;reading.caught=generate(100,2);reading.age=1;
    Input readInput;readInput.read=true;tick(reading,readInput);assert(reading.stage==Stage::Dossier&&!reading.dossierBook);
    float readingAge=reading.age;tick(reading,{},600);assert(reading.age==readingAge);
    for(unsigned page=1;page<=3;++page){tick(reading,{true},60);assert(reading.dossierPage==page%3);tick(reading,{});}
    tick(reading,readInput);assert(reading.stage==Stage::Caught&&reading.landed==0);
    tick(reading,{},1);reading.stage=Stage::Book;tick(reading,readInput);assert(reading.stage==Stage::Dossier&&reading.dossierBook);
    tick(reading,{});Input back;back.back=true;tick(reading,back);assert(reading.stage==Stage::Book);
    uint16_t pixels[Width*Height];Canvas canvas(pixels);std::set<std::string> mainCards;
    for(unsigned family=0;family<8+ObjectTypes;++family)for(unsigned traitsIndex=0;traitsIndex<32;++traitsIndex)for(unsigned spot=0;spot<3;++spot) {
        unsigned type=family-8;
        Catch c;c.form=family<8?family|((traitsIndex&7)<<10)|((traitsIndex>>3)<<15):ObjectFlag|(type&7)|((type&24)<<5)|((traitsIndex&3)<<6)|((traitsIndex>>2)<<3);c.spot=spot;
        mainCards.insert(dossier(c,0).title);
        for(unsigned page=0;page<3;++page) {
            const auto card=dossier(c,page);assert(canvas.textWidth(card.title)<=224);
            for(const auto* line:card.lines){assert(line&&line[0]);if(canvas.textWidth(line)>224)std::cerr<<line<<"\n";assert(canvas.textWidth(line)<=224);}
            Catch resized=c;resized.seed=919;resized.millimetres=399;
            assert(std::strcmp(card.title,dossier(resized,page).title)==0);
            for(unsigned line=0;line<6;++line)assert(std::strcmp(card.lines[line],dossier(resized,page).lines[line])==0);
        }
    }
    assert(mainCards.size()==56);
    std::cout<<"generator: 100000 samples; "<<fishN<<" fish, "<<objectN<<" objects, "<<identities.size()<<" distinct appearance IDs\n";
    std::cout<<"gameplay: no-input/held-input/early-hook/no-reel/over-tension all fail to catch; pause/help freeze verified\n";
    std::cout<<"tracking controller: "<<wins<<"/300 landed; mean fight "<<totalTime/wins<<" seconds (automated, not human playtest)\n";
    std::cout<<"assisted controller: "<<assistedWins<<"/300 landed without A/D, 200ms decisions, mean fight "<<assistedTime/assistedWins<<" seconds; held-only broke "<<heldLosses<<"/300\n";
    std::cout<<"dossiers: 48 object + 8 fish main records; 24 linked evidence pages form one connected trail; all page layouts fit; modal paging/freezing/return checked\n";
    std::cout<<"compatibility: v1 fixture exact; mixed v1/v2 append and reload; extended IDs distinct; all 768 object forms sampled; fish generation unchanged in 100000 seeds; invalid versions/bounds rejected\n";
    std::cout<<"journal: dedup excludes size; object namespace separate; restart/partial write/corruption/no SD/disk full checked\n";
}
static void renders() {
    Game g(10);ViewState v;v.save=SaveState::Ready;v.discoveries=12;v.saved=true;v.fresh=true;
    snapshot("shore",g,v);
    for(unsigned m=0;m<3;++m){g.method=Method(m);char name[40];std::snprintf(name,sizeof name,"method-%u",m);snapshot(name,g,v);}g.method=Method::Shallow;
    g.spot=2;snapshot("night",g,v);
    hook(g);g.fish=.64;g.rod=.56;g.progress=.57;g.tension=.6;snapshot("fight",g,v);
    g.surgeLeft=1;g.tension=.83;snapshot("surge",g,v);
    g.stage=Stage::Bite;snapshot("bite",g,v);
    g.stage=Stage::Caught;g.caught=generate(987,1);g.landed=3;snapshot("caught",g,v);
    g.stage=Stage::Help;snapshot("help",g,v);
    g.stage=Stage::Book;v.bookValid=true;v.bookCatch=generate(1034,0);v.bookIndex=5;snapshot("book",g,v);
    g.stage=Stage::Waiting;g.age=1;g.anomaly=Anomaly::DoubleReflection;snapshot("event-reflection",g,v,1000);
    g.stage=Stage::Caught;g.anomaly=Anomaly::FalseClock;g.caught.form=ObjectFlag+259;snapshot("event-clock",g,v);
    g.anomaly=Anomaly::FutureReport;g.caught.form=ObjectFlag+257;snapshot("event-report",g,v);g.anomaly=Anomaly::None;
    g.stage=Stage::Dossier;g.dossierBook=false;
    const unsigned loreForms[]={ObjectFlag+257, ObjectFlag+259, ObjectFlag+262, ObjectFlag+516, ObjectFlag+518, ObjectFlag+519};
    for(unsigned n=0;n<6;++n){g.caught.form=loreForms[n];g.caught.spot=n%3;g.dossierPage=0;char name[40];std::snprintf(name,sizeof name,"lore-%u",n);snapshot(name,g,v);}
    for(unsigned page=1;page<3;++page){g.dossierPage=page;char name[40];std::snprintf(name,sizeof name,"lore-extra-%u",page);snapshot(name,g,v);}
    g.caught.form=ObjectFlag+258;v.knownObjects=(1u<<10)|(1u<<16);g.dossierPage=3;syncDossierPages(g,v);snapshot("annotation-card-photo",g,v);
    g.caught.form=ObjectFlag+257;v.knownObjects=(1u<<9)|(1u<<11);syncDossierPages(g,v);snapshot("annotation-tape-clock",g,v);
    v.knownObjects=0;syncDossierPages(g,v);
    g.stage=Stage::Shore;v.save=SaveState::Missing;snapshot("no-sd",g,v);
    std::set<uint32_t> rasterHashes;
    uint16_t pixels[Width*Height];Canvas c(pixels);
    std::set<uint32_t> objectRasters;
    for(unsigned type=0;type<ObjectTypes;++type){
        Catch f;f.form=ObjectFlag|(type&7)|((type&24)<<5);
        c.clear(rgb(3,10,3));drawCatch(c,f,120,60,1);
        objectRasters.insert(crc32(reinterpret_cast<uint8_t*>(pixels),sizeof pixels));
        c.clear(rgb(3,10,3));drawCatch(c,f,120,60,2);c.center(109,objectName(type),rgb(189,255,137));
        char path[100];std::snprintf(path,sizeof path,"build/object-%02u.ppm",type);writePPM(path,pixels);
    }
    assert(objectRasters.size()==ObjectTypes);std::cout<<"objects: 24 distinct base rasters at device 1x scale\n";
    for(unsigned n=0;n<256;++n) {
        c.clear(rgb(3,10,3));Catch f=generate(mix(n+33),n%3);drawCatch(c,f,120,67,2);
        rasterHashes.insert(crc32(reinterpret_cast<uint8_t*>(pixels),sizeof pixels));
        if(n<48){char path[100];std::snprintf(path,sizeof path,"build/specimen-%02u.ppm",n);writePPM(path,pixels);}
    }
    assert(rasterHashes.size()>240);std::cout<<"renderer: "<<rasterHashes.size()<<"/256 distinct sample rasters\n";
    // Render an actual state-machine fight driven by the same controller used above.
    Game demo(1034);v.save=SaveState::Ready;v.bookValid=false;bool reel=true;
    for(unsigned n=0;n<1200;++n) {
        Input in;
        if(n==40||demo.stage==Stage::Bite)in.action=true;
        if(demo.stage==Stage::Fight){if(n%12==0){if(demo.tension>.68)reel=false;else if(demo.tension<.25)reel=true;}in.action=reel;}
        tick(demo,in);
        if(n%6==0){draw(c,demo,v,n*1000/60);char path[100];std::snprintf(path,sizeof path,"build/frame-%03u.ppm",n/6);writePPM(path,pixels);}
    }
}
void featureTests();
int main(){tests();featureTests();renders();return 0;}
