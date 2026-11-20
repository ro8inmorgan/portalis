#pragma once
#include <cmath>
#include <cstring>
#include <iostream>
#define PROGMEM
#include <glcdfont.h>
namespace fonts { inline int Font0 = 0; }
constexpr int top_left = 0;
// Host-only adapter: record the real drawUi() primitives for documentation images.
class M5Canvas {
    float scale_ = 2;
    unsigned color_ = 0;
    template<class... Args> void emit(const char* kind, Args... args) {
        std::cout << "[\"" << kind << "\"";
        ((std::cout << ',' << args), ...);
        std::cout << "]\n";
    }
public:
    void setFont(const int*) {} void setTextWrap(bool) {} void setTextDatum(int) {}
    void setTextSize(float scale) { scale_ = scale; }
    void setTextColor(unsigned color) { color_ = color; }
    int textWidth(const char* s) { return std::strlen(s) * 6 * scale_; }
    void clearClipRect() { emit("unclip"); }
    void setClipRect(int x,int y,int w,int h) { emit("clip",x,y,w,h); }
    void fillSprite(unsigned c) { emit("background",c); }
    void fillRect(int x,int y,int w,int h,unsigned c) { emit("rect",x,y,w,h,c); }
    void fillRoundRect(int x,int y,int w,int h,int r,unsigned c) { emit("round",x,y,w,h,r,c); }
    void drawFastHLine(int x,int y,int w,unsigned c) { drawLine(x,y,x+w-1,y,c); }
    void drawLine(int x,int y,int xx,int yy,unsigned c) { emit("line",x,y,xx,yy,c); }
    void drawCircle(int x,int y,int r,unsigned c) { emit("circle",x,y,r,c); }
    void fillCircle(int x,int y,int r,unsigned c) { emit("disc",x,y,r,c); }
    void drawString(const char* value,int x,int y) {
        for (size_t i=0; value[i]; ++i) {
            const unsigned ch=static_cast<unsigned char>(value[i]);
            for (int col=0; col<5; ++col) for (int row=0; row<8; ++row) {
                if (!(font[ch*5+col] & (1<<row))) continue;
                const int left=x+static_cast<int>((i*6+col)*scale_);
                const int top=y+static_cast<int>(row*scale_);
                fillRect(left,top,x+static_cast<int>((i*6+col+1)*scale_)-left,
                    y+static_cast<int>((row+1)*scale_)-top,color_);
            }
        }
    }
};
