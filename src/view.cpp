#include "view.h"
#include "font_data.h"
#include "lore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#ifndef ARDUINO
#include <cassert>
#endif
namespace pond {
static constexpr uint16_t ink = rgb(27, 32, 29), cream = rgb(233, 221, 188), gold = rgb(224, 172, 87);
static constexpr uint16_t mint = rgb(138, 174, 153), muted = rgb(164, 171, 153);
static constexpr uint16_t grid = rgb(64, 78, 69), panel = rgb(37, 47, 41);
void Canvas::clear(uint16_t c) { std::fill(pixels, pixels + Width * Height, c); }
void Canvas::pixel(int x, int y, uint16_t c) { if (x >= 0 && x < Width && y >= 0 && y < Height) pixels[y * Width + x] = c; }
void Canvas::rect(int x, int y, int w, int h, uint16_t c) {
    for (int yy = std::max(0, y); yy < std::min(Height, y + h); ++yy)
        for (int xx = std::max(0, x); xx < std::min(Width, x + w); ++xx) pixel(xx, yy, c);
}
void Canvas::line(int x, int y, int x1, int y1, uint16_t c) {
    int dx = std::abs(x1 - x), sx = x < x1 ? 1 : -1, dy = -std::abs(y1 - y), sy = y < y1 ? 1 : -1, e = dx + dy;
    for (;;) { pixel(x, y, c); if (x == x1 && y == y1) break; int e2 = 2 * e; if (e2 >= dy) { e += dy; x += sx; } if (e2 <= dx) { e += dx; y += sy; } }
}
void Canvas::ellipse(int x, int y, int rx, int ry, uint16_t c) {
    if (rx <= 0 || ry <= 0) return;
    for (int yy = -ry; yy <= ry; ++yy) for (int xx = -rx; xx <= rx; ++xx)
        if (xx * xx * ry * ry + yy * yy * rx * rx <= rx * rx * ry * ry) pixel(x + xx, y + yy, c);
}
void Canvas::triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    auto edge = [](int ax, int ay, int bx, int by, int x, int y) { return (x - ax) * (by - ay) - (y - ay) * (bx - ax); };
    for (int y = std::max(0, std::min({y0, y1, y2})); y <= std::min(Height - 1, std::max({y0, y1, y2})); ++y)
        for (int x = std::max(0, std::min({x0, x1, x2})); x <= std::min(Width - 1, std::max({x0, x1, x2})); ++x) {
            int a = edge(x0,y0,x1,y1,x,y), b = edge(x1,y1,x2,y2,x,y), d = edge(x2,y2,x0,y0,x,y);
            if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) pixel(x,y,c);
        }
}
static uint32_t utf8(const char*& s) {
    auto a = uint8_t(*s++); if (a < 128) return a;
    unsigned n = a < 224 ? 1 : a < 240 ? 2 : 3; uint32_t u = a & ((1u << (6 - n)) - 1);
    while (n-- && *s) u = (u << 6) | (uint8_t(*s++) & 63);
    return u;
}
static const Glyph* glyph(uint32_t cp) {
    size_t lo = 0, hi = GlyphCount;
    while (lo < hi) { size_t m = (lo + hi) / 2; if (Glyphs[m].code < cp) lo = m + 1; else hi = m; }
    return lo < GlyphCount && Glyphs[lo].code == cp ? &Glyphs[lo] : nullptr;
}
int Canvas::textWidth(const char* s) const {
    int w = 0; while (*s) { auto cp = utf8(s); w += cp < 128 ? 7 : 12; } return w;
}
void Canvas::text(int x, int y, const char* s, uint16_t c) {
#ifndef ARDUINO
    if(x<0||x+textWidth(s)>Width||y<0||y+12>Height)std::fprintf(stderr,"text outside screen (%d,%d): %s\n",x,y,s);
    assert(x>=0&&x+textWidth(s)<=Width&&y>=0&&y+12<=Height);
#endif
    while (*s) { auto cp = utf8(s); const auto* g = glyph(cp);
        if (g) { for (int yy = 0; yy < 12; ++yy) for (int xx = 0; xx < 12; ++xx) if (g->rows[yy] & (1u << xx)) pixel(x + xx, y + yy, c); }
        else { rect(x + 1, y + 2, 5, 8, c); }
        x += cp < 128 ? 7 : 12;
    }
}
void drawCatch(Canvas& c, const Catch& original, int x, int y, int scale) {
    Catch f=original;if(!f.object())f.form=speciesForm(f.species());
    // Logical pixels are scaled as blocks: same deterministic drawing on device and host.
    auto p = [&](int a, int b, uint16_t col) { c.rect(x + a * scale, y + b * scale, scale, scale, col); };
    static const uint16_t colors[] = {rgb(137,172,153),rgb(192,142,99),rgb(209,179,113),rgb(114,154,168),rgb(175,157,165),rgb(122,155,121),rgb(218,210,176),rgb(113,136,143)};
    uint16_t base = colors[f.object() ? (f.form >> 3) & 7 : f.palette()];
    if (f.object()) {
        unsigned shape = f.objectType(), finish = (f.form >> 6) & 3;
        for (int yy = -17; yy <= 17; ++yy) for (int xx = -21; xx <= 21; ++xx) {
            bool inside = false;
            switch (shape) {
            case 0: inside = (xx > -11 && xx < 0 && yy > -14 && yy < 9) || (xx > -11 && xx < 17 && yy >= 5 && yy < 13); break;
            case 1: inside = (std::abs(xx) < 5 && yy > -16 && yy < -7) || (std::abs(xx) < 11 && yy >= -7 && yy < 15); break;
            case 2: inside = ((xx+9)*(xx+9)+yy*yy < 64 && (xx+9)*(xx+9)+yy*yy > 18) || (xx >= -4 && xx < 20 && std::abs(yy) < 3) || (xx > 9 && xx < 15 && yy > 0 && yy < 8); break;
            case 3: inside = xx*xx*2+yy*yy*3 < 310 || (xx>8 && xx<19 && yy>-7 && yy<0) || (xx<-9 && xx>-17 && std::abs(yy)<8 && (xx<-13 || std::abs(yy)>4)) || (std::abs(xx)<7 && yy==-12); break;
            case 4: inside = xx*xx+yy*yy<180 && xx+yy<14; break;
            case 5: inside = (xx*xx+yy*yy<200 && xx*xx+yy*yy>30) || (std::abs(xx)<4 && std::abs(yy)<17) || (std::abs(yy)<4 && std::abs(xx)<17); break;
            case 6: inside = std::abs(xx)<17 && std::abs(yy)<11; break;
            case 7: inside = (std::abs(xx)<8 && yy>-15 && yy<10) || (yy>4 && yy<14 && std::abs(xx)<15); break;
            case 8: inside = xx*xx+yy*yy < 145 || ((std::abs(xx)<14&&std::abs(yy)<4)||(std::abs(yy)<14&&std::abs(xx)<4)); break;
            case 9: inside = std::abs(xx)<20&&std::abs(yy)<13; break;
            case 10: inside = (std::abs(xx)<12&&yy>-7&&yy<17)||(std::abs(xx)<3&&yy>=-17&&yy<=-7); break;
            case 11: inside = xx*xx+yy*yy<174||(std::abs(xx)<4&&yy<-11&&yy>-18); break;
            case 12: inside = ((xx+11)*(xx+11)+(yy+9)*(yy+9)<65)||((xx-11)*(xx-11)+(yy-9)*(yy-9)<65)||(std::abs(yy-xx)<4&&std::abs(xx)<13); break;
            case 13: inside = xx*xx*2+(yy+5)*(yy+5)<155||(std::abs(xx)<3&&yy>=3&&yy<18); break;
            case 14: inside = (yy>-10&&yy<11&&std::abs(xx)<(yy+22)/2)||(yy>=9&&yy<13&&std::abs(xx)<17)||(xx*xx+(yy-13)*(yy-13)<10); break;
            case 15: inside = std::abs(xx)<13&&std::abs(yy)<13; break;
            case 16: inside = std::abs(xx)<17&&std::abs(yy)<16; break;
            case 17: inside = (std::abs(xx)<17&&yy>7&&yy<14)||(std::abs(xx)<5&&yy>=-1&&yy<=7)||(xx*xx+(yy+7)*(yy+7)<72); break;
            case 18: inside = std::abs(xx)<12&&std::abs(yy)<16; break;
            case 19: inside = xx*xx+yy*yy<196; break;
            case 20: inside = xx*xx*2+yy*yy<210||(std::abs(xx)>9&&std::abs(xx)<19&&std::abs(yy)<4)||(std::abs(xx)<5&&yy>=9&&yy<17); break;
            case 21: inside = (std::abs(xx)<20&&yy>-7&&yy<15)||(xx==8&&yy>-18&&yy<=-7); break;
            case 22: inside = std::abs(xx)<21&&std::abs(yy)<6; break;
            case 23: inside = xx*xx+(yy-6)*(yy-6)<90||((xx*xx+(yy+7)*(yy+7)<100)&&(xx*xx+(yy+7)*(yy+7)>50)); break;
            }
            if (!inside) continue;
            bool accent = shape == 6 ? (yy == 0 || (xx + yy) % 13 == 0) : shape == 4 ? ((xx*xx+yy*yy)/18)%2 : false;
            uint16_t col = accent ? cream : base;
            if (finish == 1 && (xx * 3 + yy * 7 + 1000) % 11 < 3) col = mint;
            if (finish == 2 && (xx + yy + 100) % 6 == 0) col = gold;
            if (finish == 3 && (xx * 7 + yy * 3 + 1000) % 19 == 0) col = cream;
            p(xx,yy,col);
        }
        if (shape == 7) { c.ellipse(x,y-4*scale,3*scale,3*scale,ink); c.line(x-3*scale,y+14*scale,x,y+18*scale,gold); }
        // Each new type has a distinct silhouette or internal construction at actual 1x size.
        if(shape==8){c.ellipse(x,y,8*scale,8*scale,ink);c.line(x-4*scale,y,x+4*scale,y,cream);}
        if(shape==9){for(int a:{-8,8}){c.ellipse(x+a*scale,y-2*scale,5*scale,5*scale,ink);c.ellipse(x+a*scale,y-2*scale,2*scale,2*scale,cream);}c.line(x-10*scale,y+8*scale,x+10*scale,y+8*scale,ink);}
        if(shape==10){c.ellipse(x-3*scale,y,3*scale,3*scale,ink);c.rect(x-7*scale,y+4*scale,8*scale,4*scale,ink);c.line(x-7*scale,y+12*scale,x+7*scale,y+12*scale,ink);}
        if(shape==11){c.ellipse(x,y,10*scale,10*scale,ink);c.line(x,y,x+5*scale,y-5*scale,cream);c.line(x,y,x,y+6*scale,cream);}
        if(shape==12){for(int a:{-11,11})for(int j=-1;j<=1;++j)p(a+j*2,a<0?-9:9,ink);}
        if(shape==13){c.ellipse(x,y-5*scale,6*scale,8*scale,ink);c.line(x-3*scale,y-3*scale,x+3*scale,y-9*scale,cream);}
        if(shape==14)c.line(x-10*scale,y+7*scale,x+10*scale,y+7*scale,ink);
        if(shape==15){for(int a:{-6,6})for(int b:{-6,6})c.ellipse(x+a*scale,y+b*scale,2*scale,2*scale,ink);c.ellipse(x,y,2*scale,2*scale,ink);}
        if(shape==16){c.rect(x-13*scale,y-12*scale,26*scale,22*scale,ink);c.ellipse(x+6*scale,y-7*scale,2*scale,2*scale,cream);c.triangle(x-12*scale,y+8*scale,x-3*scale,y-5*scale,x+9*scale,y+8*scale,mint);}
        if(shape==17)c.line(x-12*scale,y+10*scale,x+12*scale,y+10*scale,ink);
        if(shape==18){c.rect(x-2*scale,y-8*scale,4*scale,16*scale,ink);c.rect(x-7*scale,y-3*scale,14*scale,5*scale,ink);}
        if(shape==19){c.ellipse(x,y,10*scale,10*scale,ink);c.triangle(x-4*scale,y+3*scale,x+4*scale,y+3*scale,x,y-9*scale,cream);c.line(x,y,x,y+8*scale,mint);}
        if(shape==20){for(int a:{-5,5})c.ellipse(x+a*scale,y-3*scale,3*scale,4*scale,ink);c.rect(x-2*scale,y+7*scale,4*scale,5*scale,ink);}
        if(shape==21){c.ellipse(x-9*scale,y+3*scale,7*scale,7*scale,ink);c.rect(x+2*scale,y-3*scale,13*scale,4*scale,ink);for(int j=0;j<3;++j)c.line(x+3*scale,y+(5+j*3)*scale,x+14*scale,y+(5+j*3)*scale,ink);}
        if(shape==22)for(int j=-18;j<=18;j+=4)c.line(x+j*scale,y-4*scale,x+j*scale,y+(j%3==0?2:-1)*scale,ink);
        if(shape==23){c.line(x-4*scale,y+3*scale,x+4*scale,y+9*scale,ink);c.line(x+4*scale,y+3*scale,x-4*scale,y+9*scale,ink);}
        return;
    }
    unsigned body = f.body(), tail = f.tail(), fin = f.fin(), pattern = f.pattern();
    static const int lengths[] = {19,13,25,14,28,18,17,20};
    static const int heights[] = {10,14,7,12,5,11,14,12};
    int rx = lengths[body], ry = heights[body];
    // Tail shapes: fan, fork, ribbon, rounded.
    for (int yy=-16; yy<=16; ++yy) for(int xx=-rx-14; xx<-rx+4; ++xx) {
        int d = -rx - xx; bool inside = false;
        if (tail==0) inside = d>=0 && d<12 && std::abs(yy)<=d;
        if (tail==1) inside = d>=0 && d<14 && std::abs(yy)<=d && std::abs(yy)>=d/2;
        if (tail==2) inside = d>=0 && d<14 && std::abs(yy-(d/3-2))<3;
        if (tail==3) inside = (xx+rx+6)*(xx+rx+6)+yy*yy<70;
        if (inside) p(xx,yy,base);
    }
    // Four dorsal profiles all visible even on the smallest body.
    for(int xx=-rx/2;xx<rx/2;++xx) {
        int h=fin==0 ? 5 : fin==1 ? 4+(xx+rx/2)%5 : fin==2 ? 12-std::abs(xx) : 3+std::abs(xx)/2;
        for(int yy=-ry-h;yy<-ry+4;++yy) if(h>0) p(xx,yy,base);
    }
    for(int yy=-ry;yy<=ry;++yy) for(int xx=-rx;xx<=rx;++xx) {
        bool inside=xx*xx*ry*ry+yy*yy*rx*rx<=rx*rx*ry*ry;
        if(body==5) inside = std::abs(xx)*ry+std::abs(yy)*rx<=rx*ry;
        if(body==6) inside = xx*xx+yy*yy*2<=rx*rx;
        if(!inside) continue;
        bool mark=false;
        switch(pattern) {
        case 1: mark=(xx*7+yy*11+1000)%23<3; break;
        case 2: mark=(yy+30)%5==0; break;
        case 3: mark=(xx+40)%6<2; break;
        case 4: mark=((xx+40)/4+(yy+30)/4)%2==0; break;
        case 5: mark=(xx*xx+yy*yy)%61<12; break;
        case 6: mark=((xx+50)/7*17+(yy+30)/5*13)%7<2; break;
        case 7: mark=(xx+yy+70)%7==0; break;
        }
        p(xx,yy,mark ? cream : (yy>ry/2 ? gold : base));
    }
    if(body==3) for(int a=-10;a<=10;a+=5) {p(a,-ry-2,cream);p(a,ry+2,cream);}
    // Face: round eye, masked eye, sleepy eye, whiskered eye.
    int ex=rx-5, ey=-3;
    if(f.face()==1) c.ellipse(x+ex*scale,y+ey*scale,4*scale,4*scale,cream);
    if(f.face()==2) c.line(x+(ex-2)*scale,y+ey*scale,x+(ex+2)*scale,y+ey*scale,ink);
    else { c.ellipse(x+ex*scale,y+ey*scale,2*scale,2*scale,ink); p(ex,ey-1,cream); }
    if(f.face()==3) {c.line(x+(rx-1)*scale,y+2*scale,x+(rx+5)*scale,y+6*scale,cream);c.line(x+(rx-1)*scale,y+3*scale,x+(rx+4)*scale,y+9*scale,cream);}
    if(body==5)c.triangle(x+(rx-4)*scale,y-3*scale,x+(rx+3)*scale,y-3*scale,x+(rx+5)*scale,y-12*scale,base);
    c.line(x+(rx-2)*scale,y+3*scale,x+rx*scale,y+3*scale,ink);
    if(body==7||f.ornament()==1) {c.line(x,y-ry*scale,x+8*scale,y-(ry+8)*scale,gold);c.ellipse(x+9*scale,y-(ry+8)*scale,2*scale,2*scale,cream);}
    if(f.ornament()==2) {c.line(x-3*scale,y+ry*scale,x-8*scale,y+(ry+8)*scale,base);c.line(x+3*scale,y+ry*scale,x+8*scale,y+(ry+8)*scale,base);}
    if(f.ornament()==3) {for(int a=-1;a<=1;++a) c.triangle(x+(a*5-3)*scale,y-(ry+3)*scale,x+(a*5+3)*scale,y-(ry+3)*scale,x+a*5*scale,y-(ry+10)*scale,gold);}
}

