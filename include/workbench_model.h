#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include "ssh_model.h"
namespace portalis {
struct SurveyStatus {bool recording=false,failed=false;uint32_t sweeps=0,rows=0,missed=0,seconds=0;char path[64]={},message[80]={};};
struct TransferStatus {bool running=false,stopping=false;uint32_t uploads=0,downloads=0;char address[32]={},code[17]={},message[96]={};};
struct TcpConfig {std::string host;uint16_t port=23;bool telnet=true;};
enum class TcpPhase {Idle,Connecting,Active,Stopping,Closed,Failed};
struct TcpStatus {TcpPhase phase=TcpPhase::Idle;char message[80]={};};
inline bool simpleFileName(const std::string& name) {
    if(name.empty() || name.size()>80 || name.front()=='.') return false;
    for(unsigned char c:name) if(c<32 || c>126 || c=='/' || c=='\\' || c==':' || c=='"') return false;
    return name.back()!=' ' && name.back()!='.';
}
inline std::string csvQuote(const std::string& value) {
    std::string out="\"";for(char c:value) {if(c=='"') out+='"';out+=c;}return out+'"';
}
class ScriptEditor {
public:
    static constexpr size_t limit=8192;
    std::string name,text,message;
    size_t cursor=0,top=0,left=0;
    bool dirty=false;
    void reset(const std::string& file,const std::string& content,bool fresh=false) {name=file;text=content;cursor=top=left=0;dirty=fresh;message.clear();}
    size_t lineStart(size_t at) const {auto p=at ? text.rfind('\n',at-1) : std::string::npos;return p==std::string::npos ? 0 : p+1;}
    size_t lineEnd(size_t at) const {auto p=text.find('\n',at);return p==std::string::npos ? text.size() : p;}
    size_t row() const {return std::count(text.begin(),text.begin()+cursor,'\n');}
    size_t column() const {return cursor-lineStart(cursor);}
    void reveal() {size_t r=row(),c=column();if(r<top)top=r;if(r>=top+12)top=r-11;if(c<left)left=c;if(c>=left+40)left=c-39;}
    bool insert(const std::string& value) {if(text.size()+value.size()>limit) {message="Editor limit: 8 KB";return false;}text.insert(cursor,value);cursor+=value.size();dirty=true;message.clear();reveal();return true;}
    void newline() {auto start=lineStart(cursor);size_t n=0;while(start+n<cursor && text[start+n]==' ')++n;insert("\n"+std::string(std::min(n,size_t(80)),' '));}
    void backspace() {if(cursor) {text.erase(--cursor,1);dirty=true;message.clear();reveal();}}
    void move(int dx,int dy) {
        if(dx<0 && cursor)--cursor;else if(dx>0 && cursor<text.size())++cursor;
        if(dy) {size_t col=column(),start=lineStart(cursor);if(dy<0 && start) {size_t end=start-1;cursor=std::min(lineStart(end)+col,end);}else if(dy>0) {size_t end=lineEnd(cursor);if(end<text.size())cursor=std::min(end+1+col,lineEnd(end+1));}}
        reveal();
    }
    std::string visible(size_t rowIndex) const {size_t start=0;for(size_t r=0;r<top+rowIndex;++r) {auto end=text.find('\n',start);if(end==std::string::npos)return "";start=end+1;}size_t end=lineEnd(start);return left>=end-start ? "" : text.substr(start+left,std::min(size_t(40),end-start-left));}
};
class SshProfiles {
public:
    std::array<SshConfig,8> entries;size_t count=0;
    bool remember(const SshConfig& config);
    bool forget(size_t index);
    bool load(const std::string& json);
    std::string serialize() const;
};
// Incremental Telnet parser. Negotiation never enters the terminal text buffer.
class TelnetDecoder {
    enum State {Data,Iac,Option,Sub,SubIac} state_=Data;
    uint8_t command_=0;
    std::array<bool,256> remote_{};
public:
    bool remoteEcho=false;
    template<class Output,class Reply> void feed(uint8_t byte,Output output,Reply reply) {
        switch(state_) {
        case Data: if(byte==255)state_=Iac;else output(byte);break;
        case Iac:
            if(byte==255) {output(byte);state_=Data;}
            else if(byte==250)state_=Sub;
            else if(byte>=251 && byte<=254) {command_=byte;state_=Option;}
            else state_=Data;
            break;
        case Option:
            if(command_==251) { // WILL: accept echo and suppress-go-ahead only.
                bool accept=byte==1 || byte==3;
                if(!remote_[byte]) {uint8_t response[]={255,uint8_t(accept?253:254),byte};reply(response,3);}
                remote_[byte]=accept;if(byte==1)remoteEcho=accept;
            } else if(command_==252) {remote_[byte]=false;if(byte==1)remoteEcho=false;}
            else if(command_==253) {uint8_t response[]={255,252,byte};reply(response,3);}
            state_=Data;break;
        case Sub: if(byte==255)state_=SubIac;break;
        case SubIac: state_=byte==240 ? Data : Sub;break;
        }
    }
};
} // namespace portalis
