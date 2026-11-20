#include <unity.h>
#include "probe_model.h"

using namespace portalis;
void setUp() {}
void tearDown() {}

void test_only_exact_test_ssid_is_permitted() {
    ProbeConfig config;
    TEST_ASSERT_FALSE(config.permits("test"));
    config.ssids[0] = "test"; config.count = 1;
    TEST_ASSERT_TRUE(config.permits("test"));
    TEST_ASSERT_FALSE(config.permits(nullptr));
    for (const char* name : {"", "Test", "TEST", "test ", " test", "testing", "Other Lab"})
        TEST_ASSERT_FALSE(config.permits(name));
    config.ssids[0] = "Other Lab";
    config.count = 1;
    TEST_ASSERT_FALSE(config.permits("Other Lab"));
}

void test_schedule_rejects_unapproved_network_before_activation() {
    ProbeConfig config;
    config.ssids[0] = "test"; config.count = 1;
    ProbeSchedule schedule;
    ProbeCandidate candidate;
    std::strcpy(candidate.ssid, "Other Lab");
    TEST_ASSERT_FALSE(schedule.begin(config, candidate, 0));
    TEST_ASSERT_FALSE(schedule.active());
    std::strcpy(candidate.ssid, "test");
    TEST_ASSERT_TRUE(schedule.begin(config, candidate, 0));
    TEST_ASSERT_EQUAL_STRING("test", schedule.target().ssid);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_only_exact_test_ssid_is_permitted);
    RUN_TEST(test_schedule_rejects_unapproved_network_before_activation);
    return UNITY_END();
}
