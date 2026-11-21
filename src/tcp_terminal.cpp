#include "tcp_terminal.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>
namespace portalis {
bool TcpTerminal::emit(const Event& event) {while(xQueueSend(events_,&event,pdMS_TO_TICKS(20))!=pdTRUE)if(cancel_)return false;return true;}
bool TcpTerminal::start(const TcpConfig& config) {
    SshConfig validation;validation.host=config.host;validation.port=config.port;
    if(busy() || !validation.valid() || WiFi.status()!=WL_CONNECTED || (WiFi.getMode()&WIFI_MODE_AP))return false;
    if(!events_)events_=xQueueCreate(8,sizeof(Event));if(!input_)input_=xQueueCreate(64,sizeof(Input));
    if(!events_ || !input_)return false;xQueueReset(events_);xQueueReset(input_);
    config_=config;status_={};status_.phase=TcpPhase::Connecting;terminal_.reset();cancel_=false;worker_=true;pending_=true;
    if(xTaskCreate(task,"tcp-terminal",6144,this,1,nullptr)!=pdPASS) {worker_=false;pending_=false;status_.phase=TcpPhase::Failed;std::snprintf(status_.message,sizeof(status_.message),"Not enough memory");return false;}return true;
}
void TcpTerminal::task(void* arg) {
    auto* self=static_cast<TcpTerminal*>(arg);WiFiClient client;TelnetDecoder decoder;
    const char* failure=nullptr;client.setTimeout(1);
    if(self->cancel_) {} else if(!client.connect(self->config_.host.c_str(),self->config_.port,3000))failure="Connection failed";
    else {
        Event active;active.phase=TcpPhase::Active;self->emit(active);
        auto write=[&](const uint8_t* data,size_t size) {
            size_t offset=0;uint32_t start=millis();
            while(offset<size && !self->cancel_ && client.connected() && uint32_t(millis()-start)<3000) {
                size_t sent=client.write(data+offset,size-offset);offset+=sent;if(!sent)vTaskDelay(1);
            }
            if(offset!=size)failure="Write failed or timed out";
        };
        while(!self->cancel_ && WiFi.status()==WL_CONNECTED && (client.connected() || client.available())) {
            Input input;
            if(xQueueReceive(self->input_,&input,0)==pdTRUE) {
                uint8_t encoded[16];size_t count=0;
                for(size_t i=0;i<input.size;++i) {encoded[count++]=uint8_t(input.bytes[i]);if(self->config_.telnet && uint8_t(input.bytes[i])==255)encoded[count++]=255;}
                write(encoded,count);
                if(!self->config_.telnet || !decoder.remoteEcho) {Event echo;echo.size=input.size;std::memcpy(echo.bytes,input.bytes,input.size);self->emit(echo);}
            }
            Event output;
            for(int i=0;i<192 && client.available();++i) {
                int value=client.read();if(value<0)break;
                auto append=[&](uint8_t byte) {output.bytes[output.size++]=char(byte);};
                if(self->config_.telnet)decoder.feed(value,append,write);else append(value);
            }
            if(output.size)self->emit(output);
            if(failure)break;vTaskDelay(pdMS_TO_TICKS(5));
        }
        if(WiFi.status()!=WL_CONNECTED && !self->cancel_)failure="Wi-Fi disconnected";
    }
    client.stop();Event done;done.phase=failure && !self->cancel_ ? TcpPhase::Failed : TcpPhase::Closed;
    std::snprintf(done.bytes,sizeof(done.bytes),"%s",self->cancel_ ? "Disconnected" : failure ? failure : "Remote closed");self->emit(done);self->worker_=false;vTaskDelete(nullptr);
}
bool TcpTerminal::send(const char* data,size_t size) {if(status_.phase!=TcpPhase::Active || !size || size>8)return false;Input input;input.size=size;std::memcpy(input.bytes,data,size);return xQueueSend(input_,&input,0)==pdTRUE;}
void TcpTerminal::stop() {if(pending_) {cancel_=true;status_.phase=TcpPhase::Stopping;}}
void TcpTerminal::poll() {
    if(!pending_)return;bool done=!worker_.load();Event event;
    for(int i=0;i<8 && xQueueReceive(events_,&event,0)==pdTRUE;++i) {
        if(event.size)terminal_.feed(event.bytes,event.size);
        else if(!cancel_ || event.phase==TcpPhase::Closed || event.phase==TcpPhase::Failed) {status_.phase=event.phase;std::snprintf(status_.message,sizeof(status_.message),"%.79s",event.bytes);}
    }
    if(done) {pending_=false;if(cancel_) {status_.phase=TcpPhase::Closed;std::snprintf(status_.message,sizeof(status_.message),"Disconnected");}}
}
} // namespace portalis
