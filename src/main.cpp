#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>
#include "game.h"
#include "journal.h"
#include "view.h"
#include "lore.h"

using namespace pond;
namespace {
constexpr const char* Directory = "/PocketFishing";
constexpr const char* SaveFile = "/PocketFishing/catches-v1.pfj";
class SDStorage : public Storage {
    bool mounted = false;
    File reader;
public:
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
        if (!SD.exists(SaveFile)) { bytes = 0; return true; }
        reader = SD.open(SaveFile, FILE_READ);
        if (!reader || reader.isDirectory() || reader.size() > UINT32_MAX) return false;
        bytes = uint32_t(reader.size()); return true;
    }
    bool read(uint32_t offset, uint8_t* out, size_t count) override {
        if (!mounted) return false;
        if (!reader) reader = SD.open(SaveFile, FILE_READ);
        return reader && reader.seek(offset) && reader.read(out, count) == int(count);
    }
    bool append(const uint8_t* bytes, size_t count) override {
        if (!mounted) return false;
        if (reader) reader.close();
        auto file = SD.open(SaveFile, FILE_APPEND);
        if (!file) return false;
        size_t n = file.write(bytes, count); file.flush(); file.close();
        // Journal performs a reopen + length + byte-for-byte readback before reporting saved.
        return n == count;
    }
} storage;
Game game;
Journal journal;
ViewState view;
uint16_t framebuffer[Width * Height];
Canvas canvas(framebuffer);
uint32_t lastTick = 0, lastFrame = 0, lastInput = 0;
bool oldLeft = false, oldRight = false, oldMute = false;
bool dim = false;
Stage oldStage = Stage::Shore;
void loadBook() {
    view.bookValid = journal.discoveries && journal.discovery(view.bookIndex, view.bookCatch);
    view.save = journal.state;
}
void refreshView() {
    view.save = journal.state; view.discoveries = journal.discoveries; view.savedCatches = journal.records;
    view.knownObjects = journal.knownObjectTypes();
}
}
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(100);
    M5Cardputer.Display.setSwapBytes(true);
    M5Cardputer.Speaker.setVolume(55);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.println("Pocket Fishing / loading journal...");
    storage.begin(); journal.load(storage); refreshView();
    game = Game(esp_random());
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
    in.help = key('h'); in.pause = key('p'); in.read = key('r'); in.method = key('f');
    // An empty or unreadable catalogue has no specimen dossier to open.
    if(game.stage==Stage::Book&&!view.bookValid)in.read=false;
    if(key('1'))in.spot=0;else if(key('2'))in.spot=1;else if(key('3'))in.spot=2;
    bool mute = key('m');
    if(mute&&!oldMute) { view.sound=!view.sound; if(view.sound) M5Cardputer.Speaker.tone(660,60); }
    oldMute=mute;
    bool active = in.action || in.left || in.right || in.book || in.back || in.help || in.pause || in.read || in.method || mute || in.spot>=0;
    if(active) lastInput=now;
    bool shouldDim=(now-lastInput>60000u)&&(game.stage==Stage::Shore||game.stage==Stage::Book||game.stage==Stage::Dossier||game.paused);
    if(shouldDim!=dim){dim=shouldDim;M5Cardputer.Display.setBrightness(dim?20:100);}
    if(game.stage==Stage::Book) {
        bool moved=false;
        if(in.left&&!oldLeft&&view.bookIndex>0){--view.bookIndex;moved=true;}
        if(in.right&&!oldRight&&view.bookIndex+1<journal.discoveries){++view.bookIndex;moved=true;}
        if(moved)loadBook();
    }
    oldLeft=in.left;oldRight=in.right;
    syncDossierPages(game,view);
    game.tick((now-lastTick)/1000.0f,in);lastTick=now;
    if(game.newCatch) {
        const uint32_t before=unlockedAnnotations(journal.knownObjectTypes());
        view.fresh=!journal.known(game.caught);
        view.saved=journal.save(game.caught);
        game.newCatch=false;refreshView();
        view.newAnnotations=unlockedAnnotations(view.knownObjects)&~before;
    }
    if(game.stage!=oldStage) {
        if(game.stage==Stage::Book&&oldStage!=Stage::Dossier){view.bookIndex=journal.discoveries?journal.discoveries-1:0;loadBook();}
        if(view.sound) {
            if(game.stage==Stage::Bite)M5Cardputer.Speaker.tone(880,90);
            if(game.stage==Stage::Caught&&oldStage!=Stage::Dossier)M5Cardputer.Speaker.tone(1175,100);
        }
        oldStage=game.stage;
    }
    if(now-lastFrame>=33u) {
        draw(canvas,game,view,now);
        M5Cardputer.Display.pushImage(0,0,Width,Height,framebuffer);
        lastFrame=now;
    }
    delay(5);
}
