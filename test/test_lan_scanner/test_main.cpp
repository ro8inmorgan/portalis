#include <unity.h>
#include "../../src/lan_scanner.cpp"
using namespace portalis;
LanScanner* current=nullptr;
bool stopAfterTwo=false;
void setUp() {
    WiFi={}; WiFi.currentMode=WIFI_STA; WiFi.connectionStatus=WL_CONNECTED;
    fakeTask=nullptr; fakeTaskFail=false; fakeTaskTick=nullptr; fakeTcpSuccess=true;
    fakeDeletedRunning=0; current=nullptr; stopAfterTwo=false;
}
void tearDown() { for (auto* ping:fakePings) delete ping; fakePings.clear(); fakeTaskTick=nullptr; }
void test_input_bounds() {
    uint32_t first,last; uint16_t a,b;
    TEST_ASSERT_TRUE(scanRange("192.168.1.0","192.168.1.255",first,last));
    TEST_ASSERT_EQUAL_STRING("192.168.1.255",scanIpText(last).c_str());
    TEST_ASSERT_FALSE(scanRange("192.168.1.0","192.168.2.0",first,last));
    TEST_ASSERT_FALSE(scanRange("192.168.1.2","192.168.1.1",first,last));
    for (const char* ip:{"", "1.2.3", "1.2.3.256", "1..2.3", "1.2.3.4.", "127.0.0.1", "224.0.0.1", "255.255.255.255", "0.0.0.0"})
        TEST_ASSERT_FALSE(parseScanIp(ip,first));
    TEST_ASSERT_TRUE(scanPorts("1-65535",a,b)); TEST_ASSERT_EQUAL(65535,b);
    TEST_ASSERT_TRUE(scanPorts("443",a,b)); TEST_ASSERT_EQUAL(a,b);
    for (const char* ports:{"", "0", "65536", "-1", "1-", "5-3", "1-2-3", "1.2", "1-999999999"})
        TEST_ASSERT_FALSE(scanPorts(ports,a,b));
}
void test_range_results_and_selected_host_port_scan() {
    LanScanner scanner;
    TEST_ASSERT_TRUE(scanner.startRange("192.168.1.1","192.168.1.3"));
    auto* first=fakePings.back(); fakePingReply(first); fakePingEnd(first); scanner.poll();
    TEST_ASSERT_EQUAL(1,scanner.range().count); TEST_ASSERT_TRUE(first->deleted);
    TEST_ASSERT_FALSE(scanner.startPorts("192.168.1.1","80"));
    fakePingEnd(fakePings.back()); scanner.poll();
    fakePingReply(fakePings.back()); fakePingEnd(fakePings.back()); scanner.poll();
    TEST_ASSERT_FALSE(scanner.busy()); TEST_ASSERT_EQUAL(3,scanner.range().checked);
    TEST_ASSERT_EQUAL(2,scanner.range().count);
    const auto selected=scanIpText(scanner.range().hosts[1].ip);
    TEST_ASSERT_EQUAL_STRING("192.168.1.3",selected.c_str());
    TEST_ASSERT_TRUE(scanner.startPorts(selected,"65535")); runFakeTask(); scanner.poll();
    TEST_ASSERT_EQUAL(ScanPhase::Complete,scanner.ports().phase);
    TEST_ASSERT_EQUAL(65535,scanner.ports().ports[0]);
    TEST_ASSERT_EQUAL(scanner.range().hosts[1].ip,scanner.ports().host);
    TEST_ASSERT_EQUAL(2,scanner.range().count); // results survive the drill-down
    TEST_ASSERT_EQUAL(0,fakeDeletedRunning);
}
void test_range_cancel_waits_for_callback_and_ignores_late_reply() {
    LanScanner scanner;
    TEST_ASSERT_TRUE(scanner.startRange("10.0.0.1","10.0.0.2"));
    auto* ping=fakePings.back(); scanner.cancel();
    TEST_ASSERT_TRUE(scanner.busy()); TEST_ASSERT_FALSE(ping->deleted);
    fakePingReply(ping); fakePingEnd(ping); scanner.poll();
    TEST_ASSERT_FALSE(scanner.busy()); TEST_ASSERT_EQUAL(ScanPhase::Cancelled,scanner.range().phase);
    TEST_ASSERT_EQUAL(0,scanner.range().count); TEST_ASSERT_EQUAL(0,fakeDeletedRunning);
}
void taskTick() {
    current->poll();
    if (stopAfterTwo && current->ports().checked==2) current->cancel();
}
void test_port_progress_cancel_capacity_and_failure() {
    LanScanner scanner; current=&scanner; fakeTaskTick=taskTick;
    TEST_ASSERT_TRUE(scanner.startPorts("10.0.0.1","1-300")); runFakeTask(); scanner.poll();
    TEST_ASSERT_EQUAL(300,scanner.ports().checked); TEST_ASSERT_EQUAL(300,scanner.ports().found);
    TEST_ASSERT_EQUAL(256,scanner.ports().count); TEST_ASSERT_FALSE(scanner.busy());
    stopAfterTwo=true;
    TEST_ASSERT_TRUE(scanner.startPorts("10.0.0.1","1-100")); runFakeTask(); scanner.poll();
    TEST_ASSERT_EQUAL(ScanPhase::Cancelled,scanner.ports().phase); TEST_ASSERT_EQUAL(2,scanner.ports().checked);
    stopAfterTwo=false; fakeTcpSuccess=false;
    TEST_ASSERT_TRUE(scanner.startPorts("10.0.0.1","22-23")); runFakeTask(); scanner.poll();
    TEST_ASSERT_EQUAL(2,scanner.ports().checked); TEST_ASSERT_EQUAL(0,scanner.ports().count);
    fakeTaskFail=true; TEST_ASSERT_FALSE(scanner.startPorts("10.0.0.1","80"));
    TEST_ASSERT_FALSE(scanner.busy()); TEST_ASSERT_EQUAL(ScanPhase::Failed,scanner.ports().phase);
}
void test_disconnect_and_mode_ownership() {
    LanScanner scanner;
    WiFi.currentMode=WIFI_MODE_AP; TEST_ASSERT_FALSE(scanner.startRange("10.0.0.1","10.0.0.2"));
    WiFi.currentMode=WIFI_STA; TEST_ASSERT_TRUE(scanner.startRange("10.0.0.1","10.0.0.2"));
    WiFi.connectionStatus=WL_DISCONNECTED; scanner.poll();
    auto* ping=fakePings.back(); TEST_ASSERT_TRUE(ping->stopped); TEST_ASSERT_FALSE(ping->deleted);
    fakePingEnd(ping); scanner.poll(); TEST_ASSERT_FALSE(scanner.busy());
    TEST_ASSERT_EQUAL(ScanPhase::Failed,scanner.range().phase);
    WiFi.connectionStatus=WL_CONNECTED; TEST_ASSERT_TRUE(scanner.startPorts("10.0.0.1","80-81"));
    WiFi.connectionStatus=WL_DISCONNECTED; runFakeTask(); scanner.poll();
    TEST_ASSERT_EQUAL(ScanPhase::Failed,scanner.ports().phase); TEST_ASSERT_EQUAL(0,scanner.ports().checked);
}
int main() {
    UNITY_BEGIN(); RUN_TEST(test_input_bounds); RUN_TEST(test_range_results_and_selected_host_port_scan);
    RUN_TEST(test_range_cancel_waits_for_callback_and_ignores_late_reply);
    RUN_TEST(test_port_progress_cancel_capacity_and_failure); RUN_TEST(test_disconnect_and_mode_ownership);
    return UNITY_END();
}
