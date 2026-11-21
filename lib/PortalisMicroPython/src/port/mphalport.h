// Define so there's no dependency on extmod/virtpin.h
#define mp_hal_pin_obj_t

#include "py/misc.h"
int portalis_mp_readline(vstr_t*, const char*);
