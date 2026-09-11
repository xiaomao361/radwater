#pragma once
#include <cstdint>
#include <cstddef>

namespace pond {
constexpr uint32_t FishForms = 1u << 17;
constexpr uint32_t LegacyObjectForms = 256;
constexpr uint32_t ObjectTypes = 24;
constexpr uint32_t ObjectForms = ObjectTypes * 32;
constexpr uint32_t ObjectFlag = 1u << 20;
constexpr unsigned FishSpecies = 16;
constexpr uint32_t FormCount = FishSpecies + ObjectForms;
uint32_t speciesForm(unsigned species);
const char* speciesName(unsigned species);
constexpr unsigned RecordBytes = 32;
constexpr unsigned GeneratorVersion = 3;
float clamp(float x, float lo, float hi);
uint32_t mix(uint32_t x);
struct Random {
    uint32_t state;
    explicit Random(uint32_t seed) : state(seed ? seed : 0x59a1b7u) {}
    uint32_t next();
    uint32_t below(uint32_t n);
};
struct Catch {
    uint32_t form = 0, seed = 0, millimetres = 0, spot = 0;
    uint32_t generator = GeneratorVersion;
    bool object() const { return (form & ObjectFlag) != 0; }
    uint32_t index() const { return object() ? FishSpecies + (form & (ObjectFlag - 1)) : (form < FishForms ? species() : FormCount); }
    unsigned objectType() const { return (form & 7) | ((form >> 5) & 24); }
    unsigned species() const { return (form & 7) | ((form >> 12) & 8); }
    unsigned body() const { return form & 7; }
    unsigned tail() const { return (form >> 3) & 3; }
    unsigned fin() const { return (form >> 5) & 3; }
    unsigned palette() const { return (form >> 7) & 7; }
    unsigned pattern() const { return (form >> 10) & 7; }
    unsigned face() const { return (form >> 13) & 3; }
    unsigned ornament() const { return (form >> 15) & 3; }
    unsigned behavior() const { return mix(form ^ 0xc0ffeeu) & 3; }
    unsigned rarity() const;
};
Catch generate(uint32_t seed, unsigned spot, unsigned version = GeneratorVersion);
enum class Method { Shallow, Bottom, Deep };
struct MethodProfile {
    const char* name;
    const char* hint;
    float waitBase,waitSpan,fishSpeed,reelRate;
    unsigned reflectionOdds;
};
const MethodProfile& methodProfile(Method method);
Catch generateForMethod(uint32_t seed,unsigned spot,Method method);
const char* bodyName(unsigned i);
const char* colorName(unsigned i);
const char* patternName(unsigned i);
const char* objectName(unsigned i);
const char* behaviorName(unsigned i);
const char* spotName(unsigned i);
void catchName(const Catch& c, char* out, size_t cap);
void encode(const Catch& c, uint32_t sequence, uint8_t out[RecordBytes]);
bool decode(const uint8_t in[RecordBytes], uint32_t expectedSequence, Catch& c);
uint32_t crc32(const uint8_t* bytes, size_t n);

enum class Stage { Shore, Waiting, Bite, Fight, Caught, Lost, Book, Help, Dossier, Notes };
enum class Loss { Early, Late, Broken, Escaped, Released };
enum class Anomaly { None, DoubleReflection, FalseClock, FutureReport, Drain, Shift, Broadcast, Knock, Receipt, Wind, Lamp, Rain, Bell };
constexpr unsigned EventCount = 12;
// Event selection has its own RNG; it never changes catch generation or fight RNG.
Anomaly chooseAnomaly(uint32_t seed, const Catch& caught, unsigned reflectionOdds = 40);
struct Input {
    bool action = false, left = false, right = false;
    bool book = false, back = false, help = false, pause = false;
    int spot = -1;
    bool read = false;
    bool method = false, notes = false, respond = false;
    bool activity = false; // Device-only keys (resume, sound) also interrupt the entrance.
};
class Game {
public:
    explicit Game(uint32_t seed = 1) : random(seed) {}
    Stage stage = Stage::Shore;
    Loss loss = Loss::Early;
    Catch caught;
    unsigned spot = 0;
    Method method = Method::Shallow;
    bool paused = false, newCatch = false;
    float age = 0, waitFor = 0, biteWindow = 0, fightAge = 0;
    float arrivalAge = 0, shoreIdle = 0;
    bool arrivalGreeting = true;
    float fish = 0.5f, rod = 0.5f, progress = 0.18f, tension = 0.12f;
    float target = 0.5f, turnIn = 0, surgeIn = 0, surgeLeft = 0;
    unsigned landed = 0;
    unsigned dossierPage = 0;
    unsigned dossierPages = 3;
    bool dossierBook = false;
    Anomaly anomaly = Anomaly::None;
    uint32_t knownFish = 0, knownObjects = 0;
    unsigned quietCasts = 0, recent[3] = {FormCount,FormCount,FormCount};
    unsigned effectLeft = 0, notePage = 0;
    Anomaly effect = Anomaly::None;
    bool eventPending = false, responded = false;
    uint32_t eventCode = 0;
    bool catchEvent() const { return anomaly==Anomaly::FalseClock||anomaly==Anomaly::FutureReport||anomaly==Anomaly::Receipt; }
    bool anomalyVisible() const;
    bool surging() const { return surgeLeft > 0; }
    float zone() const { return caught.object() ? 0.22f : 0.18f; }
    bool aligned() const;
    bool nibble() const;
    void tick(float seconds, const Input& input);
    void cast();
private:
    Random random;
    Input previous;
    Stage beforeHelp = Stage::Shore, beforeNotes = Stage::Shore;
    unsigned anomalyCooldown = 0;
    void lose(Loss why);
};
}
