#include <unity.h>
#include "../../src/ssh_terminal.cpp"
using namespace portalis;
SshTerminal* terminal=nullptr;
bool acceptHost=true, sent=false;
void tick() {
    fakeMillis+=20;terminal->poll();
    if(terminal->status().phase==SshPhase::VerifyHost) {
        TEST_ASSERT_EQUAL(0,fakeSshPasswordCalls+fakeSshKeyCalls);
        if(acceptHost) terminal->trustHost();else terminal->stop();
    }
    if(terminal->status().phase==SshPhase::Active && !sent) { TEST_ASSERT_TRUE(terminal->send("pwd\r",4));sent=true; }
}
void setUp() {
    WiFi={};WiFi.currentMode=WIFI_STA;WiFi.connectionStatus=WL_CONNECTED;SD.reset();
    fakeMillis=0;fakeTaskFail=false;fakeTaskTick=tick;fakeTask=nullptr;
    fakeSshFailConnect=fakeSshFailAuth=fakeSshFailKey=fakeSshFailRead=fakeSshFailShell=false;
    fakeSshPasswordCalls=fakeSshKeyCalls=fakeSshFree=fakeSshChannelsFree=0;
    fakeSshImportedKey.clear();fakeSshPassphrase.clear();fakeSshWritten.clear();acceptHost=true;sent=false;
}
void tearDown() {fakeTaskTick=nullptr;}
SshConfig config() { SshConfig value;value.host="192.168.1.10";value.user="tester";value.secret="example-only";return value; }
void test_password_requires_host_verification_and_shell_io() {
    SshTerminal client;terminal=&client;
    TEST_ASSERT_TRUE(client.start(config()));runFakeTask();client.poll();
    TEST_ASSERT_EQUAL(SshPhase::Closed,client.status().phase);TEST_ASSERT_FALSE(client.busy());
    TEST_ASSERT_EQUAL(1,fakeSshPasswordCalls);TEST_ASSERT_EQUAL(0,fakeSshKeyCalls);
    TEST_ASSERT_EQUAL_STRING("pwd\r",fakeSshWritten.c_str());
    TEST_ASSERT_EQUAL(40,fakeSshColumns);TEST_ASSERT_EQUAL(14,fakeSshRows);
    TEST_ASSERT_EQUAL_STRING_LEN("hello",client.terminal().line(0),5);
    TEST_ASSERT_EQUAL(1,fakeSshFree);TEST_ASSERT_EQUAL(1,fakeSshChannelsFree);
}
void test_reject_host_sends_no_credentials() {
    SshTerminal client;terminal=&client;acceptHost=false;
    TEST_ASSERT_TRUE(client.start(config()));runFakeTask();client.poll();
    TEST_ASSERT_FALSE(client.busy());TEST_ASSERT_EQUAL(0,fakeSshPasswordCalls+fakeSshKeyCalls);
    TEST_ASSERT_EQUAL(1,fakeSshFree);
}
void test_private_key_and_passphrase_from_sd() {
    SshTerminal client;terminal=&client;auto settings=config();settings.auth=SshAuth::PrivateKey;settings.secret="key-passphrase";
    SD.seed("/ssh/id_ed25519","-----BEGIN OPENSSH PRIVATE KEY-----\nfixture\n-----END OPENSSH PRIVATE KEY-----");
    TEST_ASSERT_TRUE(client.start(settings));runFakeTask();client.poll();
    TEST_ASSERT_EQUAL(1,fakeSshKeyCalls);TEST_ASSERT_EQUAL(0,fakeSshPasswordCalls);
    TEST_ASSERT_EQUAL_STRING("key-passphrase",fakeSshPassphrase.c_str());
    TEST_ASSERT_NOT_EQUAL(std::string::npos,fakeSshImportedKey.find("BEGIN OPENSSH PRIVATE KEY"));
    TEST_ASSERT_FALSE(client.busy());
}
void test_failures_and_cancel_release_resources() {
    for(int failure=0;failure<5;++failure) {
        setUp();SshTerminal client;terminal=&client;
        fakeSshFailConnect=failure==0;fakeSshFailAuth=failure==1;fakeSshFailRead=failure==2;fakeSshFailShell=failure==3;
        auto settings=config();if(failure==4) settings.auth=SshAuth::PrivateKey;
        TEST_ASSERT_TRUE(client.start(settings));runFakeTask();client.poll();
        TEST_ASSERT_EQUAL(SshPhase::Failed,client.status().phase);TEST_ASSERT_FALSE(client.busy());TEST_ASSERT_EQUAL(1,fakeSshFree);
    }
    setUp();SshTerminal client;terminal=&client;fakeTaskFail=true;
    TEST_ASSERT_FALSE(client.start(config()));TEST_ASSERT_FALSE(client.busy());
    fakeTaskFail=false;TEST_ASSERT_TRUE(client.start(config()));client.stop();runFakeTask();client.poll();TEST_ASSERT_FALSE(client.busy());
}
void test_terminal_control_sequences_and_bounded_history() {
    TerminalBuffer buffer;
    std::string output="hello\rX\x1b[2;3H!\x1b[31mred\x1b[0m";buffer.feed(output.data(),output.size());
    TEST_ASSERT_EQUAL('X',buffer.line(0)[0]);TEST_ASSERT_EQUAL('!',buffer.line(1)[2]);
    output="\x1b[2J\x1b[Hclean\x1b]0;hidden title\x07";buffer.feed(output.data(),output.size());
    TEST_ASSERT_EQUAL_STRING_LEN("clean",buffer.line(0),5);
    output="\x1b[999999;999999H!";buffer.feed(output.data(),output.size());TEST_ASSERT_EQUAL('!',buffer.line(13)[39]);
    buffer.reset();for(int i=0;i<200;++i) {output=std::to_string(i)+"\r\n";buffer.feed(output.data(),output.size());}
    TEST_ASSERT_EQUAL(96,buffer.historyCount());
    TEST_ASSERT_EQUAL_STRING_LEN("199",buffer.line(12),3);
    for(int row=0;row<14;++row) TEST_ASSERT_EQUAL(40,std::strlen(buffer.line(row,9999)));
}
void test_settings_validation_and_secret_erasure() {
    auto settings=config();TEST_ASSERT_TRUE(settings.valid());settings.host="host;command";TEST_ASSERT_FALSE(settings.valid());
    settings=config();settings.auth=SshAuth::PrivateKey;settings.keyPath="../key";TEST_ASSERT_FALSE(settings.valid());
    settings=config();settings.user="bad\nuser";TEST_ASSERT_FALSE(settings.valid());
    std::string secret="example";eraseSecret(secret);TEST_ASSERT_TRUE(secret.empty());
}
int main() {
    UNITY_BEGIN();RUN_TEST(test_password_requires_host_verification_and_shell_io);RUN_TEST(test_reject_host_sends_no_credentials);
    RUN_TEST(test_private_key_and_passphrase_from_sd);RUN_TEST(test_failures_and_cancel_release_resources);
    RUN_TEST(test_terminal_control_sequences_and_bounded_history);RUN_TEST(test_settings_validation_and_secret_erasure);
    return UNITY_END();
}
