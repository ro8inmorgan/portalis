#include "ssh_terminal.h"
#include <WiFi.h>
#include <SD.h>
#include <libssh_esp32.h>
#include <libssh/libssh.h>
#include <freertos/task.h>

namespace portalis {
bool SshTerminal::busy() const {
    return workerRunning_.load() || status_.phase==SshPhase::Connecting || status_.phase==SshPhase::VerifyHost ||
        status_.phase==SshPhase::Authenticating || status_.phase==SshPhase::Active || status_.phase==SshPhase::Stopping;
}
bool SshTerminal::start(const SshConfig& config) {
    if(busy() || !config.valid() || WiFi.status()!=WL_CONNECTED || (WiFi.getMode()&WIFI_MODE_AP)) return false;
    if(!events_) events_=xQueueCreate(8,sizeof(Event));
    if(!input_) input_=xQueueCreate(64,sizeof(Input));
    if(!events_ || !input_) return false;
    xQueueReset(events_);xQueueReset(input_);
    eraseSecret(config_.secret);config_=config;terminal_.reset();status_={};status_.phase=SshPhase::Connecting;
    cancel_.store(false);trust_.store(0);workerRunning_.store(true);
    // The library's ESP32 client example uses 50 KB; crypto must not run on the
    // 8 KB Arduino loop/UI stack. Allocation failure is reported without reboot.
    if(xTaskCreate(task,"portalis-ssh",51200,this,1,nullptr)!=pdPASS) {
        workerRunning_.store(false);eraseSecret(config_.secret);status_.phase=SshPhase::Failed;
        std::snprintf(status_.message,sizeof(status_.message),"Not enough memory for SSH");return false;
    }
    return true;
}
bool SshTerminal::emit(const Event& event) {
    while(xQueueSend(events_,&event,pdMS_TO_TICKS(20))!=pdTRUE) if(cancel_.load()) return false;
    return true;
}
void SshTerminal::state(SshPhase phase,const char* text) {
    Event event{Kind::State,phase};std::snprintf(event.text,sizeof(event.text),"%s",text);emit(event);
}
void SshTerminal::task(void* arg) {
    auto* self=static_cast<SshTerminal*>(arg);
    static bool initialized=false;
    if(!initialized) { libssh_begin();initialized=true; }
    ssh_session session=nullptr;ssh_channel channel=nullptr;ssh_key key=nullptr;
    const char* failure=nullptr;
    auto retry=[&](auto operation,int again=SSH_AGAIN) {
        const uint32_t began=millis();
        while(!self->cancel_.load() && WiFi.status()==WL_CONNECTED && uint32_t(millis()-began)<15000) {
            int rc=operation();if(rc!=again) return rc;vTaskDelay(pdMS_TO_TICKS(10));
        }
        return SSH_ERROR;
    };
    do {
        session=ssh_new();if(!session) { failure="SSH allocation failed";break; }
        long timeout=5;unsigned int port=self->config_.port;bool processConfig=false;
        if(ssh_options_set(session,SSH_OPTIONS_HOST,self->config_.host.c_str())!=SSH_OK ||
           ssh_options_set(session,SSH_OPTIONS_PORT,&port)!=SSH_OK ||
           ssh_options_set(session,SSH_OPTIONS_USER,self->config_.user.c_str())!=SSH_OK ||
           ssh_options_set(session,SSH_OPTIONS_TIMEOUT,&timeout)!=SSH_OK ||
           ssh_options_set(session,SSH_OPTIONS_PROCESS_CONFIG,&processConfig)!=SSH_OK) { failure="Invalid SSH options";break; }
        ssh_set_blocking(session,0);
        if(retry([&] {return ssh_connect(session);})!=SSH_OK) { failure="SSH connection failed or timed out";break; }
        ssh_key serverKey=nullptr;unsigned char* hash=nullptr;size_t length=0;
        if(ssh_get_server_publickey(session,&serverKey)!=SSH_OK) { failure="Could not read host key";break; }
        const int hashResult=ssh_get_publickey_hash(serverKey,SSH_PUBLICKEY_HASH_SHA256,&hash,&length);
        ssh_key_free(serverKey);
        if(hashResult!=SSH_OK || length!=32) { if(hash) ssh_clean_pubkey_hash(&hash);failure="Could not fingerprint host key";break; }
        Event fingerprint{Kind::Fingerprint,SshPhase::VerifyHost};
        for(size_t i=0;i<length;++i) std::snprintf(fingerprint.text+i*2,3,"%02x",hash[i]);
        ssh_clean_pubkey_hash(&hash);self->emit(fingerprint);
        const uint32_t verifyAt=millis();
        while(!self->trust_.load() && !self->cancel_.load() && WiFi.status()==WL_CONNECTED && uint32_t(millis()-verifyAt)<120000)
            vTaskDelay(pdMS_TO_TICKS(20));
        if(!self->trust_.load() || self->cancel_.load()) { failure="Host key not accepted";break; }
        self->state(SshPhase::Authenticating,"Authenticating");
        int auth;
        if(self->config_.auth==SshAuth::PrivateKey) {
            File file=SD.open(self->config_.keyPath.c_str());
            if(!file || file.isDirectory() || !file.size() || file.size()>16384) { failure="Private key missing or over 16 KB";break; }
            std::string pem(file.size(),'\0');const size_t read=file.readBytes(&pem[0],pem.size());file.close();
            const bool valid=read==pem.size() && pem.find('\0')==std::string::npos;
            int imported=valid ? ssh_pki_import_privkey_base64(pem.c_str(),self->config_.secret.empty()?nullptr:self->config_.secret.c_str(),nullptr,nullptr,&key) : SSH_ERROR;
            eraseSecret(pem);
            if(imported!=SSH_OK) { failure="Key format or passphrase rejected";break; }
            auth=retry([&] {return ssh_userauth_publickey(session,nullptr,key);},SSH_AUTH_AGAIN);
        } else auth=retry([&] {return ssh_userauth_password(session,nullptr,self->config_.secret.c_str());},SSH_AUTH_AGAIN);
        eraseSecret(self->config_.secret);
        if(auth!=SSH_AUTH_SUCCESS) { failure="Authentication failed";break; }
        channel=ssh_channel_new(session);if(!channel) { failure="SSH channel allocation failed";break; }
        if(retry([&] {return ssh_channel_open_session(channel);})!=SSH_OK ||
           retry([&] {return ssh_channel_request_pty_size(channel,"vt100",TerminalBuffer::columns,TerminalBuffer::rows);})!=SSH_OK ||
           retry([&] {return ssh_channel_request_shell(channel);})!=SSH_OK) { failure="Server refused terminal shell";break; }
        self->state(SshPhase::Active,"Connected");
        while(!self->cancel_.load() && WiFi.status()==WL_CONNECTED && ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel)) {
            Input input;
            if(xQueueReceive(self->input_,&input,0)==pdTRUE) {
                size_t offset=0;
                while(offset<input.length && !self->cancel_.load()) {
                    const int written=retry([&] { int rc=ssh_channel_write(channel,input.bytes+offset,input.length-offset);return rc==0 ? SSH_AGAIN : rc; });
                    if(written<0) { failure="SSH write failed";break; }
                    offset+=written;
                }
                if(failure) break;
            }
            for(int stream=0;stream<2;++stream) {
                Event output{Kind::Output};
                const int count=ssh_channel_read_nonblocking(channel,output.text,sizeof(output.text),stream);
                if(count>0) { output.length=count;self->emit(output); }
                else if(count==SSH_ERROR) { failure="SSH read failed";break; }
            }
            if(failure) break;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if(WiFi.status()!=WL_CONNECTED && !self->cancel_.load()) failure="Wi-Fi disconnected";
    } while(false);
    eraseSecret(self->config_.secret);
    if(key) ssh_key_free(key);
    if(channel) { ssh_channel_send_eof(channel);ssh_channel_close(channel);ssh_channel_free(channel); }
    if(session) { ssh_disconnect(session);ssh_free(session); }
    self->state(failure && !self->cancel_.load() ? SshPhase::Failed : SshPhase::Closed,
        self->cancel_.load() ? "Disconnected" : failure ? failure : "Remote session closed");
    self->workerRunning_.store(false);
    vTaskDelete(nullptr);
}
void SshTerminal::stop() { cancel_.store(true); if(busy()) status_.phase=SshPhase::Stopping; }
bool SshTerminal::send(const char* bytes,size_t size) {
    if(status_.phase!=SshPhase::Active || !size || size>8) return false;
    Input input;input.length=size;std::memcpy(input.bytes,bytes,size);
    return xQueueSend(input_,&input,0)==pdTRUE;
}
void SshTerminal::poll() {
    if(!events_) return;
    Event event;
    for(int i=0;i<8 && xQueueReceive(events_,&event,0)==pdTRUE;++i) {
        if(event.kind==Kind::Output) terminal_.feed(event.text,event.length);
        else if(event.kind==Kind::Fingerprint) {
            if(!cancel_.load()) { status_.phase=SshPhase::VerifyHost;std::snprintf(status_.fingerprint,sizeof(status_.fingerprint),"%.64s",event.text); }
        } else if(!cancel_.load() || event.phase==SshPhase::Closed || event.phase==SshPhase::Failed) {
            status_.phase=event.phase;std::snprintf(status_.message,sizeof(status_.message),"%.95s",event.text);
        }
    }
    // A full output queue during cancellation may drop the final status event.
    if(cancel_.load() && !workerRunning_.load()) { status_.phase=SshPhase::Closed;std::snprintf(status_.message,sizeof(status_.message),"Disconnected"); }
}
} // namespace portalis
