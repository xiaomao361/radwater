#include "view.h"
#include "font_data.h"
#include "lore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace pond {
static constexpr uint16_t ink = rgb(3, 10, 3), cream = rgb(189, 255, 137), gold = rgb(143, 241, 83);
static constexpr uint16_t mint = rgb(108, 196, 64), muted = rgb(107, 165, 77);
static constexpr uint16_t grid = rgb(28, 66, 22), panel = rgb(8, 24, 7);
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
    while (*s) { auto cp = utf8(s); const auto* g = glyph(cp);
        if (g) { for (int yy = 0; yy < 12; ++yy) for (int xx = 0; xx < 12; ++xx) if (g->rows[yy] & (1u << xx)) pixel(x + xx, y + yy, c); }
        else { rect(x + 1, y + 2, 5, 8, c); }
        x += cp < 128 ? 7 : 12;
    }
}
void drawCatch(Canvas& c, const Catch& f, int x, int y, int scale) {
    // Logical pixels are scaled as blocks: same deterministic drawing on device and host.
    auto p = [&](int a, int b, uint16_t col) { c.rect(x + a * scale, y + b * scale, scale, scale, col); };
    static const uint16_t colors[] = {rgb(115,212,64),rgb(142,232,81),rgb(163,242,98),rgb(87,165,44),rgb(102,186,56),rgb(128,222,74),rgb(188,255,137),rgb(72,145,38)};
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
    c.line(x+(rx-2)*scale,y+3*scale,x+rx*scale,y+3*scale,ink);
    if(f.ornament()==1) {c.line(x,y-ry*scale,x+8*scale,y-(ry+8)*scale,gold);c.ellipse(x+9*scale,y-(ry+8)*scale,2*scale,2*scale,cream);}
    if(f.ornament()==2) {c.line(x-3*scale,y+ry*scale,x-8*scale,y+(ry+8)*scale,base);c.line(x+3*scale,y+ry*scale,x+8*scale,y+(ry+8)*scale,base);}
    if(f.ornament()==3) {for(int a=-1;a<=1;++a) c.triangle(x+(a*5-3)*scale,y-(ry+3)*scale,x+(a*5+3)*scale,y-(ry+3)*scale,x+a*5*scale,y-(ry+10)*scale,gold);}
}

