#include <unity.h>
#include "probe_model.h"
#include "../../src/page.cpp"
#include "../../src/probe_config.cpp"
using namespace portalis;
void setUp() { SD.reset(); } void tearDown() {}
ProbeConfig scope() { ProbeConfig c; c.cooldownSeconds = 600; c.ssids[0] = "test"; c.count = 1; return c; }
ProbeCandidate candidate(unsigned id = 1, const char* name = "test") { ProbeCandidate p; p.device = {2,0,0,0,0,static_cast<uint8_t>(id)}; strcpy(p.ssid, name); return p; }
void test_bounded_probe_parser() {
    uint8_t frame[80] = {}; frame[0] = 0x40; frame[10] = 2; frame[24] = 0; frame[25] = 6;
    memcpy(frame + 26, "My Lab", 6); ProbeCandidate p;
    TEST_ASSERT_TRUE(parseProbe(frame, 32, p)); TEST_ASSERT_EQUAL_STRING("My Lab", p.ssid);
    for (size_t size = 0; size < 32; ++size) TEST_ASSERT_FALSE(parseProbe(frame, size, p));
    frame[25] = 0; TEST_ASSERT_FALSE(parseProbe(frame, 32, p));
    frame[25] = 33; TEST_ASSERT_FALSE(parseProbe(frame, 80, p));
    frame[25] = 6; frame[10] = 3; TEST_ASSERT_FALSE(parseProbe(frame, 32, p));
    frame[10] = 0; TEST_ASSERT_FALSE(parseProbe(frame, 32, p));
    frame[10] = 2; frame[0] = 0x80; TEST_ASSERT_FALSE(parseProbe(frame, 32, p));
    frame[0] = 0x40; frame[28] = 0; TEST_ASSERT_FALSE(parseProbe(frame, 32, p));
    TEST_ASSERT_FALSE(parseProbe(nullptr, 32, p));
}
void test_deadlines_and_wrap() {
    for (uint32_t start : {0U, UINT32_MAX - 100U}) {
        ProbeSchedule s; TEST_ASSERT_TRUE(s.begin(scope(), candidate(), start));
        TEST_ASSERT_EQUAL(120, s.remaining(start)); TEST_ASSERT_FALSE(s.finished(start + 119999));
        TEST_ASSERT_TRUE(s.finished(start + 120000)); TEST_ASSERT_EQUAL(0, s.remaining(start + 120000));
        s.reset(); s.begin(scope(), candidate(), start);
        TEST_ASSERT_TRUE(s.observeConnection(start + 1000)); TEST_ASSERT_FALSE(s.observeConnection(start + 2000));
        TEST_ASSERT_EQUAL(119, s.remaining(start + 1000)); TEST_ASSERT_FALSE(s.finished(start + 119999));
        TEST_ASSERT_TRUE(s.finished(start + 120000));
        s.reset(); s.begin(scope(), candidate(), start); s.observeConnection(start + 119000);
        TEST_ASSERT_EQUAL(1, s.remaining(start + 119000)); TEST_ASSERT_TRUE(s.finished(start + 120000));
    }
}
void test_configurable_total_timeout() {
    for (uint32_t seconds : {30U, 120U, 300U, 600U}) {
        for (uint32_t start : {0U, UINT32_MAX - 100U}) {
            auto config = scope(); config.timeoutSeconds = seconds;
            ProbeSchedule schedule;
            TEST_ASSERT_TRUE(schedule.begin(config, candidate(), start));
            TEST_ASSERT_EQUAL(seconds, schedule.remaining(start));
            // A join near the deadline neither resets the timer nor applies an old 120s cap.
            TEST_ASSERT_TRUE(schedule.observeConnection(start + seconds * 1000 - 500));
            config.timeoutSeconds = 30; // active attempt keeps its initial timeout
            TEST_ASSERT_EQUAL(1, schedule.remaining(start + seconds * 1000 - 1));
            TEST_ASSERT_FALSE(schedule.finished(start + seconds * 1000 - 1));
            TEST_ASSERT_TRUE(schedule.finished(start + seconds * 1000));
            TEST_ASSERT_EQUAL(0, schedule.remaining(start + seconds * 1000));
        }
    }
    for (uint32_t seconds : {0U, 29U, 601U, UINT32_MAX}) {
        auto config = scope(); config.timeoutSeconds = seconds;
        ProbeSchedule schedule;
        TEST_ASSERT_FALSE(schedule.begin(config, candidate(), 0));
    }
}
void test_timeout_config_validation() {
    ProbeConfig config; std::string error;
    for (const char* value : {"0", "29", "601", "-1", "4294967295", "1.5", "true", "\"120\""}) {
        SD.seed("/portalis.json", std::string("{\"timeout_seconds\":") + value + "}");
        TEST_ASSERT_FALSE(loadProbeConfig(config, error));
    }
    SD.seed("/portalis.json", R"({"timeout_seconds":300,"cooldown_seconds":90})");
    TEST_ASSERT_TRUE(loadProbeConfig(config, error));
    TEST_ASSERT_EQUAL(300, config.timeoutSeconds);
    TEST_ASSERT_EQUAL(90, config.cooldownSeconds);
    SD.seed("/portalis.json", "{}");
    TEST_ASSERT_TRUE(loadProbeConfig(config, error));
    TEST_ASSERT_EQUAL(120, config.timeoutSeconds);
}
void test_cooldown_cannot_be_bypassed_by_another_ssid_or_full_cache() {
    ProbeSchedule s;
    TEST_ASSERT_TRUE(s.begin(scope(), candidate(), 0)); s.end(120000);
    TEST_ASSERT_FALSE(s.begin(scope(), candidate(1, "Other Lab"), 120001));
    TEST_ASSERT_FALSE(s.begin(scope(), candidate(), 719999));
    TEST_ASSERT_TRUE(s.begin(scope(), candidate(), 720000)); s.end(720000);
    s.reset();
    for (unsigned i = 0; i < 32; ++i) { TEST_ASSERT_TRUE(s.begin(scope(), candidate(i), i)); s.end(i); }
    TEST_ASSERT_FALSE(s.begin(scope(), candidate(33), 100));
    TEST_ASSERT_FALSE(s.begin(scope(), candidate(0), 100));
    TEST_ASSERT_TRUE(s.begin(scope(), candidate(33), 600000));
}
void test_config_rejects_invalid_input() {
    ProbeConfig c; std::string error;
    TEST_ASSERT_TRUE(loadProbeConfig(c, error));
    TEST_ASSERT_TRUE(c.noticePath.empty());
    for (const char* invalid : {"broken", "[]", R"({"cooldown_seconds":0})", R"({"cooldown_seconds":86401})", R"({"notice_template":42})"}) {
        c.noticeHtml = "old"; SD.seed("/portalis.json", invalid);
        TEST_ASSERT_FALSE(loadProbeConfig(c, error)); TEST_ASSERT_TRUE(c.noticeHtml.empty());
    }
    SD.seed("/portalis.json", R"({"ap_names":[],"notice_template":""})");
    TEST_ASSERT_TRUE(loadProbeConfig(c, error)); TEST_ASSERT_TRUE(c.noticePath.empty());
}

void test_notice_and_bounded_history() {
    for (const char* element : {"<form", "<input", "<script", "http://", "https://"}) TEST_ASSERT_NULL(strstr(kProbeNotice, element));
    ProbeStatus status;
    for (int i = 0; i < 40; ++i) status.record("My Lab", i, i % 2);
    TEST_ASSERT_EQUAL(32, status.eventCount); TEST_ASSERT_EQUAL(39, status.events[0].seconds);
    TEST_ASSERT_EQUAL(8, status.events[31].seconds);
}
int main() { UNITY_BEGIN(); RUN_TEST(test_configurable_total_timeout); RUN_TEST(test_timeout_config_validation); RUN_TEST(test_bounded_probe_parser); RUN_TEST(test_deadlines_and_wrap); RUN_TEST(test_cooldown_cannot_be_bypassed_by_another_ssid_or_full_cache); RUN_TEST(test_config_rejects_invalid_input); RUN_TEST(test_notice_and_bounded_history); return UNITY_END(); }
