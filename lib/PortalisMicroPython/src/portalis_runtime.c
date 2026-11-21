#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include "portalis_python.h"
#include "py/compile.h"
#include "py/builtin.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "shared/runtime/gchelper.h"

static jmp_buf cancel_jump;
static void cancel_run(void) { longjmp(cancel_jump,1); }
void portalis_mp_hook(void) { if(portalis_python_poll()) cancel_run(); }
#if defined(__xtensa__)
#include <xtensa/hal.h>
#include <xtensa/config/core-isa.h>
// As in MicroPython's ESP32 port: spill the Xtensa register windows before
// tracing the task stack. The generic setjmp collector alone is insufficient.
static MP_NOINLINE void collect_xtensa(volatile unsigned level) {
    if(level<XCHAL_NUM_AREGS/8) {
        collect_xtensa(level+1);
        __asm__ volatile ("" ::: "memory");
    } else {
        uintptr_t sp;__asm__ volatile ("mov %0, a1" : "=r" (sp));
        gc_collect_root((void**)sp,((uintptr_t)MP_STATE_THREAD(stack_top)-sp)/sizeof(uintptr_t));
    }
}
#endif
void gc_collect(void) {
    gc_collect_start();
    #if defined(__xtensa__)
    collect_xtensa(0);
    #else
    gc_helper_collect_regs_and_stack();
    #endif
    gc_collect_end();
}
void nlr_jump_fail(void* value) { (void)value;abort(); }
void mp_hal_stdout_tx_strn_cooked(const char* str,size_t len) {
    if(portalis_python_output(str,len)) cancel_run();
}
int portalis_mp_readline(vstr_t* line,const char* prompt) {
    (void)prompt;
    portalis_python_input_state(1);
    for(;;) {
        int ch=portalis_python_read_char();
        if(ch<0) cancel_run();
        if(ch=='\r' || ch=='\n') { mp_hal_stdout_tx_strn_cooked("\n",1);break; }
        if(ch==8 || ch==127) {
            if(line->len) {vstr_cut_tail_bytes(line,1);mp_hal_stdout_tx_strn_cooked("\b \b",3);}
        } else if(ch>=32 && ch<127 && line->len<255) {
            vstr_add_char(line,ch);char byte=ch;mp_hal_stdout_tx_strn_cooked(&byte,1);
        }
    }
    portalis_python_input_state(0);
    return 0;
}
// This first embedded port has built-in modules, not a filesystem/VFS binding.
mp_import_stat_t mp_import_stat(const char* path) { (void)path;return MP_IMPORT_STAT_NO_EXIST; }
mp_lexer_t* mp_lexer_new_from_file(qstr name) { (void)name;mp_raise_OSError(2); }

static mp_obj_t py_ticks_ms(void) { return mp_obj_new_int_from_uint(portalis_python_ticks_ms()); }
static MP_DEFINE_CONST_FUN_OBJ_0(ticks_ms_obj,py_ticks_ms);
static void sleep_ms(uint32_t duration) {
    uint32_t start=portalis_python_ticks_ms();
    while((uint32_t)(portalis_python_ticks_ms()-start)<duration) {
        portalis_mp_hook();
        uint32_t left=duration-(uint32_t)(portalis_python_ticks_ms()-start);
        portalis_python_delay_ms(left>10 ? 10 : left);
    }
    portalis_mp_hook();
}
static mp_obj_t py_sleep_ms(mp_obj_t arg) {
    mp_int_t value=mp_obj_get_int(arg);
    if(value<0) mp_raise_ValueError(MP_ERROR_TEXT("negative sleep"));
    sleep_ms(value);return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(sleep_ms_obj,py_sleep_ms);
static mp_obj_t py_sleep(mp_obj_t arg) {
    mp_float_t value=mp_obj_get_float(arg);
    if(!(value>=0 && value<=86400)) mp_raise_ValueError(MP_ERROR_TEXT("sleep: 0..86400 seconds"));
    sleep_ms((uint32_t)(value*1000));return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(sleep_obj,py_sleep);
static const mp_rom_map_elem_t time_globals_table[]={
    {MP_ROM_QSTR(MP_QSTR___name__),MP_ROM_QSTR(MP_QSTR_time)},
    {MP_ROM_QSTR(MP_QSTR_sleep),MP_ROM_PTR(&sleep_obj)},
    {MP_ROM_QSTR(MP_QSTR_sleep_ms),MP_ROM_PTR(&sleep_ms_obj)},
    {MP_ROM_QSTR(MP_QSTR_ticks_ms),MP_ROM_PTR(&ticks_ms_obj)},
};
static MP_DEFINE_CONST_DICT(time_globals,time_globals_table);
const mp_obj_module_t portalis_time_module={.base={&mp_type_module},.globals=(mp_obj_dict_t*)&time_globals};
MP_REGISTER_MODULE(MP_QSTR_time,portalis_time_module);

int portalis_python_run(void* heap,size_t heap_size,size_t stack_limit,const char* path,const char* source,size_t size) {
    volatile int result=0;
    int stack_top;
    mp_stack_set_top(&stack_top);mp_stack_set_limit(stack_limit);
    gc_init(heap,(char*)heap+heap_size);
    if(setjmp(cancel_jump)==0) {
        nlr_buf_t nlr;
        if(nlr_push(&nlr)==0) {
            mp_init();
            mp_store_global(MP_QSTR___file__,mp_obj_new_str(path,strlen(path)));
            mp_lexer_t* lexer=mp_lexer_new_from_str_len(qstr_from_str(path),source,size,0);
            qstr name=lexer->source_name;
            mp_parse_tree_t tree=mp_parse(lexer,MP_PARSE_FILE_INPUT);
            mp_obj_t module=mp_compile(&tree,name,false);
            mp_call_function_0(module);
            nlr_pop();
        } else {
            result=1;
            nlr_buf_t printer;
            if(nlr_push(&printer)==0) {
                mp_obj_print_exception(&mp_plat_print,(mp_obj_t)nlr.ret_val);nlr_pop();
            } else mp_hal_stdout_tx_strn_cooked("Exception while formatting traceback\n",37);
        }
    } else result=2;
    // Cancellation bypasses Python handlers intentionally. No C++ objects are
    // unwound and no external resources belong to this VM; discard its heap.
    MP_STATE_THREAD(nlr_top)=NULL;
    MP_STATE_THREAD(nlr_jump_callback_top)=NULL;
    mp_deinit();
    portalis_python_input_state(0);
    return result;
}