static void outline(Canvas& c,int x,int y,int w,int h,uint16_t col) {
    c.line(x,y,x+w-1,y,col);c.line(x,y+h-1,x+w-1,y+h-1,col);
    c.line(x,y,x,y+h-1,col);c.line(x+w-1,y,x+w-1,y+h-1,col);
}
static void tabs(Canvas& c,const char* title,const char* status){
    c.rect(0,0,240,20,ink);c.text(8,4,title,cream);c.text(232-c.textWidth(status),4,status,gold);c.line(8,19,231,19,muted);
}
static void footer(Canvas& c,const char* s){c.rect(0,119,240,16,ink);c.line(8,119,231,119,grid);c.center(122,s,cream);}
static const char* saveLabel(SaveState s){switch(s){case SaveState::Ready:return "收藏已保存";case SaveState::Missing:return "无SD：本局不存档";case SaveState::Corrupt:return "存档异常：只读";default:return "写入失败：未保存";}}
static void page(Canvas& c,const LorePage& p){
    c.rect(0,20,240,99,cream);c.rect(0,20,3,99,gold);c.text(9,23,p.title,rgb(115,69,37));
    c.line(9,37,230,37,rgb(181,170,140));
    for(unsigned i=0;i<6;++i)c.text(9,40+i*13,p.lines[i],ink);
}
static void scene(Canvas& c,const Game& g,const ViewState& v){
    unsigned spot=g.spot,ms=unsigned(g.age*1000);
    uint16_t sky=spot==2?rgb(45,61,64):spot==1?rgb(159,121,84):rgb(122,143,131);
    uint16_t distant=spot==2?rgb(63,81,78):rgb(94,108,91),rust=rgb(129,91,63),water=spot==2?rgb(38,63,63):rgb(64,91,82);
    c.rect(0,20,240,56,sky);c.rect(0,76,240,43,water);
    if(spot==1)c.ellipse(193,42,12,12,rgb(219,175,107));
    if(spot==2){c.ellipse(198,39,7,7,cream);c.ellipse(201,36,7,7,sky);for(unsigned i=0;i<10;++i)c.pixel(15+mix(i)%175,25+mix(i+31)%31,muted);}
    // Far factory: stepped silhouettes, broken windows, a leaning chimney.
    for(int i=0;i<8;++i){unsigned h=mix(i+17);int x=i*34-8,y=52+h%13;c.rect(x,y,25,77-y,distant);}
    c.rect(146,37,7,33,distant);c.line(146,37,158,34,distant);
    for(int x=11;x<225;x+=17)c.rect(x,65,3,4,sky);
    c.rect(0,74,240,3,rust);
    if(spot==0){
        c.rect(18,45,50,30,rust);c.rect(23,49,39,22,rgb(155,130,96));c.text(31,52,"03",ink);
        c.rect(58,64,27,12,rust);c.ellipse(84,70,8,9,ink);c.ellipse(84,70,4,6,water);
        if((v.knownObjects&(1u<<5))||g.anomaly==Anomaly::Drain){int y=79+ms/100%13;c.line(82,78,82,y,mint);c.line(87,78,87,y+2,mint);}
        for(int i=0;i<6;++i){int x=3+i*6;c.line(x,107,x+3,80-i%3*6,ink);c.line(x+2,94,x+7,85,ink);}
    }else if(spot==1){
        c.rect(8,55,34,22,rust);c.rect(14,59,8,9,(v.knownObjects&(1u<<10))||g.anomaly==Anomaly::Lamp?gold:ink);
        c.rect(29,61,7,16,ink);c.rect(0,82,81,5,rgb(141,117,82));
        for(int x=9;x<79;x+=19){c.line(x,87,x-2,113,rust);c.line(x,83,x+9,83,ink);}
        if(v.knownObjects&(1u<<16)){c.line(52,65,52,81,ink);c.line(52,74,65,74,ink);c.line(64,74,64,81,ink);}
    }else{
        c.line(30,79,49,48,rust);c.line(49,48,95,52,rust);c.line(95,52,95,75,ink);c.rect(86,71,17,8,rust);
        c.ellipse(175,86,10,3,rust);c.rect(172,68,6,18,rust);c.rect(170,67,10,3,gold);
        if(v.knownObjects&(1u<<23))c.line(174,78,182,81,cream);
    }
    for(unsigned i=0;i<22;++i){unsigned h=mix(i+9);int x=(h%240+ms/280)%240,y=80+(h>>8)%34;c.line(x,y,std::min(x+int(h%9)+3,239),y,i%3?grid:mint);}
    if(g.anomaly==Anomaly::Rain)for(int i=0;i<14;++i){unsigned h=mix(i+89);int x=h%233,y=80+(h>>8)%33;c.line(x,y,x+2+ms/250%3,y,mint);}
    if(g.anomaly==Anomaly::Wind){int x=100+ms/70%120;c.rect(x,54+ms/200%9,6,4,cream);}
    if(g.anomaly==Anomaly::Shift)for(int i=0;i<5;++i){int x=(80+i*23+ms/70)%235;c.line(x,89,x+6,87,mint);}
    // Foreground shore and rod frame the water, leaving the centre open.
    c.triangle(0,106,75,119,0,119,ink);c.line(15,116,108,60,gold);c.line(15,117,109,61,rust);
    c.ellipse(20,111,5,5,rust);c.ellipse(20,111,2,2,ink);
}
static float ease(float t){t=clamp(t,0,1);return t*t*(3-2*t);}
static int between(int a,int b,float t){return a+int((b-a)*t);}
static uint16_t fadeInk(uint16_t a,uint16_t b,float t){
    return uint16_t(between((a>>11)&31,(b>>11)&31,t)<<11 |
                    between((a>>5)&63,(b>>5)&63,t)<<5 |
                    between(a&31,b&31,t));
}
// The quiet shore uses the existing framebuffer and primitives; no bitmap/video assets.
static void quietShore(Canvas& c,const Game& g,const ViewState& v,uint32_t ms){
    const float unfold=ease(g.arrivalAge/1.1f);
    const bool seated=g.arrivalAge>=1.75f;
    // Two coherent views, separated by a short fade, instead of stretching a chair into arms.
    const float sit=seated?.86f+.14f*ease((g.arrivalAge-1.75f)/.85f):0.f;
    const int horizon=between(64,45,sit);
    const bool night=g.spot==2;
    const auto sky=night?rgb(45,61,64):g.spot==1?rgb(159,121,84):rgb(145,123,92);
    const auto far=night?rgb(57,75,73):rgb(86,105,92);
    const auto water=night?rgb(38,63,63):rgb(58,83,76);
    const auto rust=rgb(129,91,63),wood=rgb(105,91,62),canvas=rgb(93,100,65);
    c.clear(sky);c.rect(0,horizon,240,135-horizon,water);
    if(night){
        c.ellipse(207,horizon-25,6,6,cream);c.ellipse(210,horizon-28,6,6,sky);
        for(unsigned i=0;i<9;++i)c.pixel(12+mix(i+31)%179,4+mix(i+71)%18,muted);
    }else c.ellipse(207,horizon-25,9,9,rgb(208,165,102));
    for(int i=0;i<12;++i){int x=i*22-8,h=5+mix(i+17)%12;c.rect(x,horizon-h,17,h,far);}
    c.rect(34,horizon-31,4,31,far);c.line(34,horizon-31,43,horizon-33,far);
    c.rect(0,horizon-2,240,2,rust);
    // Pump station 03, a single warm window, and the old overhead service pipe.
    c.rect(134,horizon-27,31,30,far);c.rect(139,horizon-30,21,3,far);
    c.rect(147,horizon-19,4,5,gold);c.rect(154,horizon-9,6,12,ink);
    c.line(92,horizon-20,141,horizon-20,far);c.line(92,horizon-19,92,horizon-6,far);
    c.text(136,horizon-13,"03",muted);
    if(g.spot==0){
        c.rect(8,horizon-11,24,12,wood);c.rect(25,horizon-5,20,7,rust);
        c.ellipse(45,horizon-1,4,5,ink);
        if((v.knownObjects&(1u<<5))||(g.effectLeft&&g.effect==Anomaly::Drain)){
            int y=horizon+5+int(ms/180%5);c.line(45,horizon+3,45,y,mint);
        }
    }else if(g.spot==1){
        c.rect(0,horizon+12,61,3,wood);
        for(int x=8;x<61;x+=17)c.line(x,horizon+15,x-2,horizon+30,rust);
        c.rect(14,horizon-7,5,5,(v.knownObjects&(1u<<10))?gold:ink);
        if(v.knownObjects&(1u<<16)){
            c.line(46,horizon+3,46,horizon+12,ink);c.line(46,horizon+8,53,horizon+8,ink);c.line(53,horizon+8,53,horizon+12,ink);
        }
    }else{
        c.line(23,horizon+3,43,horizon-23,rust);c.line(43,horizon-23,76,horizon-20,rust);
        c.line(76,horizon-20,76,horizon-2,ink);c.rect(71,horizon-4,10,5,rust);
        c.ellipse(190,horizon+22,6,2,rust);c.rect(188,horizon+11,3,11,rust);c.pixel(189,horizon+10,gold);
        if(v.knownObjects&(1u<<23))c.line(189,horizon+17,194,horizon+19,cream);
    }
    // Slow discrete ripples; the light itself stays steady, its reflection breathes.
    for(unsigned i=0;i<24;++i){unsigned h=mix(i+9);int x=(h%240+ms/750)%240,y=horizon+6+(h>>8)%57;
        c.line(x,y,std::min(x+int(h%7)+2,239),y,i%4?grid:rgb(111,143,125));}
    for(int i=0;i<4;++i){int x=146+int((ms/1100+i)%3),y=horizon+6+i*7;
        c.line(x,y,x+2+i%2,y,rgb(142,128,85));}
    // Bank, resting rod, and a low crate follow the eye down into the seat.
    int bank=between(120,126,sit);
    c.rect(0,between(124,133,sit),240,11,ink);
    c.triangle(0,bank-13,83,135,0,135,ink);c.triangle(240,bank-10,167,135,240,135,ink);
    c.rect(0,132,240,3,ink);
    for(int i=0;i<4;++i){c.line(3+i*4,bank,2+i*4,bank-12-i%2*4,ink);}
    c.line(4,bank+1,between(86,67,sit),bank-8,gold);c.line(4,bank+2,between(86,67,sit),bank-7,rust);
    c.ellipse(23,bank+1,4,4,rust);c.ellipse(23,bank+1,2,2,ink);
    const int cx=202,cy=between(109,118,sit);
    c.rect(cx-9,cy,30,20,ink);c.rect(cx-8,cy,29,3,wood);c.rect(cx-7,cy+4,27,15,grid);
    c.line(cx-7,cy+9,cx+18,cy+9,ink);c.line(cx-3,cy+4,cx-3,cy+18,wood);c.line(cx+14,cy+4,cx+14,cy+18,wood);
    c.rect(cx+9,cy-8,6,6,ink);c.rect(cx+10,cy-7,3,3,water);
    c.rect(cx-1,cy-10,11,10,cream);c.rect(cx-1,cy-11,11,2,ink);c.line(cx+1,cy-10,cx+7,cy-10,wood);
    c.rect(cx,cy-3,2,3,rust);c.pixel(cx+8,cy-8,wood);
    if(!seated){
        // X-frame rails keep a fixed length. Both feet spread around a fixed centre
        // on the same ground plane; the seat lowers as the scissors open.
        const int mid=80,footY=128;
        const float span=6.f+26.f*unfold;
        const int half=int(span*.5f),rise=int(std::sqrt(38.f*38.f-span*span));
        const int l=mid-half,r=mid+half,seat=footY-rise;
        // Rear frame (a small, constant perspective offset).
        c.line(l+5,seat-3,r+5,footY-3,wood);c.line(r+5,seat-3,l+5,footY-3,wood);
        c.line(l,seat,r,footY,rust);c.line(r,seat,l,footY,rust);
        c.rect(l-2,footY,5,2,wood);c.rect(r-2,footY,5,2,wood);
        c.pixel(mid,(seat+footY)/2,cream);
        // A real seat surface and upright backrest, never a full-screen polygon.
        c.triangle(l,seat,r,seat,r+5,seat-5,canvas);
        c.triangle(l,seat,r+5,seat-5,l+5,seat-5,canvas);
        c.line(l,seat,r,seat,ink);
        c.line(l+5,seat-5,l+4,seat-30,rust);c.line(r+5,seat-5,r+6,seat-30,rust);
        c.triangle(l+5,seat-28,r+5,seat-28,l+6,seat-7,canvas);
        c.triangle(r+5,seat-28,l+6,seat-7,r+4,seat-7,canvas);
        c.line(l+5,seat-28,r+5,seat-28,wood);
        c.line(l+6,seat-8,r+4,seat-8,wood);
        c.line(l-2,seat-6,l+5,seat-10,ink);c.line(r+1,seat-6,r+7,seat-10,ink);
        c.line(l-2,seat-5,l+5,seat-9,canvas);c.line(r+1,seat-5,r+7,seat-9,canvas);
        c.line(l-1,seat-5,l,seat+5,rust);c.line(r+1,seat-5,r,seat+5,rust);
    }else{
        const int drop=between(3,0,ease((g.arrivalAge-1.75f)/.85f));
        // Independently drawn first-person arms: no backrest is pulled through the camera.
        for(int side=0;side<2;++side){
            int x=side?188:53,d=side?-1:1,y=125+drop;
            c.triangle(x,y+1,x+d*12,y-3,x+d*9,y+9,canvas);
            c.line(x,y,x+d*12,y-4,ink);c.line(x,y+2,x+d*4,y+15,rust);
        }
    }
    float shade=0;
    if(g.arrivalAge>1.55f&&g.arrivalAge<1.75f)shade=ease((g.arrivalAge-1.55f)/.2f);
    else if(g.arrivalAge>=1.75f&&g.arrivalAge<1.95f)shade=1-ease((g.arrivalAge-1.75f)/.2f);
    if(shade>0)for(int i=0;i<Width*Height;++i)c.pixels[i]=fadeInk(c.pixels[i],ink,shade);
    if(g.arrivalGreeting&&g.shoreIdle>=2.6f&&g.shoreIdle<7){
        float opacity=clamp((g.shoreIdle-2.6f)/.6f,0,1)*clamp((7-g.shoreIdle)/1.5f,0,1);
        c.center(99,"坐会儿吧。",fadeInk(water,cream,opacity));
    }
    const bool hint=g.arrivalGreeting?(g.shoreIdle>=7&&g.shoreIdle<12):(g.shoreIdle<4);
    if(hint){
        char b[64];std::snprintf(b,sizeof b,"%s / %s",spotName(g.spot),methodProfile(g.method).name);
        c.text(8,5,b,cream);
        if(g.effectLeft){std::snprintf(b,sizeof b,"%s：余 %u 竿",eventName(g.effect),g.effectLeft);c.text(8,20,b,gold);}
        c.rect(61,119,119,15,ink);c.center(121,"空格抛竿 H 帮助",muted);
    }
    // Quiet presentation must not conceal persistence failures.
    if(v.save!=SaveState::Ready){c.rect(0,0,240,18,ink);c.center(3,saveLabel(v.save),cream);}
}
void syncDossierPages(Game& g,const ViewState& v){
    bool book=g.stage==Stage::Book||(g.stage==Stage::Dossier&&g.dossierBook);
    const auto& f=book?v.bookCatch:g.caught;
    g.dossierPages=f.object()?(annotationFor(f,v.knownObjects)?4:3):(fishAnnotation(f,v.knownObjects)?2:1);
    if(g.dossierPage>=g.dossierPages)g.dossierPage=0;
}
void draw(Canvas& c,const Game& g,const ViewState& v,uint32_t ms){
    char b[128];c.clear(ink);
    if(g.stage==Stage::Shore){quietShore(c,g,v,ms);return;}
    if(g.stage==Stage::Notes){
        unsigned n=g.notePage%(2+v.reading.count);
        std::snprintf(b,sizeof b,"%u/%u",n+1,2+v.reading.count);tabs(c,"岸边手记",b);
        if(n==0){
            page(c,{"调查摘记",{"只记录已经见过的材料。",v.knownObjects&(1u<<10)?"工牌：正反面属于同一个人。":"工牌：尚未发现。",v.knownObjects&(1u<<16)?"合照：十二个名字，十三个人。":"合照：尚未发现。",v.knownObjects&(1u<<11)?"怀表：沈在额外一格里签过退。":"怀表：尚未发现。",v.knownObjects&(1u<<23)?"铅封：夹痕位于封口内侧。":"铅封：尚未发现。",v.notebookSave==SaveState::Ready?"手记存档就绪，可随时放下。":v.notebookSave==SaveState::Missing?"无SD：手记仅保留本次开机。":"手记保存异常，本次修改未存。"}});
        }else if(n==1)page(c,siteDossier(g.spot));else page(c,eventDossier(v.reading.events[n-2]));
        footer(c,"空格下一页 A 上一页 N 返回");return;
    }
    if(g.stage==Stage::Dossier){
        std::snprintf(b,sizeof b,"%u/%u",g.dossierPage+1,g.dossierPages);tabs(c,"水边档案",b);
        if(g.dossierBook&&!v.bookValid){c.center(57,"没有可读的档案",cream);footer(c,"R 返回");return;}
        page(c,dossier(g.dossierBook?v.bookCatch:g.caught,g.dossierPage,v.knownObjects));
        footer(c,v.linkAvailable?"空格翻页 T 关联 R 返回":"空格翻页 R 返回");return;
    }
    if(g.stage==Stage::Help){
        tabs(c,"余波 / RADWATER","操作指南");
        page(c,{"在这里歇一会儿",{"空格抛竿，咬钩后再按一次。","按住收线，挣扎时松一下。","B 图鉴 R 档案 U 找未读","N 手记 T 关联 C 续读","F 钓法 1/2/3 水域 M 音","P 暂停；来不及咬钩会等你。"}});
        footer(c,"空格 / H 返回");return;
    }
    if(g.stage==Stage::Book||g.stage==Stage::Caught){
        bool book=g.stage==Stage::Book;const auto& f=book?v.bookCatch:g.caught;
        std::snprintf(b,sizeof b,"%lu/%lu %s",(unsigned long)v.bookIndex+1,(unsigned long)v.discoveries,v.unread?"未读":"已读");
        tabs(c,book?"收藏柜":"今日打捞",book?b:!v.saved?"未保存":v.fresh?"新发现":"又见面了");
        if(book&&!v.bookValid){c.center(49,v.discoveries?"图鉴读取失败":"收藏柜还是空的",cream);c.center(72,"钓获并保存后再来看看",muted);footer(c,"B 返回水域");return;}
        if(book){
            catchName(f,b,sizeof b);c.text(8,24,b,cream);drawCatch(c,f,125,83,2);
            std::snprintf(b,sizeof b,"%lu.%lu cm",(unsigned long)f.millimetres/10,(unsigned long)f.millimetres%10);c.text(232-c.textWidth(b),24,b,muted);
            footer(c,"A/D 选择 R 阅读 U 未读 B 返回");return;
        }
        drawCatch(c,f,56,51,1);catchName(f,b,sizeof b);c.text(110,28,b,cream);
        std::snprintf(b,sizeof b,"%lu.%lu cm",(unsigned long)f.millimetres/10,(unsigned long)f.millimetres%10);c.text(110,44,b,muted);
        if(g.anomaly!=Anomaly::None)std::snprintf(b,sizeof b,"N %s",eventName(g.anomaly));
        else std::snprintf(b,sizeof b,"%s",v.newAnnotations?"有新关联批注":"R 阅读完整档案");
        c.text(110,59,b,gold);
        auto card=dossier(f,0,v.knownObjects);unsigned start=f.object()?1:0;
        if(!v.fresh&&!f.object()&&fishAnnotation(f,v.knownObjects)){card=*fishAnnotation(f,v.knownObjects);start=0;}
        if(g.anomaly!=Anomaly::None){card=eventDossier(g.eventCode);start=0;}
        c.rect(0,75,240,43,panel);for(unsigned i=0;i<3;++i)c.text(8,77+i*13,card.lines[start+i],cream);
        footer(c,!v.saved?saveLabel(v.save):(v.notebookSave==SaveState::WriteFailed||v.notebookSave==SaveState::Corrupt)?"收藏已存 手记未存 N 查看":g.anomaly==Anomaly::Knock&&!g.responded?"空格再钓 E 轻敲 N 记录":"空格再钓 R 档案 N 手记");return;
    }
    std::snprintf(b,sizeof b,"%s / %s",spotName(g.spot),methodProfile(g.method).name);tabs(c,b,g.paused?"暂停":g.stage==Stage::Fight?"收线":g.stage==Stage::Bite?"咬钩":"水边");
    scene(c,g,v);
    if(g.stage==Stage::Fight){
        int x=185-int(g.progress*100),y=86+int(g.progress*16);
        c.ellipse(x,y,7,2,mint);c.triangle(x-6,y,x-10,y-3,x-10,y+3,mint);c.line(109,61,x,y-2,gold);
        c.rect(8,24,224,17,ink);c.center(26,g.surging()?"鱼在挣扎，松一下空格":"按住空格，慢慢靠岸",cream);
        if(g.surging()){c.line(x-12,y+4,x+12,y+4,cream);c.line(x-8,y-5,x+8,y-5,cream);}
        c.rect(150,109,80,5,ink);c.rect(151,110,int(78*g.tension),3,g.tension>.7f?gold:mint);
        footer(c,"按住收线 / 松手放线 P 暂停");
    }else if(g.stage==Stage::Waiting||g.stage==Stage::Bite){
        int y=g.stage==Stage::Bite?100:88+int(std::sin(g.age*4)*2);c.line(109,61,161,y-5,cream);c.rect(160,y-5,3,8,gold);c.line(150,y+4,173,y+4,mint);
        if(g.anomaly==Anomaly::DoubleReflection){c.line(156,y+7,156,y+11,muted);c.line(168,y+7,168,y+11,muted);}
        if(g.stage==Stage::Bite){c.rect(58,27,171,20,gold);c.text(66,31,"咬钩了！按一下空格",ink);}
        else if(g.anomalyVisible()){c.rect(8,25,148,17,ink);c.text(13,27,eventName(g.anomaly),gold);}
        footer(c,g.stage==Stage::Bite?"空格提竿 P 暂停":"等一小会儿 P 暂停");
    }else if(g.stage==Stage::Lost){
        c.rect(15,34,210,44,ink);c.center(40,g.loss==Loss::Broken?"这次没留住它":"收好鱼竿，歇一会儿",cream);c.center(59,"再钓一竿也来得及",muted);footer(c,"空格再钓 B 图鉴 N 手记");
    }else{
        if(g.effectLeft){std::snprintf(b,sizeof b,"%s：余 %u 竿",eventName(g.effect),g.effectLeft);c.rect(8,25,205,16,ink);c.text(12,27,b,gold);}
        else if(v.save!=SaveState::Ready){c.rect(8,25,220,16,ink);c.text(12,27,saveLabel(v.save),cream);}
        c.rect(86,103,147,14,ink);c.text(90,104,"B 收藏 N 手记 C 续读",cream);
        footer(c,"空格抛竿 F 钓法 H 帮助");
    }
    if(g.paused){c.rect(27,43,186,59,ink);outline(c,27,43,186,59,gold);c.center(48,"歇一会儿",cream);c.center(66,"鱼和进度都在这里等你",muted);c.center(85,"P 继续",gold);}
}
}
