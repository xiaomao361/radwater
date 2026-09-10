#include "game.h"
#include "view.h"
#include "lore.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
using namespace pond;

static void eventTests() {
    Game sampler(8181);unsigned count=0,last=0,types=0;
    for(unsigned i=0;i<20000;++i){
        sampler.cast();
        if(sampler.anomaly!=Anomaly::None){
            if(count)assert(i-last>=4);
            last=i;++count;types|=1u<<unsigned(sampler.anomaly);
        }
    }
    assert(count>200&&count<1200&&types==14);
    Game a(18),b(18);a.cast();b.cast();
    for(unsigned i=0;i<3000;++i){
        a.anomaly=Anomaly::FalseClock;b.anomaly=Anomaly::None;
        Input in;in.action=a.stage==Stage::Bite||(a.stage==Stage::Fight&&a.tension<.65f&&!a.surging());
        a.tick(1.0f/60,in);b.tick(1.0f/60,in);
        assert(a.stage==b.stage&&a.fish==b.fish&&a.rod==b.rod&&a.tension==b.tension&&a.progress==b.progress);
    }
    assert(a.landed==1&&b.landed==1);
    uint8_t ra[RecordBytes],rb[RecordBytes];encode(a.caught,1,ra);encode(b.caught,1,rb);assert(!std::memcmp(ra,rb,RecordBytes));
    a.stage=Stage::Waiting;a.age=1;a.anomaly=Anomaly::DoubleReflection;assert(a.anomalyVisible());
    Input pause;pause.pause=true;a.tick(.01f,pause);float age=a.age;a.tick(.05f,{});assert(a.age==age&&a.anomalyVisible());
    a.stage=Stage::Bite;assert(!a.anomalyVisible());a.stage=Stage::Fight;assert(!a.anomalyVisible());
    uint16_t normal[Width*Height],odd[Width*Height];Canvas n(normal),o(odd);ViewState v;
    for(auto save:{SaveState::Ready,SaveState::Missing,SaveState::Corrupt,SaveState::WriteFailed}){
        v.save=save;v.saved=save==SaveState::Ready;a.stage=Stage::Caught;a.age=1;
        a.anomaly=Anomaly::None;draw(n,a,v,1000);
        for(auto anomaly:{Anomaly::FalseClock,Anomaly::FutureReport}){
            a.anomaly=anomaly;draw(o,a,v,1000);
            assert(!std::memcmp(normal+119*Width,odd+119*Width,16*Width*sizeof(uint16_t)));
        }
    }
    a.age=3;assert(!a.anomalyVisible());
    std::cout<<"events: "<<count<<"/20000 casts, all 3 kinds; 3-cast cooldown; timers/pause; unchanged fight, record and save-status pixels\n";
}

