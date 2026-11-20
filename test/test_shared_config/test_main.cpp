#include <unity.h>
#include "../../src/storage.cpp"
#include "../../src/page.cpp"
#include "../../src/probe_config.cpp"
using namespace portalis;
void setUp() { SD.reset(); }
void tearDown() {}
void test_fresh_card_creates_unified_config_without_template() {
    Storage storage; TEST_ASSERT_TRUE(storage.begin());
    TEST_ASSERT_TRUE(SD.exists("/portalis.json"));
    TEST_ASSERT_FALSE(SD.exists("/probe-test.json"));
    TEST_ASSERT_FALSE(SD.exists("/probe-notice.html"));
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, SD.nodes.at("/portalis.json")->content));
    TEST_ASSERT_EQUAL(120, doc["cooldown_seconds"].as<int>());
    TEST_ASSERT_EQUAL(120, doc["timeout_seconds"].as<int>());
    TEST_ASSERT_EQUAL_STRING("", doc["notice_template"].as<const char*>());
    PortalConfig portal; ProbeConfig probe; std::string error;
    TEST_ASSERT_TRUE(storage.loadConfig(portal, error));
    TEST_ASSERT_EQUAL(3, portal.count);
    TEST_ASSERT_TRUE(loadProbeConfig(probe, error));
    TEST_ASSERT_TRUE(probe.noticePath.empty());
    TEST_ASSERT_TRUE(probe.noticeHtml.empty());
    BrowserPage page;
    TEST_ASSERT_TRUE(storage.browse("/", 0, page, false));
    TEST_ASSERT_EQUAL(0, page.count);
    TEST_ASSERT_TRUE(storage.browse("/", 0, page));
    TEST_ASSERT_EQUAL(1, page.count);
}
void test_shared_settings_and_custom_file_survive_remount() {
    const std::string body = R"({"ap_names":["Guest"],"cooldown_seconds":90,"notice_template":"/custom.htm"})";
    SD.seed("/portalis.json", body);
    SD.seed("/custom.htm", "<h1>Custom</h1>");
    SD.seed("/probe-test.json", R"({"cooldown_seconds":999})");
    Storage storage; storage.begin(); storage.begin();
    TEST_ASSERT_EQUAL_STRING(body.c_str(), SD.nodes.at("/portalis.json")->content.c_str());
    ProbeConfig probe; PortalConfig portal; std::string error;
    TEST_ASSERT_TRUE(loadProbeConfig(probe, error));
    TEST_ASSERT_EQUAL(90, probe.cooldownSeconds);
    TEST_ASSERT_EQUAL_STRING("/custom.htm", probe.noticePath.c_str());
    TEST_ASSERT_TRUE(storage.loadConfig(portal, error));
    TEST_ASSERT_EQUAL_STRING("Guest", portal.apNames[0].c_str());
    SD.nodes.at("/custom.htm")->content = "<p>Updated</p>";
    TEST_ASSERT_TRUE(loadProbeConfig(probe, error));
    TEST_ASSERT_EQUAL_STRING("<p>Updated</p>", probe.noticeHtml.c_str());
    SD.remove("/custom.htm");
    TEST_ASSERT_FALSE(loadProbeConfig(probe, error));
    TEST_ASSERT_TRUE(probe.noticeHtml.empty());
}
void test_old_config_defaults_and_explicit_picker_selection() {
    SD.seed("/portalis.json", R"({"ap_names":[]})");
    ProbeConfig probe; std::string error;
    TEST_ASSERT_TRUE(loadProbeConfig(probe, error));
    TEST_ASSERT_EQUAL(120, probe.cooldownSeconds);
    TEST_ASSERT_TRUE(probe.noticePath.empty());
    SD.seed("/selected.html", "<h1>Selected</h1>");
    TEST_ASSERT_TRUE(loadProbeConfig(probe, error, "/selected.html"));
    TEST_ASSERT_EQUAL_STRING("/selected.html", probe.noticePath.c_str());
}
void test_bad_config_and_missing_card_do_not_reuse_old_selection() {
    ProbeConfig probe; std::string error;
    for (const char* body : {"broken", "[]", R"({"cooldown_seconds":0})", R"({"notice_template":42})"}) {
        SD.seed("/portalis.json", body);
        Storage storage; storage.begin();
        TEST_ASSERT_EQUAL_STRING(body, SD.nodes.at("/portalis.json")->content.c_str());
        probe.noticeHtml = "old";
        TEST_ASSERT_FALSE(loadProbeConfig(probe, error));
        TEST_ASSERT_TRUE(probe.noticeHtml.empty());
    }
    SD.reset(); SD.present = false;
    Storage storage; TEST_ASSERT_FALSE(storage.begin());
    TEST_ASSERT_FALSE(loadProbeConfig(probe, error));
    TEST_ASSERT_FALSE(error.empty());
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_card_creates_unified_config_without_template);
    RUN_TEST(test_shared_settings_and_custom_file_survive_remount);
    RUN_TEST(test_old_config_defaults_and_explicit_picker_selection);
    RUN_TEST(test_bad_config_and_missing_card_do_not_reuse_old_selection);
    return UNITY_END();
}
