#pragma once
#include "game.h"
#include "journal.h"
#include <cstdint>
namespace pond {
constexpr int Width = 240, Height = 135;
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) { return uint16_t((r & 248) << 8 | (g & 252) << 3 | b >> 3); }
class Canvas {
public:
    uint16_t* pixels;
    explicit Canvas(uint16_t* p) : pixels(p) {}
    void clear(uint16_t c);
    void pixel(int x, int y, uint16_t c);
    void rect(int x, int y, int w, int h, uint16_t c);
    void line(int x0, int y0, int x1, int y1, uint16_t c);
    void ellipse(int x, int y, int rx, int ry, uint16_t c);
    void triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c);
    void text(int x, int y, const char* s, uint16_t c);
    int textWidth(const char* s) const;
    void center(int y, const char* s, uint16_t c) { text((Width - textWidth(s)) / 2, y, s, c); }
};
struct ViewState {
    SaveState save = SaveState::Missing;
    uint32_t discoveries = 0, savedCatches = 0, bookIndex = 0;
    uint32_t knownObjects = 0, newAnnotations = 0;
    bool fresh = false, saved = false, sound = false, bookValid = false;
    Catch bookCatch;
};
void drawCatch(Canvas& c, const Catch& fish, int x, int y, int scale = 1);
void syncDossierPages(Game& game, const ViewState& view);
void draw(Canvas& canvas, const Game& game, const ViewState& view, uint32_t ms);
}
