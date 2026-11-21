#include <unity.h>
#include "../../src/sd_workbench.cpp"
#include "../../src/ssh_profiles.cpp"
#include "../../src/tcp_terminal.cpp"
#include "../../src/file_transfer.cpp"
using namespace portalis;
void setUp() {SD.reset();fakeWriteLimit=static_cast<size_t>(-1);WiFi={};fakeMillis=0;fakeTaskFail=false;fakeTaskTick=nullptr;fakeRandom=0;fakeTcpSuccess=true;fakeTcpConnected=false;fakeTcpRead.clear();fakeTcpWritten.clear();fakeTcpWriteLimit=static_cast<size_t>(-1);}
void tearDown() {}
void test_editor_navigation_indent_and_safe_save() {
    ScriptEditor editor;TEST_ASSERT_TRUE(loadEditor(editor,"hello.py",true));
    editor.insert("if True:");editor.newline();editor.insert("    print(1)");editor.newline();TEST_ASSERT_EQUAL_STRING("if True:\n    print(1)\n    ",editor.text.c_str());
    editor.move(0,-1);TEST_ASSERT_EQUAL(4,editor.column());editor.backspace();TEST_ASSERT_TRUE(editor.dirty);
    TEST_ASSERT_TRUE(saveEditor(editor));TEST_ASSERT_FALSE(editor.dirty);
    std::string old=SD.nodes.at("/scripts/hello.py")->content;editor.insert("x");fakeWriteLimit=2;TEST_ASSERT_FALSE(saveEditor(editor));TEST_ASSERT_EQUAL_STRING(old.c_str(),SD.nodes.at("/scripts/hello.py")->content.c_str());fakeWriteLimit=static_cast<size_t>(-1);
    TEST_ASSERT_TRUE(saveEditor(editor));TEST_ASSERT_FALSE(SD.exists("/scripts/hello.py.bak"));
    SD.rejectRenameTo="/scripts/hello.py";editor.insert("y");TEST_ASSERT_FALSE(saveEditor(editor));TEST_ASSERT_TRUE(SD.exists("/scripts/hello.py.bak"));
    TEST_ASSERT_FALSE(loadEditor(editor,"../x.py",true));
    editor.reset("x.py",std::string(8192,'a'));TEST_ASSERT_FALSE(editor.insert("b"));
}
void test_editor_long_lines_and_normalization() {
    ScriptEditor editor;SD.seed("/scripts/tabs.py","if 1:\r\n\tprint(1)\r\n");TEST_ASSERT_TRUE(loadEditor(editor,"tabs.py",false));TEST_ASSERT_TRUE(editor.dirty);TEST_ASSERT_EQUAL_STRING("if 1:\n    print(1)\n",editor.text.c_str());
    editor.reset("x.py","");for(int i=0;i<25;++i)editor.insert(std::string(80,'x')+"\n");editor.move(0,-1);TEST_ASSERT_TRUE(editor.top>0);
    editor.move(1,0);TEST_ASSERT_LESS_OR_EQUAL(40,editor.visible(0).size());
    SD.seed("/scripts/unicode.py",std::string("\xc3\xa9"));TEST_ASSERT_FALSE(loadEditor(editor,"unicode.py",false));
}
void test_survey_csv_generations_failure_and_stop() {
    SurveyRecorder survey;TEST_ASSERT_TRUE(survey.start(100,0));RadarSnapshot snapshot;snapshot.generation=1;snapshot.count=1;snapshot.total=5;
    auto& ap=snapshot.points[0];ap.id=0x020304050607ULL;std::strcpy(ap.ssid,"Lab, \"WiFi\"");ap.channel=6;ap.rssi=-42;std::strcpy(ap.security,"WPA2");
    survey.observe(snapshot,2100);survey.observe(snapshot,2200);TEST_ASSERT_EQUAL(1,survey.status().sweeps);TEST_ASSERT_EQUAL(1,survey.status().rows);
    snapshot.generation=2;snapshot.failed=true;survey.observe(snapshot,3100);survey.stop();
    const auto data=SD.nodes.at(survey.status().path)->content;
    TEST_ASSERT_NOT_EQUAL(std::string::npos,data.find("\"Lab, \"\"WiFi\"\"\",02:03:04:05:06:07,6,-42"));TEST_ASSERT_NOT_EQUAL(std::string::npos,data.find("failed,0"));TEST_ASSERT_EQUAL(1,survey.status().missed);TEST_ASSERT_FALSE(survey.status().recording);
    TEST_ASSERT_TRUE(survey.start(4000,2));fakeWriteLimit=1;snapshot.generation=3;survey.observe(snapshot,5000);TEST_ASSERT_TRUE(survey.status().failed);TEST_ASSERT_FALSE(survey.status().recording);
}
void test_ssh_profiles_never_persist_secrets() {
    SshProfiles profiles;SshConfig config;config.host="192.168.1.10";config.user="tester";config.secret="do-not-save";config.auth=SshAuth::PrivateKey;config.keyPath="/ssh/id_ed25519";
    TEST_ASSERT_TRUE(profiles.remember(config));TEST_ASSERT_TRUE(profiles.entries[0].secret.empty());auto json=profiles.serialize();TEST_ASSERT_EQUAL(std::string::npos,json.find("do-not-save"));
    SshProfiles restored;TEST_ASSERT_TRUE(restored.load(json));TEST_ASSERT_EQUAL_STRING(config.host.c_str(),restored.entries[0].host.c_str());TEST_ASSERT_EQUAL(SshAuth::PrivateKey,restored.entries[0].auth);
    for(int i=0;i<10;++i) {config.host="host"+std::to_string(i);profiles.remember(config);}TEST_ASSERT_EQUAL(8,profiles.count);TEST_ASSERT_TRUE(profiles.forget(0));TEST_ASSERT_EQUAL(7,profiles.count);
    TEST_ASSERT_FALSE(restored.load("[{\"host\":\"bad\",\"port\":99999}]"));TEST_ASSERT_EQUAL(0,restored.count);
}
void test_telnet_fragmented_negotiation_and_raw_data() {
    TelnetDecoder decoder;std::string data,reply;
    auto output=[&](uint8_t c){data+=char(c);};auto send=[&](const uint8_t* p,size_t n){reply.append(reinterpret_cast<const char*>(p),n);};
    for(uint8_t byte:{255,251,1,255,251,1,255,253,31,72,105,255,250,24,1,255,240,255,255})decoder.feed(byte,output,send);
    TEST_ASSERT_TRUE(decoder.remoteEcho);TEST_ASSERT_EQUAL(6,reply.size());TEST_ASSERT_EQUAL(253,uint8_t(reply[1]));TEST_ASSERT_EQUAL(252,uint8_t(reply[4]));TEST_ASSERT_EQUAL_STRING_LEN("Hi",data.c_str(),2);TEST_ASSERT_EQUAL(255,uint8_t(data.back()));
}
void test_tcp_transport_output_input_partial_write_and_close() {
    WiFi.currentMode=WIFI_STA;WiFi.connectionStatus=WL_CONNECTED;TcpTerminal terminal;TcpConfig config;config.host="192.168.1.2";config.telnet=false;
    fakeTcpRead={'O','K','\r','\n'};fakeTcpWriteLimit=1;TEST_ASSERT_TRUE(terminal.start(config));
    static TcpTerminal* current;current=&terminal;static bool sent;sent=false;
    fakeTaskTick=[] {fakeMillis+=10;current->poll();if(!sent && current->status().phase==TcpPhase::Active) {current->send("hi\r\n",4);sent=true;}else if(sent && fakeTcpWritten.size()>=4)current->stop();};
    runFakeTask();terminal.poll();TEST_ASSERT_FALSE(terminal.busy());TEST_ASSERT_EQUAL_STRING("hi\r\n",fakeTcpWritten.c_str());TEST_ASSERT_EQUAL_STRING_LEN("OK",terminal.terminal().line(0),2);
    fakeTaskTick=nullptr;fakeTcpSuccess=false;TEST_ASSERT_TRUE(terminal.start(config));runFakeTask();terminal.poll();TEST_ASSERT_EQUAL(TcpPhase::Failed,terminal.status().phase);
}
void upload(const char* name,const char* text,bool token=true,bool replace=false) {
    auto& server=*fakeServer;server.args["path"]="/scripts";server.args["replace"]=replace?"1":"0";server.headers["X-Portalis-Token"]=token?server.password:String("wrong");
    server.part.filename=name;server.part.status=UPLOAD_FILE_START;server.routes["/upload"].upload();
    server.part.status=UPLOAD_FILE_WRITE;server.part.currentSize=std::strlen(text);std::memcpy(server.part.buf,text,server.part.currentSize);server.routes["/upload"].upload();
    server.part.status=UPLOAD_FILE_END;server.routes["/upload"].upload();server.routes["/upload"].handler();
}
void test_transfer_auth_paths_upload_and_overwrite() {
    WiFi.currentMode=WIFI_STA;WiFi.connectionStatus=WL_CONNECTED;FileTransfer transfer;SD.mkdir("/scripts");TEST_ASSERT_TRUE(transfer.start("192.168.1.42"));auto& server=*fakeServer;
    TEST_ASSERT_EQUAL(5,std::strlen(transfer.status().code));
    TEST_ASSERT_EQUAL(5,std::strspn(transfer.status().code,"23456789ABCDEFGHJKLMNPQRSTUVWXYZ"));
    server.routes["/"].handler();TEST_ASSERT_EQUAL(401,server.code);
    server.user="portalis";server.password=transfer.status().code;server.routes["/"].handler();TEST_ASSERT_EQUAL(200,server.code);TEST_ASSERT_EQUAL(std::string::npos,server.response.find("@@TOKEN@@"));
    upload("test.py","print(1)");TEST_ASSERT_EQUAL(200,server.code);TEST_ASSERT_EQUAL_STRING("print(1)",SD.nodes.at("/scripts/test.py")->content.c_str());
    upload("test.py","print(2)",true,false);TEST_ASSERT_EQUAL(409,server.code);TEST_ASSERT_EQUAL_STRING("print(1)",SD.nodes.at("/scripts/test.py")->content.c_str());
    upload("test.py","print(2)",true,true);TEST_ASSERT_EQUAL(200,server.code);
    upload("bad.py","bad",false);TEST_ASSERT_FALSE(SD.exists("/scripts/bad.py"));
    upload("../escape.py","bad");TEST_ASSERT_EQUAL(400,server.code);TEST_ASSERT_FALSE(SD.exists("/escape.py"));
    fakeWriteLimit=1;upload("test.py","replacement",true,true);fakeWriteLimit=static_cast<size_t>(-1);
    TEST_ASSERT_EQUAL(400,server.code);TEST_ASSERT_EQUAL_STRING("print(2)",SD.nodes.at("/scripts/test.py")->content.c_str());TEST_ASSERT_FALSE(SD.exists("/scripts/test.py.upload"));
    server.args["path"]="/scripts";server.headers["X-Portalis-Token"]=server.password;
    server.part.filename="large.py";server.part.status=UPLOAD_FILE_START;server.routes["/upload"].upload();
    server.part.status=UPLOAD_FILE_WRITE;server.part.currentSize=2048;std::memset(server.part.buf,'x',2048);
    for(size_t i=0;i<=kTransferUploadLimit/2048;++i)server.routes["/upload"].upload();
    server.part.status=UPLOAD_FILE_END;server.routes["/upload"].upload();server.routes["/upload"].handler();
    TEST_ASSERT_EQUAL(400,server.code);TEST_ASSERT_FALSE(SD.exists("/scripts/large.py"));TEST_ASSERT_FALSE(SD.exists("/scripts/large.py.upload"));
    server.part.filename="abort.py";server.part.status=UPLOAD_FILE_START;server.routes["/upload"].upload();
    server.part.status=UPLOAD_FILE_ABORTED;server.routes["/upload"].upload();TEST_ASSERT_FALSE(SD.exists("/scripts/abort.py.upload"));
    server.args["path"]="/scripts";server.args["offset"]="0";server.routes["/list"].handler();TEST_ASSERT_EQUAL(200,server.code);TEST_ASSERT_NOT_EQUAL(std::string::npos,server.response.find("test.py"));
    server.args["path"]="/scripts/test.py";fakeTcpConnected=true;server.routes["/download"].handler();TEST_ASSERT_EQUAL_STRING("print(2)",fakeTcpWritten.c_str());
    server.args["path"]="/../ssh/id_ed25519";server.routes["/download"].handler();TEST_ASSERT_EQUAL(400,server.code);
    transfer.stop();runFakeTask();transfer.poll();TEST_ASSERT_FALSE(transfer.busy());
}
void test_explorer_navigation_mutations_and_path_guards() {
    WiFi.currentMode=WIFI_STA;WiFi.connectionStatus=WL_CONNECTED;FileTransfer transfer;TEST_ASSERT_TRUE(transfer.start("192.168.1.42"));auto& server=*fakeServer;
    server.user="portalis";server.password=transfer.status().code;server.headers["X-Portalis-Token"]=server.password;
    auto action=[&](const char* route,const char* path){server.args["path"]=path;server.routes[route].handler();};
    action("/mkdir","/data");TEST_ASSERT_EQUAL(200,server.code);
    action("/mkdir","/data/sub");TEST_ASSERT_EQUAL(200,server.code);
    action("/create","/data/sub/new.txt");TEST_ASSERT_EQUAL(200,server.code);
    action("/create","/data/sub/new.txt");TEST_ASSERT_EQUAL(409,server.code);
    SD.seed("/data/sub/binary.bin",std::string("a\0b",3));
    server.args["name"]="renamed";action("/rename","/data/sub");TEST_ASSERT_EQUAL(200,server.code);TEST_ASSERT_TRUE(SD.exists("/data/renamed/new.txt"));
    action("/list","/data");TEST_ASSERT_EQUAL(200,server.code);TEST_ASSERT_NOT_EQUAL(std::string::npos,server.response.find("renamed"));TEST_ASSERT_NOT_EQUAL(std::string::npos,server.response.find("true"));
    fakeTcpConnected=true;action("/download","/data/renamed/binary.bin");TEST_ASSERT_EQUAL(3,fakeTcpWritten.size());TEST_ASSERT_EQUAL(0,fakeTcpWritten[1]);
    for(const char* bad:{"/","/../bad","/data/../bad","/data//bad","/data/","relative","/data/bad?"}) {action("/create",bad);TEST_ASSERT_EQUAL(400,server.code);}
    server.headers["X-Portalis-Token"]="wrong";action("/delete","/data");TEST_ASSERT_EQUAL(403,server.code);TEST_ASSERT_TRUE(SD.exists("/data"));
    server.headers["X-Portalis-Token"]=server.password;action("/delete","/data");TEST_ASSERT_EQUAL(400,server.code);
    server.args["confirm"]="yes";action("/delete","/");TEST_ASSERT_EQUAL(400,server.code);
    action("/delete","/data");TEST_ASSERT_EQUAL(200,server.code);TEST_ASSERT_FALSE(SD.exists("/data/renamed/binary.bin"));TEST_ASSERT_FALSE(SD.exists("/data"));
    for(int i=0;i<65;++i)SD.seed("/file"+std::to_string(i),"");
    server.args["offset"]="0";action("/list","/");TEST_ASSERT_NOT_EQUAL(std::string::npos,server.response.find("\"more\":true"));
    server.args["offset"]="64";action("/list","/");TEST_ASSERT_NOT_EQUAL(std::string::npos,server.response.find("\"more\":false"));
    action("/list","/missing");TEST_ASSERT_EQUAL(404,server.code);
    transfer.stop();runFakeTask();transfer.poll();
}
int main() {UNITY_BEGIN();RUN_TEST(test_explorer_navigation_mutations_and_path_guards);RUN_TEST(test_editor_navigation_indent_and_safe_save);RUN_TEST(test_editor_long_lines_and_normalization);RUN_TEST(test_survey_csv_generations_failure_and_stop);RUN_TEST(test_ssh_profiles_never_persist_secrets);RUN_TEST(test_telnet_fragmented_negotiation_and_raw_data);RUN_TEST(test_tcp_transport_output_input_partial_write_and_close);RUN_TEST(test_transfer_auth_paths_upload_and_overwrite);return UNITY_END();}
