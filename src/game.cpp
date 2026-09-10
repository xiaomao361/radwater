#include "game.h"
#include <cmath>
#include <cstdio>
namespace pond {
float clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
uint32_t mix(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    x *= 0x846ca68bu; return x ^ (x >> 16);
}
uint32_t Random::next() { state += 0x9e3779b9u; return mix(state); }
uint32_t Random::below(uint32_t n) {
    if (!n) return 0;
    const uint32_t cutoff = uint32_t(-n) % n;
    uint32_t r; do { r = next(); } while (r < cutoff);
    return r % n;
}
unsigned Catch::rarity() const {
    if (object()) return ((form >> 6) & 3) == 3 ? 2 : 0;
    return ornament() == 3 ? 2 : (palette() >= 6 || ornament() != 0 ? 1 : 0);
}
Catch generate(uint32_t seed, unsigned spot, unsigned version) {
    Random r(seed); Catch c; c.seed = seed; c.spot = spot % 3; c.generator = version;
    if (r.below(9) == 0) {
        c.form = ObjectFlag | r.below(version == 1 ? LegacyObjectForms : ObjectForms);
        c.millimetres = 35 + r.below(150);
        return c;
    }
    // Fixed generator v1. The traits have visible drawing rules; size is NOT identity.
    unsigned body = r.below(8), tail = r.below(4), fin = r.below(4);
    unsigned palette = r.below(8), pattern = r.below(8), face = r.below(4);
    unsigned roll = r.below(100), ornament = roll < 65 ? 0 : roll < 85 ? 1 : roll < 97 ? 2 : 3;
    // Habitats bias appearances, while retaining access to the complete form space.
    if (r.below(3) == 0) palette = (c.spot * 2 + r.below(2)) % 8;
    c.form = body | tail << 3 | fin << 5 | palette << 7 | pattern << 10 | face << 13 | ornament << 15;
    const unsigned base[] = {160, 100, 240, 120, 260, 170, 130, 200};
    c.millimetres = base[body] / 2 + r.below(base[body] * 2);
    return c;
}
const char* bodyName(unsigned i) {
    static const char* n[] = {"溪鱼", "团鱼", "长尾鱼", "河豚", "带鱼", "角鱼", "扁鱼", "灯鱼"}; return n[i & 7];
}
const MethodProfile& methodProfile(Method method) {
    static const MethodProfile profiles[] = {
        {"浅水","F 浅水：多鱼",1.8f,2.6f,.85f,.14f,48},
        {"贴底","F 贴底：旧物",2.6f,2.8f,.95f,.13f,36},
        {"深水","F 深水：异常",3.0f,3.0f,1.0f,.11f,16}
    };
    return profiles[unsigned(method)];
}
Catch generateForMethod(uint32_t seed,unsigned spot,Method method) {
    Catch first=generate(seed,spot);
    Random choice(seed^0xa65d217bu);
    // Select whole v2 candidates, retaining the selected seed and exact identity.
    // No new record format: replaying generate(c.seed,c.spot,c.generator) still reproduces it.
    if(method==Method::Shallow&&first.object()&&choice.below(2)==0)return generate(choice.next(),spot);
    if(method==Method::Bottom&&!first.object()){
        for(unsigned attempt=0;attempt<2;++attempt){Catch next=generate(choice.next(),spot);if(next.object())return next;}
    }
    return first;
}
const char* colorName(unsigned i) {
    static const char* n[] = {"薄荷", "珊瑚", "金砂", "靛蓝", "紫雾", "青瓷", "月白", "墨玉"}; return n[i & 7];
}
const char* patternName(unsigned i) {
    static const char* n[] = {"素色", "星点", "横纹", "竖纹", "棋格", "环纹", "斑块", "鳞光"}; return n[i & 7];
}
const char* objectName(unsigned i) {
    static const char* n[] = {"旧靴子", "玻璃瓶", "遗落钥匙", "小茶壶", "海螺", "生锈齿轮", "漂流信", "玩具火箭",
        "瓶盖", "录音带", "工牌", "怀表", "听筒", "手镜", "手铃", "骰子",
        "照片", "印章", "药盒", "罗盘", "面罩", "收音机", "量尺", "铅封"};
    return i < ObjectTypes ? n[i] : "未知物品";
}
const char* behaviorName(unsigned i) {
    static const char* n[] = {"缓游", "急转", "摆尾", "冲刺"}; return n[i & 3];
}
const char* spotName(unsigned i) {
    static const char* n[] = {"苇岸", "暮湾", "星潭"}; return n[i % 3];
}
void catchName(const Catch& c, char* out, size_t cap) {
    if (c.object()) {
        static const char* ages[] = {"寻常", "苔生", "刻纹", "星尘"};
        std::snprintf(out, cap, "%s%s", ages[(c.form >> 6) & 3], objectName(c.objectType()));
    } else std::snprintf(out, cap, "%s%s", colorName(c.palette()), bodyName(c.body()));
}
static void put32(uint8_t* p, uint32_t x) { for (int i = 0; i < 4; ++i) p[i] = uint8_t(x >> (i * 8)); }
static uint32_t get32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint32_t crc32(const uint8_t* p, size_t n) {
    uint32_t crc = ~0u;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i]; for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
void encode(const Catch& c, uint32_t seq, uint8_t out[RecordBytes]) {
    put32(out, 0x314a4650u); put32(out + 4, seq); put32(out + 8, c.form);
    put32(out + 12, c.seed); put32(out + 16, c.millimetres); put32(out + 20, c.spot);
    put32(out + 24, c.generator); put32(out + 28, crc32(out, 28));
}
bool decode(const uint8_t in[RecordBytes], uint32_t seq, Catch& c) {
    if (!seq || get32(in) != 0x314a4650u || get32(in + 4) != seq ||
        (get32(in + 24) != 1 && get32(in + 24) != GeneratorVersion) || crc32(in, 28) != get32(in + 28)) return false;
    Catch next; next.form = get32(in + 8); next.seed = get32(in + 12);
    next.millimetres = get32(in + 16); next.spot = get32(in + 20);
    next.generator = get32(in + 24);
    const uint32_t objectLimit = next.generator == 1 ? LegacyObjectForms : ObjectForms;
    if ((next.form >= FishForms && (next.form < ObjectFlag || next.form >= ObjectFlag + objectLimit)) ||
        next.spot > 2 || next.millimetres < 1 || next.millimetres > 2000) return false;
    c = next; return true;
}
bool Game::aligned() const { return std::fabs(fish - rod) <= zone(); }
bool Game::nibble() const { return stage == Stage::Waiting && age > 0.9f && std::fmod(age, 1.3f) < 0.2f; }
Anomaly chooseAnomaly(uint32_t seed, const Catch& caught, unsigned reflectionOdds) {
    Random event(seed ^ 0x73c194e5u);
    if (caught.object() && caught.objectType()==11 && event.below(3)==0) return Anomaly::FalseClock;
    if (caught.object() && caught.objectType()==9 && event.below(3)==0) return Anomaly::FutureReport;
    return event.below(reflectionOdds)==0 ? Anomaly::DoubleReflection : Anomaly::None;
}
bool Game::anomalyVisible() const {
    if (anomaly==Anomaly::DoubleReflection) return stage==Stage::Waiting && age>=0.8f && age<1.6f;
    return anomaly!=Anomaly::None && stage==Stage::Caught && age>=0.2f && age<3.0f;
}
void Game::lose(Loss why) { loss = why; stage = Stage::Lost; age = 0; }
void Game::cast() {
    const auto& profile=methodProfile(method);
    caught = generateForMethod(random.next(), spot,method);
    anomaly = Anomaly::None;
    if (anomalyCooldown) --anomalyCooldown;
    else { anomaly=chooseAnomaly(caught.seed,caught,profile.reflectionOdds); if(anomaly!=Anomaly::None)anomalyCooldown=3; }
    waitFor = profile.waitBase + random.below(unsigned(profile.waitSpan*1000)) / 1000.0f;
    biteWindow = (method==Method::Shallow?1.85f:1.65f) - 0.15f * caught.rarity();
    stage = Stage::Waiting; age = 0; paused = false;
}
void Game::tick(float seconds, const Input& in) {
    const bool action = in.action && !previous.action;
    const bool book = in.book && !previous.book;
    const bool back = in.back && !previous.back;
    const bool help = in.help && !previous.help;
    const bool pause = in.pause && !previous.pause;
    const bool read = in.read && !previous.read;
    const bool left = in.left && !previous.left;
    const bool right = in.right && !previous.right;
    const bool changeMethod = in.method && !previous.method;
    previous = in;
    // An OS pause never advances an unseen fight by seconds at once.
    float dt = clamp(seconds, 0, 0.05f);
    if (stage == Stage::Dossier) {
        if (read || back || book) stage = dossierBook ? Stage::Book : Stage::Caught;
        else if (action || right) dossierPage = (dossierPage + 1) % dossierPages;
        else if (left) dossierPage = (dossierPage + dossierPages - 1) % dossierPages;
        return;
    }
    if (read && (stage == Stage::Book || stage == Stage::Caught)) {
        dossierBook = stage == Stage::Book; dossierPage = 0; stage = Stage::Dossier; return;
    }
    if (help && stage != Stage::Help && stage != Stage::Book) {
        beforeHelp = stage; stage = Stage::Help; return;
    }
    if (stage == Stage::Help) { if (help || back || action) stage = beforeHelp; return; }
    if (stage == Stage::Book) { if (book || back) stage = Stage::Shore; return; }
    if (changeMethod && (stage==Stage::Shore||stage==Stage::Caught||stage==Stage::Lost)) {
        method=Method((unsigned(method)+1)%3);stage=Stage::Shore;age=0;return;
    }
    if (back) {
        if (stage == Stage::Fight || stage == Stage::Waiting || stage == Stage::Bite) lose(Loss::Released);
        else stage = Stage::Shore;
        paused = false; return;
    }
    if (book && (stage == Stage::Shore || stage == Stage::Caught || stage == Stage::Lost)) { stage = Stage::Book; return; }
    if (pause && (stage == Stage::Waiting || stage == Stage::Bite || stage == Stage::Fight)) paused = !paused;
    if (paused) return;
    age += dt;
    switch (stage) {
    case Stage::Shore:
    case Stage::Caught:
    case Stage::Lost:
        if (in.spot >= 0 && in.spot <= 2) spot = unsigned(in.spot);
        if (action && (stage == Stage::Shore || age > 0.5f)) cast();
        break;
    case Stage::Waiting:
        if (age >= waitFor) { stage = Stage::Bite; age = 0; }
        else if (action) lose(Loss::Early);
        break;
    case Stage::Bite:
        if (age > biteWindow) lose(Loss::Late);
        else if (action) {
            stage = Stage::Fight; age = fightAge = 0; fish = rod = target = 0.5f;
            progress = 0.18f; tension = 0.12f; turnIn = 0.4f; surgeIn = 2.0f; surgeLeft = 0;
        }
        break;
    case Stage::Fight: {
        fightAge += dt;
        rod = clamp(rod + (int(in.right) - int(in.left)) * dt * 0.78f, 0.08f, 0.92f);
        const unsigned b = caught.object() ? 0 : caught.behavior();
        turnIn -= dt; surgeIn -= dt; surgeLeft -= dt;
        if (turnIn <= 0) {
            target = 0.12f + random.below(760) / 1000.0f;
            turnIn = (b == 1 ? 0.5f : 1.25f) + random.below(800) / 1000.0f;
        }
        if (surgeIn <= 0) {
            surgeLeft = b == 3 ? 1.0f : 0.65f;
            surgeIn = 3.5f + random.below(1600) / 1000.0f;
        }
        float wanted = b == 2 ? 0.5f + 0.34f * std::sin(fightAge * 1.65f) : target;
        float speed = (b == 0 ? 0.18f : 0.34f) + 0.035f * caught.rarity();
        speed *= methodProfile(method).fishSpeed;
        if (surging()) speed *= 1.6f;
        fish = clamp(fish + clamp(wanted - fish, -speed * dt, speed * dt), 0.08f, 0.92f);
        // Gentle line-follow assist removes mandatory A/D + Space chords on the tiny keyboard.
        // Manual steering still overrides it; reeling and releasing remain player decisions.
        if (!in.left && !in.right)
            rod = clamp(rod + clamp(fish - rod, -0.31f * dt, 0.31f * dt), 0.08f, 0.92f);
        const float heft = caught.object() ? 0.8f : clamp(caught.millimetres / 350.0f, 0.6f, 1.7f);
        if (in.action) {
            tension += dt * (surging() ? 0.53f : aligned() ? 0.085f + 0.035f * heft : 0.49f);
            progress += dt * (aligned() ? methodProfile(method).reelRate : -0.09f);
        } else {
            tension -= dt * 0.48f;
            progress -= dt * 0.018f;
        }
        tension = clamp(tension, 0, 1);
        progress = clamp(progress, 0, 1);
        // Failure takes precedence if both thresholds are crossed on one step.
        if (tension >= 1) lose(Loss::Broken);
        else if (progress <= 0 || fightAge >= 40) lose(Loss::Escaped);
        else if (progress >= 1) { stage = Stage::Caught; age = 0; ++landed; newCatch = true; }
        break;
    }
    default: break;
    }
}
}
