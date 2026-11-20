#include <unity.h>
#include "../../src/network_tools.cpp"
using namespace portalis;
void setUp() { WiFi={};fakeMillis=0;fakeTask=nullptr;fakeTaskFail=false;fakeDeletedRunning=0;fakeDnsSuccess=true;fakeTcpSuccess=true; }
void tearDown() { for (auto* p:fakePings) delete p;fakePings.clear(); }
RadarAccessPoint network() { RadarAccessPoint ap;ap.id=1;ap.connectable=true;strcpy(ap.ssid,"Lab");strcpy(ap.rawSsid,"Lab");return ap; }
void joined(NetworkTools& tools) { WiFi.connectionStatus=WL_CONNECTED;WiFi.address=IPAddress(192,168,1,123);tools.poll(); }
void finish(NetworkTools& tools) {
    tools.stop();
    for (auto* p:fakePings) if (!p->deleted) fakePingEnd(p);
    if (fakeTask) runFakeTask();
    tools.poll();TEST_ASSERT_FALSE(tools.busy());TEST_ASSERT_EQUAL(WIFI_OFF,WiFi.currentMode);
}
void test_connection_timeout_and_ap_ownership() {
    NetworkTools tools;WiFi.currentMode=WIFI_MODE_AP;
    TEST_ASSERT_FALSE(tools.begin(network(),"testpassword",true));TEST_ASSERT_EQUAL(WIFI_MODE_AP,WiFi.currentMode);
    WiFi.currentMode=WIFI_OFF;TEST_ASSERT_TRUE(tools.begin(network(),"testpassword",true));
    TEST_ASSERT_FALSE(WiFi.persistentStorage);fakeMillis=30000;tools.poll();
    TEST_ASSERT_EQUAL(NetworkPhase::Failed,tools.status().phase);finish(tools);
}
void test_ping_pause_replace_and_stop_wait_for_end() {
    NetworkTools tools;TEST_ASSERT_TRUE(tools.begin(network(),"testpassword",true));joined(tools);
    TEST_ASSERT_EQUAL(NetworkPhase::Ready,tools.status().phase);
    auto* first=fakePings.back();fakePingReply(first);fakePingTimeout(first);tools.poll();
    TEST_ASSERT_EQUAL(2,tools.status().ping.sent);TEST_ASSERT_EQUAL(50,tools.status().ping.lossPercent());
    TEST_ASSERT_FALSE(tools.setTarget("not-an-ip"));
    TEST_ASSERT_TRUE(tools.setTarget("1.1.1.1"));TEST_ASSERT_TRUE(first->stopped);TEST_ASSERT_FALSE(first->deleted);
    // Stale replies after replacing a target must not contaminate its statistics.
    fakePingReply(first);fakePingEnd(first);tools.poll();
    TEST_ASSERT_EQUAL(0,tools.status().ping.sent);TEST_ASSERT_TRUE(first->deleted);
    auto* second=fakePings.back();TEST_ASSERT_TRUE(second!=first);
    tools.togglePause();TEST_ASSERT_TRUE(tools.status().paused);fakePingEnd(second);tools.poll();
    tools.togglePause();TEST_ASSERT_FALSE(tools.status().paused);
    tools.stop();TEST_ASSERT_TRUE(tools.busy());finish(tools);
    TEST_ASSERT_EQUAL(0,fakeDeletedRunning);
}
void test_diagnostics_independent_results_and_cancel_worker() {
    NetworkTools tools;TEST_ASSERT_TRUE(tools.begin(network(),"testpassword",false));joined(tools);
    TEST_ASSERT_EQUAL(NetworkPhase::Checking,tools.status().phase);TEST_ASSERT_NOT_NULL(fakeTask);
    auto* ping=fakePings.back();fakePingTimeout(ping);fakePingEnd(ping);fakeDnsSuccess=false;runFakeTask();tools.poll();
    TEST_ASSERT_EQUAL(NetworkPhase::Ready,tools.status().phase);
    TEST_ASSERT_EQUAL(CheckState::NoReply,tools.status().gatewayCheck);
    TEST_ASSERT_EQUAL(CheckState::NoReply,tools.status().dns);
    TEST_ASSERT_EQUAL(CheckState::Pass,tools.status().internet);
    tools.refresh();TEST_ASSERT_NOT_NULL(fakeTask);tools.stop();
    TEST_ASSERT_EQUAL(NetworkPhase::Stopping,tools.status().phase);
    TEST_ASSERT_FALSE(tools.begin(network(),"testpassword",true));
    finish(tools);TEST_ASSERT_EQUAL(0,fakeDeletedRunning);
}
void test_disconnect_is_reported_and_worker_failure_completes() {
    NetworkTools tools;fakeTaskFail=true;TEST_ASSERT_TRUE(tools.begin(network(),"testpassword",false));joined(tools);
    auto* ping=fakePings.back();fakePingReply(ping);fakePingEnd(ping);tools.poll();
    TEST_ASSERT_EQUAL(NetworkPhase::Ready,tools.status().phase);TEST_ASSERT_EQUAL(CheckState::Error,tools.status().dns);
    WiFi.connectionStatus=WL_DISCONNECTED;tools.poll();
    TEST_ASSERT_EQUAL(NetworkPhase::Failed,tools.status().phase);finish(tools);
}
int main() { UNITY_BEGIN();RUN_TEST(test_connection_timeout_and_ap_ownership);RUN_TEST(test_ping_pause_replace_and_stop_wait_for_end);RUN_TEST(test_diagnostics_independent_results_and_cancel_worker);RUN_TEST(test_disconnect_is_reported_and_worker_failure_completes);return UNITY_END(); }
