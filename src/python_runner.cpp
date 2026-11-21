#include "python_runner.h"
#include <portalis_python.h>
#include <SD.h>
#include <WiFi.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
namespace { portalis::PythonRunner* activePython=nullptr; }
extern "C" int portalis_python_poll(void) {return activePython->checkpoint();}
extern "C" int portalis_python_output(const char* bytes,size_t length) {return activePython->output(bytes,length);}
extern "C" int portalis_python_read_char(void) {return activePython->readChar();}
extern "C" void portalis_python_input_state(int waiting) {activePython->inputState(waiting);}
extern "C" uint32_t portalis_python_ticks_ms(void) {return millis();}
extern "C" void portalis_python_delay_ms(uint32_t duration) {vTaskDelay(std::max(uint32_t(1),uint32_t(pdMS_TO_TICKS(duration))));}
namespace portalis {
bool PythonRunner::refresh() {
    if(busy()) return false;
    scripts_={};
    auto fail=[&](const char* message) { std::snprintf(scripts_.message,sizeof(scripts_.message),"%s",message);return false; };
    if(SD.cardType()==CARD_NONE) return fail("Insert SD and press R");
    File dir=SD.open("/scripts");
    if(!dir || !dir.isDirectory()) return fail("Put .py files in /scripts");
    for(File file=dir.openNextFile();file;file=dir.openNextFile()) {
        std::string name=file.name();auto slash=name.find_last_of('/');if(slash!=std::string::npos) name.erase(0,slash+1);
        if(!file.isDirectory() && scriptName(name)) {
            // Keep the alphabetically first 64 independent of FAT directory order.
            if(scripts_.count<scripts_.names.size()) scripts_.names[scripts_.count++]=name;
            else {scripts_.limited=true;if(name<scripts_.names[scripts_.count-1]) scripts_.names[scripts_.count-1]=name;}
            std::sort(scripts_.names.begin(),scripts_.names.begin()+scripts_.count);
        }
        file.close();
    }
    dir.close();
    if(!scripts_.count) return fail("Put .py files in /scripts");
    return true;
}
void PythonRunner::release() {
    if(output_) {vQueueDelete(output_);output_=nullptr;}
    if(input_) {vQueueDelete(input_);input_=nullptr;}
    free(heap_);heap_=nullptr;free(source_);source_=nullptr;sourceSize_=0;pending_=false;
}
bool PythonRunner::start(const std::string& name) {
    if(busy()) return false;
    status_={};terminal_.reset();
    std::snprintf(status_.name,sizeof(status_.name),"%.80s",name.c_str());
    auto fail=[&](const char* message) {release();status_.phase=PythonPhase::Failed;std::snprintf(status_.message,sizeof(status_.message),"%s",message);return false;};
    if(!scriptName(name)) return fail("Choose a .py file from /scripts");
    if(WiFi.getMode()!=WIFI_OFF) return fail("Stop the active Wi-Fi tool first");
    path_="/scripts/"+name;
    File file=SD.open(path_.c_str());
    if(!file || file.isDirectory() || file.size()>scriptLimit) return fail("Script missing or over 32 KB");
    sourceSize_=file.size();source_=static_cast<char*>(malloc(sourceSize_+1));
    if(!source_) {file.close();return fail("Not enough memory for source");}
    const size_t read=file.readBytes(source_,sourceSize_);file.close();source_[sourceSize_]=0;
    if(read!=sourceSize_ || std::memchr(source_,0,sourceSize_)) return fail("Could not read Python text");
    heap_=malloc(heapSize);output_=xQueueCreate(8,sizeof(Output));input_=xQueueCreate(64,sizeof(char));
    if(!heap_ || !output_ || !input_) return fail("Not enough memory for Python");
    result_=0;cancel_=false;waiting_=false;running_=true;pending_=true;yielded_=millis();
    status_.phase=PythonPhase::Running;
    if(xTaskCreate(task,"portalis-python",16*1024,this,1,nullptr)!=pdPASS) {running_=false;return fail("Not enough memory for Python task");}
    return true;
}
void PythonRunner::task(void* arg) {
    auto* self=static_cast<PythonRunner*>(arg);activePython=self;
    self->result_=portalis_python_run(self->heap_,heapSize,12*1024,self->path_.c_str(),self->source_,self->sourceSize_);
    self->waiting_=false;activePython=nullptr;self->running_=false;
    vTaskDelete(nullptr);
}
bool PythonRunner::checkpoint() {
    if(cancel_.load()) return true;
    const uint32_t now=millis();
    if(uint32_t(now-yielded_)>=5) {vTaskDelay(1);yielded_=millis();}
    return cancel_.load();
}
bool PythonRunner::output(const char* bytes,size_t length) {
    Output chunk;
    auto send=[&]() {
        while(xQueueSend(output_,&chunk,pdMS_TO_TICKS(20))!=pdTRUE) if(cancel_.load()) return false;
        chunk.size=0;return !cancel_.load();
    };
    for(size_t i=0;i<length;++i) {
        if(chunk.size+2>sizeof(chunk.bytes) && !send()) return true;
        if(bytes[i]=='\n') chunk.bytes[chunk.size++]='\r';
        chunk.bytes[chunk.size++]=bytes[i];
    }
    if(chunk.size && !send()) return true;
    return checkpoint();
}
int PythonRunner::readChar() {
    char byte;
    while(!cancel_.load()) {
        if(xQueueReceive(input_,&byte,pdMS_TO_TICKS(20))==pdTRUE) return static_cast<unsigned char>(byte);
        vTaskDelay(1);
    }
    return -1;
}
bool PythonRunner::send(char byte) { return pending_ && waiting_.load() && xQueueSend(input_,&byte,0)==pdTRUE; }
void PythonRunner::stop() { if(pending_) {cancel_=true;status_.phase=PythonPhase::Stopping;} }
void PythonRunner::poll() {
    if(!pending_) return;
    const bool done=!running_.load();
    Output output;
    for(int i=0;i<8 && xQueueReceive(output_,&output,0)==pdTRUE;++i) terminal_.feed(output.bytes,output.size);
    status_.input=waiting_;
    if(done) {
        const int result=result_.load();release();status_.input=false;
        status_.phase=result==2 ? PythonPhase::Stopped : result ? PythonPhase::Failed : PythonPhase::Done;
        std::snprintf(status_.message,sizeof(status_.message),"%s",result==2 ? "Stopped" : result ? "Python exception" : "Finished");
    }
}
} // namespace portalis
