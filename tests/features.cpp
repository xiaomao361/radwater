#include "game.h"
#include "view.h"
#include "lore.h"
#include "notebook.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <set>
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
    assert(count>1500&&count<4500);
    for(unsigned type:{9u,11u})for(unsigned seed=0;seed<1000;++seed){Catch c;c.form=ObjectFlag|(type&7)|((type&24)<<5);auto e=chooseAnomaly(seed,c,1);if(e!=Anomaly::None)types|=1u<<unsigned(e);}
    assert(types==8190);
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
    a.age=30;assert(a.anomalyVisible());
    std::cout<<"events: "<<count<<"/20000 casts, all 12 kinds; 3-cast cooldown; persistent event text/pause; unchanged fight, record and save-status pixels\n";
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
    view.bookCatch=generate(1,0);view.bookCatch.form=0;g.dossierPage=3;syncDossierPages(g,view);assert(g.dossierPages==2&&g.dossierPage==0);
    std::cout<<"annotations: 12 pairs cover 24 types; either order; original pages unchanged; duplicates; v1 upgrade/reboot; failed/missing saves never unlock; 4-page reading\n";
}
static void methodTests(){
    unsigned objects[3]={},reflections[3]={};
    uint16_t pixels[Width*Height];Canvas canvas(pixels);
    for(unsigned m=0;m<3;++m){
        Method method=Method(m);assert(canvas.textWidth(methodProfile(method).hint)<=119);
        for(unsigned seed=0;seed<100000;++seed){
            Catch c=generateForMethod(seed,seed%3,method),replay=generate(c.seed,c.spot,c.generator);
            assert(c.form==replay.form&&c.millimetres==replay.millimetres&&c.generator==3);
            if(c.object())++objects[m];
            Catch fish;fish.form=0;
            if(chooseAnomaly(seed,fish,methodProfile(method).reflectionOdds)==Anomaly::DoubleReflection)++reflections[m];
        }
    }
    assert(objects[0]>5000&&objects[0]<7000&&objects[1]>28000&&objects[1]<32000&&objects[2]>10000&&objects[2]<12500);
    assert(reflections[0]==reflections[1]&&reflections[1]<reflections[2]);
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
    for(float mean:means)assert(mean>2&&mean<4.5f);
    std::cout<<"methods: object samples/100000 shallow "<<objects[0]<<", bottom "<<objects[1]<<", deep "<<objects[2]<<"; replayable v3 seeds; shore-only F edge and session retention\n";
    std::cout<<"method play: 300/300 wins and 300/300 held-only broken per mode; mean fight "<<means[0]<<" / "<<means[1]<<" / "<<means[2]<<"s; deep reflection odds higher\n";
}


