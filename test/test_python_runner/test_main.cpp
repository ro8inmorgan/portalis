#include <unity.h>
#include "../../src/python_runner.cpp"
using namespace portalis;
void setUp() {SD.reset();WiFi={};fakeTaskFail=false;fakePythonResult=0;fakePythonSource.clear();fakeTaskTick=nullptr;fakeMillis=0;}
void tearDown() {}
void test_script_picker_filters_sorts_and_bounds() {
    PythonRunner runner;TEST_ASSERT_FALSE(runner.refresh());
    SD.seed("/scripts/z.py","print('z')");SD.seed("/scripts/a.py","");SD.seed("/scripts/note.txt","");SD.seed("/scripts/folder.py","",true);
    TEST_ASSERT_TRUE(runner.refresh());TEST_ASSERT_EQUAL(2,runner.scripts().count);TEST_ASSERT_EQUAL_STRING("a.py",runner.scripts().names[0].c_str());
    for(int i=0;i<80;++i) SD.seed("/scripts/"+std::to_string(i)+".py","");
    runner.refresh();TEST_ASSERT_EQUAL(64,runner.scripts().count);TEST_ASSERT_TRUE(runner.scripts().limited);
    TEST_ASSERT_FALSE(scriptName("../secret.py"));TEST_ASSERT_FALSE(scriptName("name.py.txt"));
}
void test_run_output_and_fresh_source_cleanup() {
    PythonRunner runner;SD.seed("/scripts/test.py","print(42)");
    TEST_ASSERT_TRUE(runner.start("test.py"));TEST_ASSERT_TRUE(runner.busy());TEST_ASSERT_FALSE(runner.refresh());
    TEST_ASSERT_FALSE(runner.start("test.py"));runFakeTask();runner.poll();
    TEST_ASSERT_FALSE(runner.busy());TEST_ASSERT_EQUAL(PythonPhase::Done,runner.status().phase);
    TEST_ASSERT_EQUAL_STRING("print(42)",fakePythonSource.c_str());
    TEST_ASSERT_EQUAL_STRING_LEN("Hello",runner.terminal().line(0),5);TEST_ASSERT_EQUAL_STRING_LEN("world",runner.terminal().line(1),5);
    fakePythonResult=1;TEST_ASSERT_TRUE(runner.start("test.py"));runFakeTask();runner.poll();TEST_ASSERT_EQUAL(PythonPhase::Failed,runner.status().phase);
}
void test_cancel_and_failures_release_runner() {
    PythonRunner runner;TEST_ASSERT_FALSE(runner.start("missing.py"));TEST_ASSERT_FALSE(runner.start("../x.py"));
    SD.seed("/scripts/big.py",std::string(32769,'x'));TEST_ASSERT_FALSE(runner.start("big.py"));
    SD.seed("/scripts/nul.py",std::string("a\0b",3));TEST_ASSERT_FALSE(runner.start("nul.py"));
    SD.seed("/scripts/test.py","");WiFi.currentMode=WIFI_STA;TEST_ASSERT_FALSE(runner.start("test.py"));TEST_ASSERT_EQUAL(WIFI_STA,WiFi.currentMode);WiFi.currentMode=WIFI_OFF;
    fakeTaskFail=true;TEST_ASSERT_FALSE(runner.start("test.py"));TEST_ASSERT_FALSE(runner.busy());fakeTaskFail=false;
    TEST_ASSERT_TRUE(runner.start("test.py"));runner.stop();runFakeTask();runner.poll();
    TEST_ASSERT_EQUAL(PythonPhase::Stopped,runner.status().phase);TEST_ASSERT_FALSE(runner.busy());
    TEST_ASSERT_TRUE(runner.start("test.py"));runFakeTask();runner.poll();TEST_ASSERT_EQUAL(PythonPhase::Done,runner.status().phase);
}
int main() {UNITY_BEGIN();RUN_TEST(test_script_picker_filters_sorts_and_bounds);RUN_TEST(test_run_output_and_fresh_source_cleanup);RUN_TEST(test_cancel_and_failures_release_runner);return UNITY_END();}
