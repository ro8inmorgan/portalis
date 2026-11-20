#pragma once
#include <ping/ping_sock.h>
inline int ipaddr_aton(const char*,ip_addr_t* out) { out->value=1;return 1; }