static void storyTests(){
    FeatureStorage storage;Journal j;j.load(storage);
    // Different legacy appearances collapse to one species without changing source bytes.
    Catch a;a.form=0;a.millimetres=90;a.generator=1;assert(j.save(a));
    Catch b=a;b.form=7u<<10;b.generator=2;assert(j.save(b));assert(j.discoveries==1&&j.records==2);
    Catch c=a;c.form=1u<<15;assert(j.save(c));assert(j.discoveries==2);auto bytes=storage.bytes;
    Journal reboot;reboot.load(storage);assert(reboot.records==3&&reboot.discoveries==2&&storage.bytes==bytes);
    uint32_t ordinal=99;Catch found;assert(reboot.find(8,ordinal,found)&&ordinal==1&&found.form==c.form);
    std::set<unsigned> forms;uint16_t pixels[Width*Height];Canvas canvas(pixels);
    for(unsigned i=0;i<100000;++i){auto x=generate(i,i%3);auto y=generate(x.seed,x.spot,x.generator);assert(x.form==y.form&&x.millimetres==y.millimetres);if(!x.object())forms.insert(x.form);}
    assert(forms.size()==16);
    for(unsigned i=0;i<16;++i){Catch x;x.form=speciesForm(i);assert(x.species()==i);
        auto* note=fishAnnotation(x,1u<<fishEvidence(i));assert(note&&!fishAnnotation(x,0));
        for(auto line:note->lines)assert(canvas.textWidth(line)<=222);
    }
    for(unsigned i=0;i<=EventCount;++i)for(unsigned variant=0;variant<4;++variant){auto card=eventDossier(i|(variant<<8));for(auto line:card.lines){if(canvas.textWidth(line)>222)std::cerr<<line<<"\n";assert(canvas.textWidth(line)<=222);}}
    Game shortPlay(5);shortPlay.cast();while(shortPlay.stage==Stage::Waiting)shortPlay.tick(.05f,{});
    shortPlay.tick(.05f,{true});while(shortPlay.stage==Stage::Fight){Input in;in.action=!shortPlay.surging();shortPlay.tick(.05f,in);}
    assert(shortPlay.stage==Stage::Caught&&shortPlay.fightAge<3);
    Game wait(2);wait.cast();for(unsigned i=0;i<2000;++i)wait.tick(.05f,{});assert(wait.stage==Stage::Bite&&wait.paused);
    Input pause;pause.pause=true;wait.tick(.01f,pause);wait.tick(.01f,{});wait.tick(.01f,{true});assert(wait.stage==Stage::Fight);
    Game quiet(41);quiet.knownFish=65535;quiet.knownObjects=0xffffff;unsigned repeats=0;
    for(unsigned i=0;i<2000;++i){unsigned last=quiet.caught.index();quiet.cast();if(i&&quiet.caught.index()==last)++repeats;}assert(repeats<10);
    Game effect(1);effect.effect=Anomaly::Shift;effect.effectLeft=2;effect.cast();assert(effect.waitFor<1.6f&&effect.effectLeft==1);
    Input note;note.notes=true;effect.stage=Stage::Caught;effect.tick(.01f,note);assert(effect.stage==Stage::Notes);float time=effect.age;for(unsigned i=0;i<300;++i)effect.tick(.05f,{});assert(effect.age==time&&effect.effectLeft==1);effect.tick(.01f,note);assert(effect.stage==Stage::Caught);
    effect.anomaly=Anomaly::Knock;effect.eventCode=7;effect.eventPending=false;Input reply;reply.respond=true;effect.tick(.01f,reply);assert(effect.responded&&effect.eventPending&&effect.eventCode==(7|65536));
    for(auto kind:{Anomaly::FalseClock,Anomaly::FutureReport,Anomaly::Receipt}){
        Game pending;pending.stage=Stage::Waiting;pending.anomaly=kind;pending.eventCode=unsigned(kind);assert(!pending.anomalyVisible());
        pending.stage=Stage::Fight;pending.progress=.99f;pending.surgeIn=0;pending.tick(.05f,{true});assert(pending.stage==Stage::Caught&&pending.eventPending);
    }
    std::cout<<"story edition: 16 canonical species; legacy grouped without byte changes; 16 evidence-linked fish notes; event text fits; short reel <3s; unattended bite waits; repeat prevention; cast-count effects; notes pause; optional response\n";
}
static void notebookTests(){
    FeatureStorage io;Notebook n;n.load(io);assert(n.state==SaveState::Ready);ReadingState s;
    for(unsigned type=0;type<8;++type){ReadingState reading;Catch a=object(type),b=a;b.form|=1u<<6;
        reading.read(a,0,0,true);assert(!reading.unread(a,0)&&reading.unread(b,0));a.form|=1u<<3;assert(!reading.unread(a,0));}
    ReadingState reading;Catch fish;fish.form=speciesForm(0);reading.read(fish,0,1u<<10,true);assert(reading.entryPage(fish,1u<<10)==1);reading.read(fish,1,1u<<10,true);assert(!reading.unread(fish,1u<<10));
    s.legacyRead=0x80000001;
    s.bookmark=8;s.page=1;s.fishRead=256;s.addEvent(6|256);assert(n.save(s));auto original=io.bytes;
    Notebook reboot;reboot.load(io);assert(reboot.data.legacyRead==0x80000001&&reboot.data.bookmark==8&&reboot.data.page==1&&reboot.data.events[0]==262&&io.bytes==original);
    for(unsigned i=1;i<=12;++i)s.addEvent(i);assert(n.save(s));assert(n.data.count==12&&n.data.events[0]==12&&n.data.events[11]==1);
    s.addEvent(7);s.addEvent(7|65536);assert(s.events[0]==65543&&s.events[1]==12);
    io.partial=true;assert(!n.save(s)&&n.state==SaveState::WriteFailed&&n.data.events[0]==12);auto damaged=io.bytes;
    Notebook prefix;prefix.load(io);assert(prefix.state==SaveState::Corrupt&&prefix.data.events[0]==12&&io.bytes==damaged);assert(!prefix.save(s)&&io.bytes==damaged);
    FeatureStorage missing;missing.present=false;Notebook absent;absent.load(missing);assert(absent.state==SaveState::Missing&&!absent.save(s));
    FeatureStorage invalid;Notebook bad;bad.load(invalid);s.bookmark=FormCount+1;assert(!bad.save(s)&&bad.state==SaveState::WriteFailed&&invalid.bytes.empty());
    std::cout<<"notebook: append/readback/reboot bookmark and history; last 12 events; response update; partial-write prefix read-only; missing SD and invalid fields cannot report saved\n";
}
void featureTests(){eventTests();annotationTests();methodTests();storyTests();notebookTests();}
