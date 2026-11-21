#pragma once
#include <map>
#include <functional>
#include <string>
#include <cstdlib>
#include "WiFiClient.h"
class String:public std::string {
public:
    using std::string::string;
    String(const std::string& value):std::string(value){}
    String()=default;
    long toInt() const {return std::strtol(c_str(),nullptr,10);}
    void replace(const char* from,const char* to) {size_t at=0;while((at=find(from,at))!=npos){std::string::replace(at,std::strlen(from),to);at+=std::strlen(to);}}
    size_t write(uint8_t byte){push_back(char(byte));return 1;}
    size_t write(const uint8_t* bytes,size_t size){append(reinterpret_cast<const char*>(bytes),size);return size;}
};
constexpr int HTTP_GET=0,HTTP_POST=1;
enum HTTPUploadStatus {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED};
struct HTTPUpload {HTTPUploadStatus status=UPLOAD_FILE_START;String filename;uint8_t buf[2048]={};size_t currentSize=0,totalSize=0;};
class WebServer;
inline WebServer* fakeServer=nullptr;
class WebServer {
public:
    struct Route {std::function<void()> handler,upload;};
    std::map<std::string,Route> routes;
    std::map<std::string,String> args,headers,responseHeaders;
    String user,password,response,type;int code=0;size_t contentLength=0;HTTPUpload part;
    explicit WebServer(int) {fakeServer=this;}
    bool authenticate(const char* u,const char* p) {return user==u && password==p;}
    void requestAuthentication() {code=401;}
    void collectHeaders(const char**,size_t) {}
    String header(const char* name) {return headers[name];}
    String arg(const char* name) {return args[name];}
    void sendHeader(const char* name,const String& value) {responseHeaders[name]=value;}
    void send(int status,const char* mime,const String& text) {code=status;type=mime;response=text;}
    void setContentLength(size_t size) {contentLength=size;}
    WiFiClient client() {return {};}
    void on(const char* path,int,std::function<void()> handler,std::function<void()> upload={}) {routes[path]={handler,upload};}
    void onNotFound(std::function<void()>) {}
    HTTPUpload& upload() {return part;}
    void begin() {}void stop() {}void handleClient() {}
};
