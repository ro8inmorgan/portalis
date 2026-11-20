#include <unity.h>
#include "radar.h"
#include "wifi_scanner.h"
#include "../../src/wifi_scanner.cpp"
using namespace portalis;
void setUp() { WiFi = {}; fakeMillis = 100; fakeResults.clear(); }
void tearDown() {}
void test_signal_radius_is_bounded_and_monotonic() {
    TEST_ASSERT_EQUAL(8, radarRadius(-30)); TEST_ASSERT_EQUAL(35, radarRadius(-95));
    for (int rssi = -200; rssi < 20; ++rssi) {
        TEST_ASSERT_GREATER_OR_EQUAL(8, radarRadius(rssi)); TEST_ASSERT_LESS_OR_EQUAL(35, radarRadius(rssi));
        TEST_ASSERT_LESS_OR_EQUAL(radarRadius(rssi), radarRadius(rssi + 1));
    }
    TEST_ASSERT_EQUAL_FLOAT(radarAngle(123), radarAngle(123));
    TEST_ASSERT_TRUE(radarAngle(123) >= 0 && radarAngle(123) < 6.283186f);
}
void test_strongest_32_deduplication_and_selection() {
    RadarSnapshot s;
    for (int i = 0; i < 60; ++i) { RadarAccessPoint p; p.id = i + 1; p.rssi = -100 + i; s.add(p); }
    TEST_ASSERT_EQUAL(32, s.count); TEST_ASSERT_EQUAL(60, s.points[0].id);
    RadarAccessPoint p; p.id = 30; p.rssi = -25; s.add(p);
    TEST_ASSERT_EQUAL(32, s.count); TEST_ASSERT_EQUAL(30, s.points[0].id);
    TEST_ASSERT_EQUAL(0, radarSelection(s, 30)); TEST_ASSERT_EQUAL(1, radarSelection(s, 60));
    TEST_ASSERT_EQUAL(0, radarSelection(s, 999));
}
void test_scanner_is_async_passive_and_does_not_interrupt_ap() {
    WifiScanner scanner; WiFi.currentMode = WIFI_MODE_AP;
    TEST_ASSERT_FALSE(scanner.start()); TEST_ASSERT_EQUAL(WIFI_MODE_AP, WiFi.currentMode);
    WiFi.currentMode = WIFI_OFF; TEST_ASSERT_TRUE(scanner.start());
    TEST_ASSERT_TRUE(WiFi.lastAsync && WiFi.lastHidden && WiFi.lastPassive);
    TEST_ASSERT_FALSE(WiFi.autoReconnect); TEST_ASSERT_TRUE(scanner.snapshot().scanning);
    scanner.poll(); TEST_ASSERT_TRUE(scanner.snapshot().scanning);
    scanner.stop(); TEST_ASSERT_EQUAL(WIFI_OFF, WiFi.currentMode);
    TEST_ASSERT_FALSE(scanner.snapshot().scanning);
}
void test_completed_scan_free_results_and_refreshes_without_old_dots() {
    WifiScanner scanner; scanner.start();
    for (int i = 0; i < 40; ++i) {
        wifi_ap_record_t r; r.bssid[5] = i + 1; r.rssi = -90 + i;
        if (i) std::memcpy(r.ssid, "Access point", 12);
        fakeResults.push_back(r);
    }
    WiFi.scanState = 40; scanner.poll();
    TEST_ASSERT_EQUAL(32, scanner.snapshot().count); TEST_ASSERT_EQUAL(40, scanner.snapshot().total);
    TEST_ASSERT_TRUE(fakeResults.empty()); TEST_ASSERT_FALSE(scanner.snapshot().scanning);
    const auto generation = scanner.snapshot().generation;
    fakeMillis += 5000; scanner.poll(); TEST_ASSERT_TRUE(scanner.snapshot().scanning);
    WiFi.scanState = 0; scanner.poll(); TEST_ASSERT_EQUAL(0, scanner.snapshot().count);
    TEST_ASSERT_GREATER_THAN(generation, scanner.snapshot().generation);
}
void test_scan_failure_retry_hidden_names_and_cleanup() {
    WifiScanner scanner; scanner.start(); WiFi.scanState = WIFI_SCAN_FAILED; scanner.poll();
    TEST_ASSERT_TRUE(scanner.snapshot().failed); TEST_ASSERT_EQUAL(0, scanner.snapshot().count);
    fakeMillis += 5000; scanner.poll(); TEST_ASSERT_TRUE(scanner.snapshot().scanning);
    wifi_ap_record_t hidden; hidden.bssid[5] = 7; fakeResults.push_back(hidden); WiFi.scanState = 1;
    scanner.poll(); TEST_ASSERT_EQUAL_STRING("Hidden 0007", scanner.snapshot().points[0].ssid);
    scanner.stop(); scanner.start(); scanner.stop(); TEST_ASSERT_EQUAL(WIFI_OFF, WiFi.currentMode);
}
void test_track_weak_ap_outside_top32_and_tally_all() {
    WifiScanner scanner;scanner.start();scanner.track(1);
    for (int i=0;i<40;++i) { wifi_ap_record_t ap;ap.bssid[5]=i+1;ap.rssi=-95+i;ap.primary=6;strcpy(reinterpret_cast<char*>(ap.ssid),"Lab");fakeResults.push_back(ap); }
    WiFi.scanState=40;scanner.poll();
    TEST_ASSERT_EQUAL(-1,scanner.snapshot().find(1));
    TEST_ASSERT_TRUE(scanner.snapshot().trackedFound);TEST_ASSERT_EQUAL(-95,scanner.snapshot().trackedPoint.rssi);
    TEST_ASSERT_EQUAL(40,scanner.snapshot().channelCounts[6]);
    TEST_ASSERT_TRUE(scanner.snapshot().points[0].connectable);
    TEST_ASSERT_EQUAL_STRING("Lab",scanner.snapshot().points[0].rawSsid);
    scanner.stop();
}
int main() { UNITY_BEGIN(); RUN_TEST(test_track_weak_ap_outside_top32_and_tally_all); RUN_TEST(test_signal_radius_is_bounded_and_monotonic); RUN_TEST(test_strongest_32_deduplication_and_selection); RUN_TEST(test_scanner_is_async_passive_and_does_not_interrupt_ap); RUN_TEST(test_completed_scan_free_results_and_refreshes_without_old_dots); RUN_TEST(test_scan_failure_retry_hidden_names_and_cleanup); return UNITY_END(); }
