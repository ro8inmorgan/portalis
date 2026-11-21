#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Runtime is single-threaded: one fresh interpreter per call, on the worker task.
// Returns 0: success, 1: Python exception, 2: cancelled.
int portalis_python_run(void* heap,size_t heap_size,size_t stack_limit,const char* path,const char* source,size_t size);
// Host callbacks always return before the C runtime performs non-local exits.
int portalis_python_poll(void);
int portalis_python_output(const char* data,size_t length);
int portalis_python_read_char(void);
void portalis_python_input_state(int waiting);
uint32_t portalis_python_ticks_ms(void);
void portalis_python_delay_ms(uint32_t millis);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
#include <string>
inline int fakePythonResult=0;
inline std::string fakePythonSource;
inline int portalis_python_run(void*,size_t,size_t,const char*,const char* source,size_t size) {
    fakePythonSource.assign(source,size);
    if(portalis_python_poll()) return 2;
    portalis_python_output("Hello\nworld\n",12);
    return fakePythonResult;
}
#endif
