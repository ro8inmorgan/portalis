#include <unity.h>
#include "../../src/page.cpp"
#include "../../src/submission.cpp"
#include "../../src/storage.cpp"
#include "../../src/probe_config.cpp"
using namespace portalis;
void setUp() { SD.reset(); }
void tearDown() {}
void test_notice_selection_and_reload() {
    ProbeConfig config; std::string error;
    SD.seed("/probe-test.json", R"({"approved_ssids":[],"notice_template":"/notices/demo.HTML"})");
    SD.seed("/notices/demo.HTML", "<h1>My notice</h1>");
    TEST_ASSERT_TRUE(loadProbeConfig(config, error));
    TEST_ASSERT_EQUAL_STRING("<h1>My notice</h1>", config.noticeHtml.c_str());
    SD.seed("/notices/demo.HTML", "<p>Updated</p>");
    TEST_ASSERT_TRUE(loadProbeConfig(config, error));
    TEST_ASSERT_EQUAL_STRING("<p>Updated</p>", config.noticeHtml.c_str());
    SD.seed("/probe-test.json", R"({"approved_ssids":[]})");
    TEST_ASSERT_TRUE(loadProbeConfig(config, error));
    TEST_ASSERT_TRUE(config.noticeHtml.empty());
    TEST_ASSERT_EQUAL_STRING(kProbeNotice, renderProbeNotice(config.noticeHtml).c_str());
}
void test_invalid_notice_clears_previous_content() {
    ProbeConfig config; std::string error;
    for (const char* value : {"123", "\"relative.html\"", "\"/../notice.html\"", "\"/notice.txt\"", "\"/missing.html\"", "\"/bad\\u0000.html\""}) {
        config.noticeHtml = "old";
        SD.seed("/probe-test.json", std::string("{\"approved_ssids\":[],\"notice_template\":") + value + "}");
        TEST_ASSERT_FALSE(loadProbeConfig(config, error));
        TEST_ASSERT_TRUE(config.noticeHtml.empty());
    }
    SD.seed("/probe-test.json", R"({"approved_ssids":[],"notice_template":"/notice.html"})");
    for (const auto& html : {std::string(), std::string(kTemplateLimit + 1, 'x'), std::string("a\0b", 3)}) {
        SD.seed("/notice.html", html);
        TEST_ASSERT_FALSE(loadProbeConfig(config, error));
    }
    SD.seed("/notice.html", std::string(kTemplateLimit, 'x'));
    TEST_ASSERT_TRUE(loadProbeConfig(config, error));
    SD.seed("/notice.html", "", true);
    TEST_ASSERT_FALSE(loadProbeConfig(config, error));
}
void test_feedback_template_has_bound_handler() {
    const std::string html = "<form><input name=\"username\"><textarea name=\"feedback\"></textarea></form>";
    const std::string page = renderProbeNotice(html, "0123456789abcdef");
    TEST_ASSERT_NOT_EQUAL(std::string::npos, page.find(html));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, page.find("nonce=\"0123456789abcdef\""));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, page.find("fetch('/confirm'"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, page.find("window.location.assign('/done')"));
}
void test_custom_form_fields_saved_and_viewable() {
    Submission submission;
    TEST_ASSERT_TRUE(parseSubmission(R"({"ticket":"0123456789abcdef","fields":[{"name":"username","value":"Robin"},{"name":"feedback","value":"Nice"},{"name":"rating","value":"5"},{"name":"features","value":"fast"},{"name":"features","value":"clear"}]})", submission));
    Storage storage;
    TEST_ASSERT_TRUE(storage.begin());
    TEST_ASSERT_TRUE(storage.appendSubmission({42, 12}, submission.fields));
    JsonDocument saved;
    TEST_ASSERT_FALSE(deserializeJson(saved, SD.nodes["/portalis/submissions.jsonl"]->content));
    TEST_ASSERT_EQUAL(5, saved["fields"].size());
    TEST_ASSERT_EQUAL_STRING("rating", saved["fields"][2]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("5", saved["fields"][2]["value"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("fast", saved["fields"][3]["value"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("clear", saved["fields"][4]["value"].as<const char*>());
    std::vector<SubmissionEntry> entries; std::string error;
    TEST_ASSERT_TRUE(storage.listSubmissions(entries, error));
    TEST_ASSERT_EQUAL(1, entries.size());
    SubmissionView view;
    TEST_ASSERT_TRUE(storage.readSubmission(entries[0], view, error));
    TEST_ASSERT_TRUE(std::find(view.lines.begin(), view.lines.end(), "rating:") != view.lines.end());
    SD.present = false;
    TEST_ASSERT_FALSE(storage.begin());
    TEST_ASSERT_FALSE(storage.appendSubmission({42, 13}, submission.fields));
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_notice_selection_and_reload);
    RUN_TEST(test_invalid_notice_clears_previous_content);
    RUN_TEST(test_feedback_template_has_bound_handler);
    RUN_TEST(test_custom_form_fields_saved_and_viewable);
    return UNITY_END();
}
