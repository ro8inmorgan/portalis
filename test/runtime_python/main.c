#include "portalis_python.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static char output[32768];static size_t length;
static unsigned polls,stop_after,ticks;static const char* input;
int portalis_python_poll(void) { return stop_after && ++polls>=stop_after; }
int portalis_python_output(const char* text,size_t count) {
    if(length+count>=sizeof(output)) return 1;
    memcpy(output+length,text,count);length+=count;output[length]=0;return portalis_python_poll();
}
int portalis_python_read_char(void) { return *input ? *input++ : -1; }
void portalis_python_input_state(int waiting) { (void)waiting; }
uint32_t portalis_python_ticks_ms(void) {return ticks;}
void portalis_python_delay_ms(uint32_t count) {ticks+=count;}
static int run(const char* source,unsigned cancel) {
    void* heap=malloc(65536);assert(heap);length=0;output[0]=0;polls=0;stop_after=cancel;input="Ada\r";
    int rc=portalis_python_run(heap,65536,12*1024,"/scripts/test.py",source,strlen(source));
    free(heap);return rc;
}
int main(int argc,char** argv) {
    assert(run("import math, gc, time\nprint(sum(x*x for x in range(10)))\nprint(math.sqrt(81))\ntime.sleep_ms(25)\nprint(input('Name? '))\ngc.collect()\nprint(__file__)\n",0)==0);
    assert(strstr(output,"285") && strstr(output,"9.0") && strstr(output,"Ada") && strstr(output,"/scripts/test.py"));
    assert(run("def broken(:\n pass",0)==1 && strstr(output,"SyntaxError"));
    assert(run("print(1/0)",0)==1 && strstr(output,"ZeroDivisionError") && strstr(output,"test.py"));
    assert(run("a = 'x' * 1000000",0)==1 && strstr(output,"MemoryError"));
    assert(run("def deep():\n return deep()\ndeep()",0)==1 && strstr(output,"RuntimeError"));
    assert(run("while True:\n try:\n  while True: pass\n except BaseException: pass\n",100)==2);
    assert(run("while True: print('output')",100)==2);
    assert(run("import time\ntime.sleep(100)",10)==2);
    assert(run("class Box:\n def __init__(self): self.data = list(range(100))\nimport gc\nfor i in range(1000):\n b=Box()\n gc.collect()\n assert b.data[99]==99\nprint('gc ok')",0)==0 && strstr(output,"gc ok"));
    assert(run("print('fresh run')",0)==0 && strstr(output,"fresh run"));
    assert(run("print(b)",0)==1 && strstr(output,"NameError"));
    assert(run("class Bad(Exception):\n def __str__(self): raise ValueError('format failed')\nraise Bad()",0)==1);
    for(int i=1;i<argc;++i) {
        FILE* file=fopen(argv[i],"rb");assert(file);
        char script[32769];size_t size=fread(script,1,sizeof(script)-1,file);assert(!ferror(file));fclose(file);script[size]=0;
        assert(run(script,0)==0);
    }
    printf("12 real MicroPython runtime checks and %d sample scripts passed\n",argc-1);return 0;
}