static void star(Canvas& c,int x,int y,uint16_t col) {c.line(x-2,y,x+2,y,col);c.line(x,y-2,x,y+2,col);}
static void outline(Canvas& c,int x,int y,int w,int h,uint16_t col) {
    c.line(x,y,x+w-1,y,col);c.line(x,y+h-1,x+w-1,y+h-1,col);
    c.line(x,y,x,y+h-1,col);c.line(x+w-1,y,x+w-1,y+h-1,col);
}
static void brackets(Canvas& c,int x,int y,int w,int h,uint16_t col) {
    for(int i=0;i<2;++i)for(int j=0;j<2;++j){int xx=x+i*(w-1),yy=y+j*(h-1);c.line(xx,yy,xx+(i?-5:5),yy,col);c.line(xx,yy,xx,yy+(j?-5:5),col);}
}
static void background(Canvas& c) {
    c.clear(ink);
    // Static phosphor texture stays behind text: no flicker or moving full-screen band.
    for(int y=21;y<119;y+=3)c.line(5,y,234,y,rgb(6,17,5));
}
static void tabs(Canvas& c,int selected,const char* status) {
    const char* names[]={"FISH","LOG","SYS"};
    int widths[]={39,32,32},x=8;
    for(int i=0;i<3;++i){if(i==selected){c.rect(x-3,2,widths[i],14,gold);c.text(x,3,names[i],ink);}else c.text(x,3,names[i],muted);x+=widths[i]+9;}
    c.text(232-c.textWidth(status),3,status,gold);c.line(5,18,234,18,mint);
}
static void footer(Canvas& c,const char* s) {c.rect(0,119,240,16,ink);c.line(5,119,234,119,mint);c.center(122,s,cream);}
static const char* saveLabel(SaveState s) {
    switch(s) {case SaveState::Ready:return "存档就绪";case SaveState::Missing:return "无SD：本局不存档";case SaveState::Corrupt:return "存档异常：只读";default:return "写入失败：未保存";}
}
static void bar(Canvas& c,int x,int y,int w,float v,uint16_t col) {
    outline(c,x,y,w,8,muted);int filled=int((w-4)*clamp(v,0,1));
    for(int n=0;n<filled;n+=5)c.rect(x+2+n,y+2,std::min(3,filled-n),4,col);
}
static void scene(Canvas& c,unsigned spot,uint32_t ms) {
    // First-person fishing rig and a line-drawn industrial shoreline, no avatar.
    brackets(c,7,40,226,63,grid);
    c.line(10,73,230,73,mint);
    const int shift=int(spot)*8;
    c.line(19,69,19,52,muted);c.line(19,52,43,52,muted);c.line(43,52,43,69,muted);
    c.line(25,52,29,43,muted);c.line(29,43,37,43,muted);c.line(37,43,41,52,muted);
    c.rect(26,57,3,5,mint);c.rect(34,57,3,5,mint);
    c.line(57+shift,69,57+shift,39,muted);c.line(69+shift,69,69+shift,39,muted);
    outline(c,53+shift,38,21,12,mint);c.line(57+shift,53,69+shift,66,muted);c.line(69+shift,53,57+shift,66,muted);
    c.line(110,73,131,65,muted);c.line(131,65,158,70,muted);c.line(158,70,172,64,muted);c.line(172,64,199,70,muted);
    c.line(210,70,211,48,muted);c.line(211,57,221,52,muted);c.line(211,61,202,53,muted);
    if(spot==2){for(int i=0;i<8;++i){unsigned h=mix(i+34);c.pixel(100+h%104,43+(h>>9)%16,mint);}star(c,181,48,gold);}
    for(int i=0;i<13;++i){unsigned h=mix(i+11);int x=15+(h%210+ms/240)%210,y=78+(h>>8)%23;c.line(x,y,std::min(x+3+int((h>>16)%8),229),y,grid);}
    c.line(11,108,130,48,gold);c.line(12,110,131,49,mint);
    for(int i=0;i<4;++i){int x=30+i*23,y=98-i*12;c.line(x,y-2,x+2,y+2,cream);}
    outline(c,18,101,12,11,mint);c.line(24,101,24,112,mint);c.line(16,104,18,104,gold);
}
static void scanFrame(Canvas& c,int x,int y,int w,int h) {
    c.rect(x,y,w,h,panel);
    for(int i=x+12;i<x+w;i+=12)c.line(i,y+2,i,y+h-3,grid);
    for(int j=y+9;j<y+h;j+=10)c.line(x+2,j,x+w-3,j,grid);
    brackets(c,x,y,w,h,mint);
}
void syncDossierPages(Game& g,const ViewState& v) {
    bool book=g.stage==Stage::Book||(g.stage==Stage::Dossier&&g.dossierBook);
    g.dossierPages=(!book||v.bookValid)&&annotationFor(book?v.bookCatch:g.caught,v.knownObjects)?4:3;
    if(g.dossierPage>=g.dossierPages)g.dossierPage=0;
}
void draw(Canvas& c,const Game& g,const ViewState& v,uint32_t ms) {
    char b[100];background(c);
    if(g.stage==Stage::Dossier) {
        std::snprintf(b,sizeof b,"%u / %u",g.dossierPage+1,g.dossierPages);tabs(c,1,b);
        if(g.dossierBook&&!v.bookValid) {
            c.center(57,"没有可读的标本档案",cream);footer(c,"R 返回图鉴");return;
        }
        const auto page=dossier(g.dossierBook?v.bookCatch:g.caught,g.dossierPage,v.knownObjects);
        c.text(8,23,page.title,gold);
        for(int i=0;i<6;++i)c.text(8,39+i*13,page.lines[i],i==0?muted:cream);
        footer(c,"空格翻页  R 返回");return;
    }
    if(g.stage==Stage::Help) {
        tabs(c,2,"指南");c.text(9,23,"ANGLER / 操作手册",gold);
        c.text(9,41,"空格抛竿，等浮漂真正下沉",cream);
        c.text(9,57,"咬钩再按空格，自动跟鱼",cream);
        c.text(9,73,"按住空格收线，张力高松手",cream);
        c.text(9,90,"B 图鉴 R 档案 P 暂停",mint);
        c.text(9,105,"F 钓法 1/2/3 水域 M 音",muted);
        footer(c,"空格 / H 返回");return;
    }
    if(g.stage==Stage::Book || g.stage==Stage::Caught) {
        const bool book=g.stage==Stage::Book;const Catch& f=book?v.bookCatch:g.caught;
        tabs(c,1,book?"图鉴":!v.saved?"未存档":v.fresh?"新发现":"已记录");
        if(book)std::snprintf(b,sizeof b,"%lu / %lu",static_cast<unsigned long>(v.bookIndex+1),static_cast<unsigned long>(v.discoveries));
        else if(g.anomalyVisible()&&g.anomaly==Anomaly::FalseClock)std::snprintf(b,sizeof b,"仪表 25:13");
        else std::snprintf(b,sizeof b,"本局 %u",g.landed);
        c.text(8,23,book?"标本档案":v.newAnnotations?"新增批注 R 档案":"捕获报告 R 档案",gold);
        if(!book||v.bookValid)c.text(232-c.textWidth(b),23,b,muted);
        if(book&&!v.bookValid){brackets(c,20,43,200,55,grid);c.center(51,v.discoveries?"图鉴读取失败":"尚无标本记录",cream);c.center(75,"成功保存后加入图鉴",muted);footer(c,"B 返回水域");return;}
        scanFrame(c,8,41,102,61);drawCatch(c,f,63,70,1);
        if(f.rarity()==2){star(c,17,50,gold);star(c,101,94,gold);}
        catchName(f,b,sizeof b);c.text(117,40,b,cream);
        if(f.object()){
            c.text(117,56,colorName((f.form>>3)&7),mint);c.text(117,72,"拾得物",muted);
            static const char* condition[]={"普通表面","苔藓覆盖","刻纹残片","星尘附着"};c.text(117,88,condition[(f.form>>6)&3],mint);
        }else{
            c.text(117,56,patternName(f.pattern()),mint);
            std::snprintf(b,sizeof b,"%lu.%lu cm",static_cast<unsigned long>(f.millimetres/10),static_cast<unsigned long>(f.millimetres%10));c.text(117,72,b,cream);
            std::snprintf(b,sizeof b,"习性 %s",behaviorName(f.behavior()));c.text(117,88,b,muted);
        }
        if(!book&&g.anomalyVisible()&&g.anomaly==Anomaly::FutureReport)c.text(8,105,"本次打捞已于明日完成",gold);
        else {
            std::snprintf(b,sizeof b,"ID %06lX",static_cast<unsigned long>(f.form));c.text(8,105,b,muted);
            static const char* type[]={"常见特征","特殊特征","稀有特征"};c.text(117,105,type[f.rarity()],gold);
        }
        footer(c,book?(annotationFor(f,v.knownObjects)?"A/D 选标本 R 有批注 B 返回":"A/D 选标本 R 档案 B 返回"):v.saved?"已保存 空格再钓 R 档案 B 图鉴":saveLabel(v.save));return;
    }
    tabs(c,0,g.paused?"暂停":g.stage==Stage::Fight?"追踪":g.stage==Stage::Bite?"咬钩":g.stage==Stage::Waiting?"侦测":"待命");
    std::snprintf(b,sizeof b,"0%u %s / %s",g.spot+1,spotName(g.spot),methodProfile(g.method).name);c.text(8,23,b,gold);
    std::snprintf(b,sizeof b,"发现 %lu",static_cast<unsigned long>(v.discoveries));c.text(232-c.textWidth(b),23,b,muted);
    if(g.stage==Stage::Fight){
        const int left=15,width=210,center=left+int(g.rod*width),fx=left+int(g.fish*width),half=int(g.zone()*width);
        c.text(9,40,g.surging()?"! 冲刺：松线":g.tension>0.75f?"! 张力过高：松手":"自动跟鱼：按住空格收线",cream);
        c.rect(left,57,width,20,panel);outline(c,left,57,width,20,muted);
        for(int i=0;i<=20;++i){int x=left+i*(width-1)/20;c.line(x,58,x,i%5==0?63:60,grid);}
        const int a=std::max(left+1,center-half),z=std::min(left+width-1,center+half);
        c.rect(a,63,z-a,12,rgb(30,85,17));c.line(a,63,a,75,gold);c.line(z,63,z,75,gold);
        c.line(center,59,center,76,mint);c.ellipse(fx,68,4,3,cream);c.triangle(fx-3,68,fx-7,64,fx-7,72,cream);
        c.text(9,82,"收线",cream);bar(c,40,86,158,g.progress,gold);std::snprintf(b,sizeof b,"%02u",unsigned(g.progress*100));c.text(209,82,b,gold);
        c.text(9,101,"张力",cream);bar(c,40,105,158,g.tension,g.tension>0.7f?cream:mint);
        c.text(209,101,g.tension>0.7f?"!!":"OK",g.tension>0.7f?cream:muted);
        footer(c,"空格收线 / 松手放线  P 暂停");
    }else{
        scene(c,g.spot,ms);
        if(g.stage==Stage::Waiting||g.stage==Stage::Bite){
            int by=g.stage==Stage::Bite?91:g.nibble()?85:80+int(std::sin(ms/230.0f)*2);
            c.line(131,49,165,by-5,muted);c.line(156,by+6,178,by+6,mint);
            c.rect(164,by-5,3,10,cream);c.rect(164,by-1,3,2,ink);
            // Reflections stay below the float and vanish before the bite signal.
            if(g.anomalyVisible()&&g.anomaly==Anomaly::DoubleReflection){
                for(int dx:{-5,5}){c.line(165+dx,by+9,165+dx,by+13,mint);c.line(164+dx,by+16,166+dx,by+16,muted);}
            }
            if(g.stage==Stage::Bite){c.rect(80,45,148,22,gold);c.text(89,50,"咬钩！现在按空格",ink);}
            else{c.rect(108,43,99,15,ink);c.text(111,44,g.nibble()?"试探中...":"等待信号...",mint);}
            footer(c,g.stage==Stage::Bite?"[ 空格提竿 ]":g.nibble()?"只是试探，继续等待":"等浮漂下沉  P 暂停");
        }else if(g.stage==Stage::Lost){
            static const char* why[]={"提早了，鱼还在试探","晚了一步，鱼游走了","线断了，下次松一松","鱼挣脱了，下次再来","已放弃本次垂钓"};
            c.rect(47,47,181,48,ink);outline(c,47,47,181,48,mint);
            c.text(57,52,"信号中断",cream);c.text(57,73,why[int(g.loss)],mint);
            footer(c,"空格再钓  B 图鉴");
        }else{
            c.rect(110,43,122,17,ink);c.text(113,45,methodProfile(g.method).hint,mint);
            c.rect(54,104,179,13,ink);c.text(232-c.textWidth(saveLabel(v.save)),105,saveLabel(v.save),v.save==SaveState::Ready?muted:cream);
            footer(c,"空格抛竿 F 钓法 B 图鉴 H 帮助");
        }
    }
    if(g.paused){c.rect(44,43,152,58,ink);outline(c,44,43,152,58,gold);c.center(49,"垂钓已暂停",cream);c.center(68,"当前进度保留",mint);c.center(84,"P 继续",gold);}
}
}
