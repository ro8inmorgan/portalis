#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace portalis {
inline void eraseSecret(std::string& value) {
    if (!value.empty()) {
        volatile char* p=&value[0];
        for (size_t i=0;i<value.size();++i) p[i]=0;
    }
    value.clear();
}
enum class SshAuth { Password, PrivateKey };
enum class SshPhase { Idle, Connecting, VerifyHost, Authenticating, Active, Closed, Failed, Stopping };
struct SshConfig {
    std::string host, user="root", secret, keyPath="/ssh/id_ed25519";
    uint16_t port=22;
    SshAuth auth=SshAuth::Password;
    bool valid() const {
        if (host.empty() || host.size()>63 || user.empty() || user.size()>63 || !port || secret.size()>128) return false;
        for (char c:host) if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-')) return false;
        for (unsigned char c:user) if (c<=32 || c>=127) return false;
        return auth!=SshAuth::PrivateKey || (!keyPath.empty() && keyPath.front()=='/' && keyPath.size()<=128 && keyPath.find("..")==std::string::npos);
    }
};
// A bounded ASCII/VT100-style display, owned exclusively by the UI thread.
class TerminalBuffer {
public:
    static constexpr int columns=40, rows=14, historyLines=96;
    using Line=std::array<char,columns+1>;
private:
    std::array<Line,rows> screen_{};
    std::array<Line,historyLines> history_{};
    size_t historyCount_=0, historyHead_=0;
    int x_=0,y_=0,savedX_=0,savedY_=0,top_=0,bottom_=rows-1;
    enum class Parse { Text, Escape, Csi, Osc, OscEscape, Charset } parser_=Parse::Text;
    std::array<int,8> params_{};
    int paramIndex_=0;
    bool private_=false,wrap_=false;
    static void blank(Line& line) { line.fill(' '); line[columns]=0; }
    void scroll() {
        if (top_==0 && bottom_==rows-1) {
            history_[historyHead_]=screen_[0]; historyHead_=(historyHead_+1)%historyLines;
            historyCount_=std::min(historyCount_+1,size_t(historyLines));
        }
        for (int row=top_;row<bottom_;++row) screen_[row]=screen_[row+1];
        blank(screen_[bottom_]);
    }
    void newline() { wrap_=false; if (y_==bottom_) scroll(); else y_=std::min(rows-1,y_+1); }
    int arg(int index,int fallback=1) const { return params_[index] ? params_[index] : fallback; }
    void csi(char code) {
        wrap_=false;
        switch (code) {
            case 'A': y_=std::max(0,y_-arg(0)); break;
            case 'B': y_=std::min(rows-1,y_+arg(0)); break;
            case 'C': x_=std::min(columns-1,x_+arg(0)); break;
            case 'D': x_=std::max(0,x_-arg(0)); break;
            case 'G': x_=std::min(columns-1,arg(0)-1); break;
            case 'd': y_=std::min(rows-1,arg(0)-1); break;
            case 'H': case 'f': y_=std::min(rows-1,arg(0)-1); x_=std::min(columns-1,arg(1)-1); break;
            case 'J':
                if (params_[0]==2 || params_[0]==3) { for (auto& line:screen_) blank(line); }
                else if (params_[0]==0) { std::fill(screen_[y_].begin()+x_,screen_[y_].begin()+columns,' '); for(int r=y_+1;r<rows;++r) blank(screen_[r]); }
                else if (params_[0]==1) { for(int r=0;r<y_;++r) blank(screen_[r]); std::fill(screen_[y_].begin(),screen_[y_].begin()+x_+1,' '); }
                break;
            case 'K': {
                const int begin=params_[0]==0 ? x_ : 0, end=params_[0]==1 ? x_+1 : columns;
                std::fill(screen_[y_].begin()+begin,screen_[y_].begin()+end,' '); break;
            }
            case 'r': if (!private_ && arg(0)<arg(1,rows) && arg(1,rows)<=rows) { top_=arg(0)-1; bottom_=arg(1,rows)-1; x_=y_=0; } break;
            case 's': savedX_=x_;savedY_=y_; break;
            case 'u': x_=savedX_;y_=savedY_; break;
            case 'h': case 'l':
                if (private_ && params_[0]==1049) { for(auto& line:screen_) blank(line); x_=y_=0; top_=0;bottom_=rows-1; }
                break;
            default: break; // Colors and unsupported terminal extensions are ignored.
        }
    }
public:
    TerminalBuffer() { reset(); }
    void reset() {
        for(auto& line:screen_) blank(line);
        historyCount_=historyHead_=0; x_=y_=savedX_=savedY_=top_=0;bottom_=rows-1;
        parser_=Parse::Text;wrap_=false;
    }
    void feed(const char* bytes,size_t size) {
        for(size_t i=0;i<size;++i) {
            const unsigned char c=bytes[i];
            if (parser_==Parse::Osc) { if(c==7) parser_=Parse::Text; else if(c==27) parser_=Parse::OscEscape; continue; }
            if (parser_==Parse::OscEscape) { parser_=c=='\\' ? Parse::Text : Parse::Osc;continue; }
            if (parser_==Parse::Charset) { parser_=Parse::Text;continue; }
            if (parser_==Parse::Escape) {
                parser_=Parse::Text;
                if(c=='[') { parser_=Parse::Csi;params_.fill(0);paramIndex_=0;private_=false; }
                else if(c==']') parser_=Parse::Osc;
                else if(c=='(' || c==')') parser_=Parse::Charset;
                else if(c=='7') { savedX_=x_;savedY_=y_; }
                else if(c=='8') { x_=savedX_;y_=savedY_;wrap_=false; }
                else if(c=='D') newline();
                else if(c=='E') { x_=0;newline(); }
                continue;
            }
            if (parser_==Parse::Csi) {
                if(c>='0'&&c<='9') params_[paramIndex_]=std::min(9999,params_[paramIndex_]*10+c-'0');
                else if(c==';') paramIndex_=std::min(7,paramIndex_+1);
                else if(c=='?') private_=true;
                else if(c>=0x40 && c<=0x7e) { csi(c);parser_=Parse::Text; }
                else if(c==27) parser_=Parse::Escape;
                continue;
            }
            if(c==27) { parser_=Parse::Escape;continue; }
            if(c=='\r') { x_=0;wrap_=false;continue; }
            if(c=='\n') { newline();continue; }
            if(c=='\b') { x_=std::max(0,x_-1);wrap_=false;continue; }
            if(c=='\t') { x_=std::min(columns-1,(x_/8+1)*8);wrap_=false;continue; }
            if(c<32 || c==127 || (c>=128&&c<192)) continue;
            if(wrap_) { x_=0;newline(); }
            screen_[y_][x_]=c<127 ? c : '?';
            if(x_==columns-1) wrap_=true; else ++x_;
        }
    }
    size_t historyCount() const { return historyCount_; }
    const char* line(int row,size_t scrollback=0) const {
        scrollback=std::min(scrollback,historyCount_);
        const size_t index=historyCount_-scrollback+std::max(0,std::min(rows-1,row));
        return index<historyCount_ ? history_[(historyHead_+historyLines-historyCount_+index)%historyLines].data() : screen_[index-historyCount_].data();
    }
    int column() const { return x_; }
    int row() const { return y_; }
};
struct SshStatus {
    SshPhase phase=SshPhase::Idle;
    char message[96]={}, fingerprint[65]={};
};
} // namespace portalis