struct FeatureStorage:Storage {
    std::vector<uint8_t> bytes;
    bool present=true,partial=false,fail=false;
    bool size(uint32_t& n)override{n=uint32_t(bytes.size());return present;}
    bool read(uint32_t off,uint8_t* out,size_t n)override{if(!present||uint64_t(off)+n>bytes.size())return false;std::memcpy(out,bytes.data()+off,n);return true;}
    bool append(const uint8_t* in,size_t n)override{if(fail||!present)return false;bytes.insert(bytes.end(),in,in+(partial?n/2:n));return !partial;}
};
static Catch object(unsigned type){Catch c;c.form=ObjectFlag|(type&7)|((type&24)<<5);c.millimetres=100;return c;}
static void annotationTests(){
    const uint32_t all=(1u<<ObjectTypes)-1;
    assert(unlockedAnnotations(all)==(1u<<AnnotationCount)-1&&unlockedAnnotations(0)==0);
    uint16_t pixels[Width*Height];Canvas canvas(pixels);
    for(unsigned type=0;type<ObjectTypes;++type){
        Catch a=object(type);assert(!annotationFor(a,1u<<type));unsigned partner=ObjectTypes,matches=0;
        for(unsigned other=0;other<ObjectTypes;++other)if(other!=type&&annotationFor(a,(1u<<type)|(1u<<other))){partner=other;++matches;}
        assert(matches==1);Catch b=object(partner);
        FeatureStorage storage;Journal j;j.load(storage);
        LorePage original[3]={dossier(a,0),dossier(a,1),dossier(a,2)};
        assert(j.save(a)&&!annotationFor(a,j.knownObjectTypes()));
        assert(j.save(b));const auto* note=annotationFor(a,j.knownObjectTypes());assert(note&&note==annotationFor(b,j.knownObjectTypes()));
        assert(canvas.textWidth(note->title)<=224);
        for(auto line:note->lines){if(canvas.textWidth(line)>224)std::cerr<<line<<'\n';assert(canvas.textWidth(line)<=224);}
        for(unsigned p=0;p<3;++p){auto after=dossier(a,p,j.knownObjectTypes());assert(!std::strcmp(after.title,original[p].title));for(unsigned l=0;l<6;++l)assert(!std::strcmp(after.lines[l],original[p].lines[l]));}
        auto bytes=storage.bytes;Journal reboot;reboot.load(storage);assert(annotationFor(a,reboot.knownObjectTypes())==note&&storage.bytes==bytes);
        auto unlocks=unlockedAnnotations(reboot.knownObjectTypes());a.form|=1u<<3;assert(reboot.save(a)&&unlockedAnnotations(reboot.knownObjectTypes())==unlocks);
    }
    for(bool partial:{false,true}){
        FeatureStorage storage;Journal j;j.load(storage);assert(j.save(object(10)));storage.partial=partial;storage.fail=!partial;
        assert(!j.save(object(16)));assert(j.knownObjectTypes()==(1u<<10)&&unlockedAnnotations(j.knownObjectTypes())==0);
        Journal reboot;reboot.load(storage);assert(reboot.knownObjectTypes()==(1u<<10));
        assert(reboot.state==(partial?SaveState::Corrupt:SaveState::Ready));
    }
    FeatureStorage missing;missing.present=false;Journal absent;absent.load(missing);assert(!absent.save(object(10))&&absent.knownObjectTypes()==0);
    FeatureStorage old;Journal j;j.load(old);auto a=object(2),b=object(6);a.generator=b.generator=1;
    assert(j.save(a)&&j.save(b));auto bytes=old.bytes;Journal upgrade;upgrade.load(old);assert(annotationFor(a,upgrade.knownObjectTypes())&&old.bytes==bytes);
    ViewState view;view.knownObjects=all;view.bookValid=true;view.bookCatch=object(10);view.bookIndex=9;
    Game g;g.stage=Stage::Book;syncDossierPages(g,view);assert(g.dossierPages==4);Input read;read.read=true;g.tick(.01f,read);
    for(unsigned p=1;p<=4;++p){g.tick(.01f,{});g.tick(.01f,{true});assert(g.dossierPage==p%4);}
    g.tick(.01f,{});g.tick(.01f,read);assert(g.stage==Stage::Book&&view.bookIndex==9);
    view.bookCatch=generate(1,0);view.bookCatch.form=0;g.dossierPage=3;syncDossierPages(g,view);assert(g.dossierPages==3&&g.dossierPage==0);
    std::cout<<"annotations: 12 pairs cover 24 types; either order; original pages unchanged; duplicates; v1 upgrade/reboot; failed/missing saves never unlock; 4-page reading\n";
}
static void methodTests(){
    unsigned objects[3]={},reflections[3]={};
    uint16_t pixels[Width*Height];Canvas canvas(pixels);
    for(unsigned m=0;m<3;++m){
        Method method=Method(m);assert(canvas.textWidth(methodProfile(method).hint)<=119);
        for(unsigned seed=0;seed<100000;++seed){
            Catch c=generateForMethod(seed,seed%3,method),replay=generate(c.seed,c.spot,c.generator);
            assert(c.form==replay.form&&c.millimetres==replay.millimetres&&c.generator==2);
            if(c.object())++objects[m];
            Catch fish;fish.form=0;
            if(chooseAnomaly(seed,fish,methodProfile(method).reflectionOdds)==Anomaly::DoubleReflection)++reflections[m];
        }
    }
    assert(objects[0]>5000&&objects[0]<7000&&objects[1]>28000&&objects[1]<32000&&objects[2]>10000&&objects[2]<12500);
    assert(reflections[0]<reflections[1]&&reflections[1]<reflections[2]);
    Game selection;Input f;f.method=true;selection.tick(.01f,f);assert(selection.method==Method::Bottom);
    for(unsigned i=0;i<100;++i)selection.tick(.01f,f);assert(selection.method==Method::Bottom);
    selection.tick(.01f,{});selection.tick(.01f,f);assert(selection.method==Method::Deep);
    selection.tick(.01f,{});selection.tick(.01f,{true});assert(selection.stage==Stage::Waiting&&selection.method==Method::Deep);
    selection.tick(.01f,f);assert(selection.method==Method::Deep);
    Input h;h.help=true;selection.tick(.01f,h);selection.tick(.01f,f);assert(selection.method==Method::Deep&&selection.stage==Stage::Help);
    selection.stage=Stage::Caught;selection.tick(.01f,{});selection.tick(.01f,f);assert(selection.method==Method::Shallow&&selection.stage==Stage::Shore);
    float means[3]={};
    for(unsigned m=0;m<3;++m){
        unsigned wins=0,heldBroken=0;
        for(unsigned seed=1;seed<=300;++seed){
            Game g(seed);g.method=Method(m);g.cast();
            while(g.stage==Stage::Waiting)g.tick(1.0f/60,{});
            g.tick(1.0f/60,{true});assert(g.stage==Stage::Fight);
            Game held=g;bool reel=true;
            for(unsigned n=0;n<3000&&g.stage==Stage::Fight;++n){
                if(n%12==0){if(g.tension>.68f)reel=false;else if(g.tension<.25f)reel=true;}
                Input in;in.action=reel;g.tick(1.0f/60,in);
            }
            if(g.stage==Stage::Caught){++wins;means[m]+=g.fightAge;}
            for(unsigned n=0;n<3000&&held.stage==Stage::Fight;++n)held.tick(1.0f/60,{true});
            if(held.stage==Stage::Lost&&held.loss==Loss::Broken)++heldBroken;
        }
        assert(wins==300&&heldBroken==300);means[m]/=wins;
    }
    assert(means[2]>means[0]+1.0f);
    std::cout<<"methods: object samples/100000 shallow "<<objects[0]<<", bottom "<<objects[1]<<", deep "<<objects[2]<<"; replayable v2 seeds; shore-only F edge and session retention\n";
    std::cout<<"method play: 300/300 wins and 300/300 held-only broken per mode; mean fight "<<means[0]<<" / "<<means[1]<<" / "<<means[2]<<"s; deep reflection odds higher\n";
}
void featureTests(){eventTests();annotationTests();methodTests();}
