#pragma once
#include "python_model.h"
#include "ssh_model.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace portalis {
class PythonRunner {
    struct Output { size_t size=0;char bytes[128]; };
    ScriptCatalog scripts_;
    PythonStatus status_;
    TerminalBuffer terminal_;
    std::string path_;
    char* source_=nullptr;
    size_t sourceSize_=0;
    QueueHandle_t output_=nullptr,input_=nullptr;
    void* heap_=nullptr;
    bool pending_=false;
    std::atomic<bool> running_{false},cancel_{false},waiting_{false};
    std::atomic<int> result_{0};
    uint32_t yielded_=0;
    static void task(void*);
    void release();
public:
    static constexpr size_t heapSize=64*1024,scriptLimit=32*1024;
    bool refresh();
    bool start(const std::string& name);
    void stop();
    void poll();
    bool send(char ch);
    bool busy() const { return pending_; }
    const ScriptCatalog& scripts() const {return scripts_;}
    const PythonStatus& status() const {return status_;}
    const TerminalBuffer& terminal() const {return terminal_;}
    // C-runtime bridge: only the interpreter task invokes these methods.
    bool checkpoint();
    bool output(const char*,size_t);
    int readChar();
    void inputState(bool waiting) {waiting_=waiting;}
};
} // namespace portalis
