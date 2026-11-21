#pragma once
#include "workbench_model.h"
#include <WebServer.h>
#include <SD.h>
#include <atomic>
namespace portalis {
inline std::string transferFolder(const std::string& group) {return group=="scripts"?"/scripts":group=="captures"?"/captures":group=="surveys"?"/surveys":group=="templates"?"/":"";}
inline bool transferName(const std::string& group,const std::string& name) {
    if(!simpleFileName(name))return false;auto dot=name.find_last_of('.');std::string ext=dot==std::string::npos?"":name.substr(dot);
    for(char& c:ext)if(c>='A'&&c<='Z')c+=32;
    return group=="scripts"?ext==".py":group=="captures"?ext==".pcap":group=="surveys"?ext==".csv":group=="templates"?(ext==".html"||ext==".htm"):false;
}
class FileTransfer {
    WebServer server_{80};File upload_;std::string temporary_,target_;size_t uploadSize_=0;
    bool uploadOk_=false,uploadDone_=false,overwrite_=false,pending_=false,configured_=false;
    std::atomic<bool> worker_{false},cancel_{false};std::atomic<uint32_t> uploads_{0},downloads_{0};
    TransferStatus status_;
    void configure();bool authorized();void list();void download();void uploadChunk();void discardUpload();
    static void task(void*);
public:
    bool start(const char* address);void stop();void poll();
    bool busy() const {return pending_;}
    const TransferStatus& status() const {return status_;}
};
} // namespace portalis
