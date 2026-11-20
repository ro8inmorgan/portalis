#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
namespace fonts { inline int Font0 = 0; }
constexpr int top_left = 0;
// A host display spy, used to check the real ui.cpp against 240x135 bounds.
struct DrawText { std::string value; int x,y; float scale; uint16_t color; bool clipped; };
class M5Canvas {
    float scale_ = 2;
    uint16_t color_ = 0;
    bool clipped_ = false;
public:
    int violations = 0;
    std::vector<DrawText> texts;
    void clearClipRect() { clipped_ = false; }
    void fillSprite(uint16_t) { texts.clear(); }
    void setFont(const int*) {} void setTextWrap(bool) {} void setTextDatum(int) {}
    void setTextColor(uint16_t c) { color_ = c; } void setTextSize(float s) { scale_ = s; }
    int textWidth(const char* s) { return strlen(s)*6*scale_; }
    void setClipRect(int x,int y,int w,int h) { bounds(x,y,w,h); clipped_ = true; }
    void bounds(int x,int y,int w,int h) { if(x<0 || y<0 || x+w>240 || y+h>135 || w<0 || h<0) ++violations; }
    void drawString(const char* s,int x,int y) {
        if (!clipped_) bounds(x,y,textWidth(s),8*scale_);
        texts.push_back({s,x,y,scale_,color_,clipped_});
    }
    void fillRect(int x,int y,int w,int h,uint16_t) { if (!clipped_) bounds(x,y,w,h); }
    void fillRoundRect(int x,int y,int w,int h,int,uint16_t) { if (!clipped_) bounds(x,y,w,h); }
    void drawFastHLine(int x,int y,int w,uint16_t) { bounds(x,y,w,1); }
    void drawLine(int x0,int y0,int x1,int y1,uint16_t) { if (!clipped_) bounds(std::min(x0,x1),std::min(y0,y1),std::abs(x1-x0)+1,std::abs(y1-y0)+1); }
    void drawCircle(int x,int y,int r,uint16_t) { if (!clipped_) bounds(x-r,y-r,r*2+1,r*2+1); }
    void fillCircle(int x,int y,int r,uint16_t) { if (!clipped_) bounds(x-r,y-r,r*2+1,r*2+1); }
};
