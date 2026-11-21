#pragma once
#include "workbench_model.h"
#include <WebServer.h>
#include <SD.h>
#include <atomic>
namespace portalis {
inline constexpr size_t kTransferUploadLimit=32*1024*1024;
inline bool transferName(const std::string& name) {
    if(name.empty() || name=="." || name==".." || name.back()=='.' || name.back()==' ')return false;
    for(unsigned char c:name)if(c<32 || c==127 || std::string("/\\:\"<>|?*").find(c)!=std::string::npos)return false;
    return true;
}
inline bool transferPath(const std::string& path) {
    if(path.empty() || path.front()!='/' || path.size()>240)return false;
    if(path=="/")return true;
    size_t start=1;
    while(start<path.size()) {auto end=path.find('/',start);if(end==std::string::npos)end=path.size();if(!transferName(path.substr(start,end-start)))return false;start=end+1;}
    return path.back()!='/';
}
inline std::string transferChild(const std::string& folder,const std::string& name) {return folder+(folder=="/"?"":"/")+name;}
class FileTransfer {
    WebServer server_{80};File upload_;std::string temporary_,target_;size_t uploadSize_=0;
    bool uploadOk_=false,uploadDone_=false,overwrite_=false,uploadConflict_=false,pending_=false,configured_=false;
    std::atomic<bool> worker_{false},cancel_{false};std::atomic<uint32_t> uploads_{0},downloads_{0};
    TransferStatus status_;
    void configure();bool authorized();bool mutationAllowed();void mutate(const char* action);bool removeTree(const std::string& path,unsigned depth=0);void list();void download();void uploadChunk();void discardUpload();
    static void task(void*);
public:
    bool start(const char* address);void stop();void poll();
    bool busy() const {return pending_;}
    const TransferStatus& status() const {return status_;}
};
} // namespace portalis
