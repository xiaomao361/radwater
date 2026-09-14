#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>
#include "game.h"
#include "journal.h"
#include "view.h"
#include "lore.h"
#include "sound.h"

using namespace pond;
namespace {
constexpr const char* Directory = "/PocketFishing";

class SDStorage : public Storage {
    bool mounted = false;
    File reader;
    const char* path;
public:
    explicit SDStorage(const char* p):path(p){}
    void attach(bool available){mounted=available;}
    bool available()const{return mounted;}
    void begin() {
        // ADV and original Cardputer share SD SPI pins; keyboard is handled by M5Cardputer.
        SPI.begin(40, 39, 14, 12);
        mounted = SD.begin(12, SPI, 20000000);
        if (!mounted) return;
        if (!SD.exists(Directory) && !SD.mkdir(Directory)) { mounted = false; return; }
    }
    bool size(uint32_t& bytes) override {
        if (!mounted) return false;
        if (reader) reader.close();
        if (!SD.exists(path)) { bytes = 0; return true; }
        reader = SD.open(path, FILE_READ);
        if (!reader || reader.isDirectory() || reader.size() > UINT32_MAX) return false;
        bytes = uint32_t(reader.size()); return true;
    }
    bool read(uint32_t offset, uint8_t* out, size_t count) override {
        if (!mounted) return false;
        if (!reader) reader = SD.open(path, FILE_READ);
        return reader && reader.seek(offset) && reader.read(out, count) == int(count);
    }
    bool append(const uint8_t* bytes, size_t count) override {
        if (!mounted) return false;
        if (reader) reader.close();
        auto file = SD.open(path, FILE_APPEND);
        if (!file) return false;
        size_t n = file.write(bytes, count); file.flush(); file.close();
        // Journal performs a reopen + length + byte-for-byte readback before reporting saved.
        return n == count;
    }
} storage("/PocketFishing/catches-v1.pfj"), noteStorage("/PocketFishing/reading-v1.pfn");
Notebook notebook;
Game game;
Journal journal;
ViewState view;
uint16_t framebuffer[Width * Height];
Canvas canvas(framebuffer);
uint32_t lastTick = 0, lastFrame = 0, lastInput = 0;
bool oldLeft = false, oldRight = false, oldMute = false;
bool dim = false;
Stage oldStage = Stage::Shore;
int8_t sounds[4][SoundSamples];
void playSound(Sound sound){
    if(view.sound&&sound!=Sound::None)
        M5Cardputer.Speaker.playRaw(sounds[unsigned(sound)],SoundSamples,SoundRate,false,1,0,true);
}
bool unread(const Catch& c){return view.reading.unread(c,view.knownObjects);}
unsigned relatedType(const Catch& c){return c.object()?evidenceNext(c.objectType()):fishEvidence(c.species());}
void loadBook() {
    view.bookValid=journal.discoveries&&journal.discovery(view.bookIndex,view.bookCatch);
    view.save=journal.state;view.unread=view.bookValid&&unread(view.bookCatch);
}
void persistReading(){notebook.save(view.reading);view.notebookSave=notebook.state;}
void refreshView(){
    view.save=journal.state;view.discoveries=journal.discoveries;view.savedCatches=journal.records;
    view.knownObjects=journal.knownObjectTypes();view.knownFish=journal.knownFishSpecies();
    game.knownObjects=view.knownObjects;game.knownFish=view.knownFish;
}
void selectUnread(){
    // Newly available annotations first, then unseen main entries. Bounded by the small catalogue.
    for(unsigned pass=0;pass<2;++pass)for(unsigned n=0;n<journal.discoveries;++n){
        Catch c;if(!journal.discovery(n,c)){refreshView();return;}
        bool note=view.reading.noteUnread(c,view.knownObjects);
        if((pass==0?note:unread(c))){view.bookIndex=n;loadBook();return;}
    }
    loadBook();
}
void markRead(){
    const auto& c=game.dossierBook?view.bookCatch:game.caught;
    if(game.dossierBook&&!view.bookValid)return;
    view.reading.read(c,game.dossierPage,view.knownObjects,journal.known(c));persistReading();
}

}
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(100);
    M5Cardputer.Display.setSwapBytes(true);
    M5Cardputer.Speaker.setVolume(55);
    for(unsigned i=0;i<4;++i)synthSound(Sound(i),sounds[i]);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.println("Radwater / loading journal...");
    storage.begin();noteStorage.attach(storage.available());journal.load(storage);notebook.load(noteStorage);
    view.reading=notebook.data;view.notebookSave=notebook.state;game=Game(esp_random());refreshView();
    lastTick = lastFrame = lastInput = millis();
}
void loop() {
    M5Cardputer.update();
    const uint32_t now = millis();
    const auto& keys = M5Cardputer.Keyboard.keysState();
    auto key = [](char k) { return M5Cardputer.Keyboard.isKeyPressed(k); };
    Input in;
    in.action = keys.space || keys.enter;
    in.left = key('a') || key(','); in.right = key('d') || key('/');
    in.book = key('b'); in.back = keys.del || key('`');
    in.help = key('h'); in.pause = key('p'); in.read = key('r'); in.method = key('f'); in.notes=key('n');in.respond=key('e');
    // An empty or unreadable catalogue has no specimen dossier to open.
    if(game.stage==Stage::Book&&!view.bookValid)in.read=false;
    if(key('1'))in.spot=0;else if(key('2'))in.spot=1;else if(key('3'))in.spot=2;
    bool mute = key('m');
    if(mute&&!oldMute) { view.sound=!view.sound; if(view.sound)playSound(Sound::Splash);else M5Cardputer.Speaker.stop(); }
    oldMute=mute;
    bool active = in.action || in.left || in.right || in.book || in.back || in.help || in.pause || in.read || in.method || in.notes || in.respond || key('u') || key('t') || key('c') || mute || in.spot>=0;
    if(active) lastInput=now;
    in.activity=active;
    bool shouldDim=(now-lastInput>60000u)&&(game.stage==Stage::Shore||game.stage==Stage::Book||game.stage==Stage::Dossier||game.stage==Stage::Notes||game.stage==Stage::Caught||game.paused);
    if(shouldDim!=dim){dim=shouldDim;M5Cardputer.Display.setBrightness(dim?20:100);}
    static bool oldUnread=false,oldLink=false,oldResume=false;
    bool u=key('u'),link=key('t'),resume=key('c');
    bool jumped=false;
    if(u&&!oldUnread&&game.stage==Stage::Book)selectUnread();
    if(resume&&!oldResume&&(game.stage==Stage::Shore||game.stage==Stage::Book)&&view.reading.bookmark<FormCount){
        if(journal.find(view.reading.bookmark,view.bookIndex,view.bookCatch)){
            view.bookValid=true;game.stage=Stage::Dossier;game.dossierBook=true;game.dossierPage=view.reading.page;jumped=true;
        }else refreshView();
    }
    if(link&&!oldLink&&(game.stage==Stage::Dossier||game.stage==Stage::Book||game.stage==Stage::Caught)){
        Catch from=game.stage==Stage::Book||(game.stage==Stage::Dossier&&game.dossierBook)?view.bookCatch:game.caught;
        for(unsigned n=0;n<journal.discoveries;++n){Catch c;if(!journal.discovery(n,c)){refreshView();break;}
            if(c.object()&&c.objectType()==relatedType(from)){view.bookIndex=n;view.bookCatch=c;view.bookValid=true;game.stage=Stage::Dossier;game.dossierBook=true;game.dossierPage=0;jumped=true;break;}}
    }
    oldUnread=u;oldLink=link;oldResume=resume;
    if(game.stage==Stage::Book) {
        bool moved=false;
        if(in.left&&!oldLeft&&view.bookIndex>0){--view.bookIndex;moved=true;}
        if(in.right&&!oldRight&&view.bookIndex+1<journal.discoveries){++view.bookIndex;moved=true;}
        if(moved)loadBook();
    }
    oldLeft=in.left;oldRight=in.right;
    syncDossierPages(game,view);
    const Stage beforeTick=game.stage;const unsigned beforePage=game.dossierPage;
    game.tick((now-lastTick)/1000.0f,in);lastTick=now;
    if(game.stage==Stage::Dossier&&beforeTick!=Stage::Dossier&&!jumped){
        const auto& c=game.dossierBook?view.bookCatch:game.caught;
        game.dossierPage=view.reading.entryPage(c,view.knownObjects);
    }
    if(game.stage==Stage::Dossier&&(jumped||beforeTick!=Stage::Dossier||beforePage!=game.dossierPage))markRead();
    if(game.eventPending){view.reading.addEvent(game.eventCode);persistReading();game.eventPending=false;}
    if(game.newCatch) {
        const uint32_t before=unlockedAnnotations(journal.knownObjectTypes());
        view.fresh=!journal.known(game.caught);
        view.saved=journal.save(game.caught);
        game.newCatch=false;refreshView();
        view.newAnnotations=unlockedAnnotations(view.knownObjects)&~before;
    }
    if(game.stage!=oldStage) {
        if(game.stage==Stage::Book&&oldStage!=Stage::Dossier){view.bookIndex=journal.discoveries?journal.discoveries-1:0;selectUnread();}
        playSound(soundFor(oldStage,game.stage,game.caught.object()));
        oldStage=game.stage;
    }
    const Catch& shown=game.stage==Stage::Book||(game.stage==Stage::Dossier&&game.dossierBook)?view.bookCatch:game.caught;
    view.linkAvailable=bool(view.knownObjects&(1u<<relatedType(shown)));view.unread=unread(shown);
    if(now-lastFrame>=33u) {
        draw(canvas,game,view,now);
        M5Cardputer.Display.pushImage(0,0,Width,Height,framebuffer);
        lastFrame=now;
    }
    delay(5);
}
