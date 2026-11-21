#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
constexpr int SSH_OK=0,SSH_ERROR=-1,SSH_AGAIN=-2,SSH_AUTH_SUCCESS=0,SSH_AUTH_AGAIN=4;
enum { SSH_OPTIONS_HOST,SSH_OPTIONS_PORT,SSH_OPTIONS_USER,SSH_OPTIONS_TIMEOUT,SSH_OPTIONS_PROCESS_CONFIG,SSH_PUBLICKEY_HASH_SHA256 };
struct FakeSshSession { bool connected=false; };
struct FakeSshChannel { int reads=0;bool eof=false; };
using ssh_session=FakeSshSession*;using ssh_channel=FakeSshChannel*;using ssh_key=int*;
inline bool fakeSshFailConnect=false,fakeSshFailAuth=false,fakeSshFailKey=false,fakeSshFailRead=false;
inline bool fakeSshFailShell=false;
inline int fakeSshPasswordCalls=0,fakeSshKeyCalls=0,fakeSshFree=0,fakeSshChannelsFree=0;
inline int fakeSshColumns=0,fakeSshRows=0;
inline std::string fakeSshImportedKey,fakeSshPassphrase,fakeSshWritten;
inline ssh_session ssh_new() { return new FakeSshSession; }
inline int ssh_options_set(ssh_session,int,const void*) {return 0;}
inline void ssh_set_blocking(ssh_session,int) {}
inline int ssh_connect(ssh_session s) {s->connected=!fakeSshFailConnect;return fakeSshFailConnect ? -1 : 0;}
inline int ssh_get_server_publickey(ssh_session,ssh_key* key) {*key=new int(1);return 0;}
inline int ssh_get_publickey_hash(ssh_key,int,unsigned char** hash,size_t* size) {*size=32;*hash=static_cast<unsigned char*>(std::malloc(32));std::memset(*hash,0xab,32);return 0;}
inline void ssh_key_free(ssh_key key) {delete key;}
inline void ssh_clean_pubkey_hash(unsigned char** hash) {std::free(*hash);*hash=nullptr;}
inline int ssh_userauth_password(ssh_session,const char*,const char*) {++fakeSshPasswordCalls;return fakeSshFailAuth ? -1 : 0;}
inline int ssh_pki_import_privkey_base64(const char* pem,const char* passphrase,void*,void*,ssh_key* key) {
    fakeSshImportedKey=pem;fakeSshPassphrase=passphrase ? passphrase : "";
    if(fakeSshFailKey) return -1;*key=new int(2);return 0;
}
inline int ssh_userauth_publickey(ssh_session,const char*,ssh_key) {++fakeSshKeyCalls;return fakeSshFailAuth ? -1 : 0;}
inline ssh_channel ssh_channel_new(ssh_session) {return new FakeSshChannel;}
inline int ssh_channel_open_session(ssh_channel) {return 0;}
inline int ssh_channel_request_pty_size(ssh_channel,const char*,int columns,int rows) {fakeSshColumns=columns;fakeSshRows=rows;return 0;}
inline int ssh_channel_request_shell(ssh_channel) {return fakeSshFailShell ? -1 : 0;}
inline int ssh_channel_is_open(ssh_channel c) {return !c->eof;}
inline int ssh_channel_is_eof(ssh_channel c) {return c->eof;}
inline int ssh_channel_write(ssh_channel,const void* bytes,uint32_t size) {
    const int written=std::min(size,2U);fakeSshWritten.append(static_cast<const char*>(bytes),written);return written;
}
inline int ssh_channel_read_nonblocking(ssh_channel c,void* bytes,uint32_t,int stream) {
    if(fakeSshFailRead) return SSH_ERROR;
    if(stream) return 0;
    if(c->reads++==0) {std::memcpy(bytes,"hello\r\n$ ",9);return 9;}
    if(c->reads>4) c->eof=true;
    return 0;
}
inline int ssh_channel_send_eof(ssh_channel) {return 0;}
inline int ssh_channel_close(ssh_channel) {return 0;}
inline void ssh_channel_free(ssh_channel c) {++fakeSshChannelsFree;delete c;}
inline void ssh_disconnect(ssh_session s) {s->connected=false;}
inline void ssh_free(ssh_session s) {++fakeSshFree;delete s;}
