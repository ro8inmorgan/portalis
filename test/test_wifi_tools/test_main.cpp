#include <unity.h>
#include "wifi_tools_model.h"
#include "../../src/wifi_profiles.cpp"
using namespace portalis;
void setUp() {} void tearDown() {}
RadarAccessPoint ap(uint64_t id,int rssi=-50,int channel=1) {
    RadarAccessPoint p; p.id=id;p.rssi=rssi;p.channel=channel;
    strcpy(p.ssid,"Lab");strcpy(p.security,"WPA2");return p;
}
void test_channel_counts_and_overlap() {
    RadarSnapshot s;
    for (int i=0;i<80;++i) { auto p=ap(i+1,-80+i/2,6);s.tally(p);s.add(p); }
    TEST_ASSERT_EQUAL(32,s.count);TEST_ASSERT_EQUAL(80,s.channelCounts[6]);
    TEST_ASSERT_EQUAL(-41,s.channelStrongest[6]);
    TEST_ASSERT_GREATER_THAN(s.channelOverlap[5],s.channelOverlap[6]);
    TEST_ASSERT_EQUAL(0,s.channelOverlap[1]);
    s.tally(ap(100,-20,0)); TEST_ASSERT_EQUAL(0,s.channelCounts[0]);
    RadarSnapshot edge;edge.tally(ap(1,-50,14));
    TEST_ASSERT_GREATER_THAN(edge.channelOverlap[12],edge.channelOverlap[13]);
}
void test_signal_tracks_identity_and_records_gaps() {
    SignalHistory signal;signal.select(ap(80),0);
    RadarSnapshot scan;scan.generation=1;scan.trackedId=80;scan.trackedFound=true;scan.trackedPoint=ap(80,-65);
    signal.update(scan);TEST_ASSERT_EQUAL(-65,signal.samples.latest());
    signal.update(scan);TEST_ASSERT_EQUAL(1,signal.samples.count);
    scan.generation=2;scan.failed=true;signal.update(scan);
    TEST_ASSERT_EQUAL(kMissingSample,signal.samples.latest());TEST_ASSERT_EQUAL(-65,signal.average());
    scan.generation=3;scan.failed=false;scan.trackedFound=false;signal.update(scan);
    TEST_ASSERT_EQUAL(kMissingSample,signal.samples.latest());
    for (int i=0;i<100;++i) { ++scan.generation;signal.update(scan); }
    TEST_ASSERT_EQUAL(60,signal.samples.count);TEST_ASSERT_EQUAL(kMissingSample,signal.average());
}
void test_changes_ignore_failed_scans_and_truncated_absence() {
    NetworkChanges changes;RadarSnapshot scan;scan.generation=1;scan.add(ap(1));changes.update(scan);
    TEST_ASSERT_EQUAL(0,changes.count);TEST_ASSERT_TRUE(changes.hasBaseline());
    scan.generation=2;scan.failed=true;scan.count=0;changes.update(scan);TEST_ASSERT_EQUAL(0,changes.count);
    scan.failed=false;scan.generation=3;scan.total=80;scan.add(ap(2));changes.update(scan);
    TEST_ASSERT_EQUAL(1,changes.count);TEST_ASSERT_NOT_NULL(strstr(changes.events[0].text,"Seen"));
    scan.generation=4;scan.total=1;scan.points[0].channel=6;strcpy(scan.points[0].security,"OPEN");changes.update(scan);
    TEST_ASSERT_EQUAL(3,changes.count);TEST_ASSERT_NOT_NULL(strstr(changes.events[0].text,"Security"));
    scan.generation=5;scan.count=scan.total=0;changes.update(scan);
    TEST_ASSERT_NOT_NULL(strstr(changes.events[0].text,"Not seen"));
    changes.reset();TEST_ASSERT_FALSE(changes.hasBaseline());TEST_ASSERT_EQUAL(0,changes.count);
}
void test_ping_statistics_and_input() {
    PingStatistics ping;ping.add(true,0);ping.add(false,0);ping.add(true,10);
    TEST_ASSERT_EQUAL(3,ping.sent);TEST_ASSERT_EQUAL(2,ping.received);TEST_ASSERT_EQUAL(33,ping.lossPercent());
    TEST_ASSERT_EQUAL(0,ping.minimum);TEST_ASSERT_EQUAL(10,ping.maximum);TEST_ASSERT_EQUAL(5,ping.average());
    for (const char* target : {"1.1.1.1","192.168.1.1","10.0.0.2"}) TEST_ASSERT_TRUE(validPingTarget(target));
    for (const char* target : {"","256.1.1.1","1.2.3","1..2.3","abc","1.2.3.4.5","0.0.0.0"}) TEST_ASSERT_FALSE(validPingTarget(target));
    TEST_ASSERT_FALSE(validWifiPassword("short"));TEST_ASSERT_TRUE(validWifiPassword("testpassword"));
    TEST_ASSERT_FALSE(validWifiPassword(std::string(64,'z')));TEST_ASSERT_TRUE(validWifiPassword(std::string(64,'a')));
}
void test_profiles_roundtrip_limit_replace_and_forget() {
    WifiProfiles profiles;
    for (int i=0;i<7;++i) TEST_ASSERT_TRUE(profiles.remember("Lab"+std::to_string(i),"testpassword"));
    TEST_ASSERT_EQUAL(5,profiles.count);TEST_ASSERT_EQUAL(-1,profiles.find("Lab0"));
    TEST_ASSERT_EQUAL(0,profiles.find("Lab6"));
    TEST_ASSERT_TRUE(profiles.remember("Lab4","updatedpassword"));TEST_ASSERT_EQUAL(0,profiles.find("Lab4"));
    WifiProfiles restored;TEST_ASSERT_TRUE(restored.load(profiles.serialize()));TEST_ASSERT_EQUAL(5,restored.count);
    TEST_ASSERT_EQUAL_STRING("updatedpassword",restored.entries[0].password.c_str());
    TEST_ASSERT_TRUE(restored.forget("Lab4"));TEST_ASSERT_EQUAL(-1,restored.find("Lab4"));
    TEST_ASSERT_FALSE(restored.remember("Lab", "bad"));
    TEST_ASSERT_FALSE(restored.load("broken"));TEST_ASSERT_EQUAL(0,restored.count);
    TEST_ASSERT_TRUE(restored.remember("Open network",""));
}
int main() { UNITY_BEGIN();RUN_TEST(test_channel_counts_and_overlap);RUN_TEST(test_signal_tracks_identity_and_records_gaps);RUN_TEST(test_changes_ignore_failed_scans_and_truncated_absence);RUN_TEST(test_ping_statistics_and_input);RUN_TEST(test_profiles_roundtrip_limit_replace_and_forget);return UNITY_END(); }
