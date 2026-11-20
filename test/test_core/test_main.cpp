#include <unity.h>
#include "model.h"
#include "../../src/page.cpp"
#include "../../src/submission.cpp"
using namespace portalis;
void setUp() {} void tearDown() {}
void test_ap_name_limits() {
    TEST_ASSERT_TRUE(validSsid("Portalis Demo")); TEST_ASSERT_TRUE(validSsid(std::string(32, 'A')));
    TEST_ASSERT_FALSE(validSsid("")); TEST_ASSERT_FALSE(validSsid(std::string(33, 'A')));
    TEST_ASSERT_FALSE(validSsid(" leading")); TEST_ASSERT_FALSE(validSsid("trailing "));
    TEST_ASSERT_FALSE(validSsid("line\nfeed"));
}
void test_template_validation() {
    PageTemplate p; TEST_ASSERT_TRUE(validTemplate(p));
    p.accent = "red;display:none"; TEST_ASSERT_FALSE(validTemplate(p));
    p.accent = "#FF00aB"; p.message = std::string(241, 'x'); TEST_ASSERT_FALSE(validTemplate(p));
}
void test_html_template_keeps_fields_and_binds_submission() {
    PageTemplate p; p.html = "<!doctype html><form><input name='note'><textarea name='more'></textarea><button>Save</button></form>";
    const auto html = renderPage(p, "1234567890abcdef");
    TEST_ASSERT_TRUE(html.find(p.html) == 0);
    TEST_ASSERT_TRUE(html.find("new FormData(form, event.submitter") != std::string::npos);
    TEST_ASSERT_TRUE(html.find("nonce=\"1234567890abcdef\"") != std::string::npos);
    TEST_ASSERT_TRUE(html.find("fetch('/confirm'") != std::string::npos);
    TEST_ASSERT_TRUE(renderPage(p, "", true).find("<form") == std::string::npos);
    TEST_ASSERT_TRUE(renderPage(p, "invalid").find("<script") == std::string::npos);
    p.html = std::string(kTemplateLimit + 1, 'x'); TEST_ASSERT_FALSE(validTemplate(p));
    p.html = std::string("a\0b", 3); TEST_ASSERT_FALSE(validTemplate(p));
}
void test_submission_retains_repeated_names_and_special_values() {
    Submission s;
    TEST_ASSERT_TRUE(parseSubmission(R"({"ticket":"1234567890abcdef","fields":[{"name":"choice","value":"a"},{"name":"choice","value":"b"},{"name":"ticket","value":"user field"},{"name":"note","value":"line\n\"quoted\" ☀\u0000"},{"name":"attachment","value":"aGk=","filename":"hi.txt","type":"text/plain","encoding":"base64"}]})", s));
    JsonDocument doc; deserializeJson(doc, s.fields);
    TEST_ASSERT_EQUAL(5, doc.size());
    TEST_ASSERT_EQUAL_STRING("b", doc[1]["value"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("ticket", doc[2]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("hi.txt", doc[4]["filename"].as<const char*>());
    TEST_ASSERT_TRUE(s.fields.find("\\u0000") != std::string::npos);
}
void test_submission_rejects_invalid_and_oversized_payloads() {
    Submission s;
    TEST_ASSERT_FALSE(parseSubmission("{}", s));
    TEST_ASSERT_FALSE(parseSubmission(std::string(kSubmissionLimit + 1, ' '), s));
    TEST_ASSERT_FALSE(parseSubmission(R"({"ticket":"1234567890abcdef","fields":[{"name":"a","value":42}]})", s));
    TEST_ASSERT_FALSE(parseSubmission(R"({"ticket":"1234567890abcdef","fields":[{"name":"","value":"x"}]})", s));
    TEST_ASSERT_FALSE(parseSubmission(R"({"ticket":"1234567890abcdef","fields":[{"name":"f","value":"x","filename":"file"}]})", s));
    TEST_ASSERT_FALSE(parseSubmission(R"({"ticket":"1234567890abcdef","fields":[{"name":"f","value":"!!!!","filename":"f","type":"","encoding":"base64"}]})", s));
    const std::string largeFile = R"({"ticket":"1234567890abcdef","fields":[{"name":"f","value":")" + std::string(10924, 'A') + R"(","filename":"f","type":"","encoding":"base64"}]})";
    TEST_ASSERT_FALSE(parseSubmission(largeFile, s));
    std::string body = R"({"ticket":"1234567890abcdef","fields":[)";
    for (size_t i = 0; i < 65; ++i) { if (i) body += ','; body += R"({"name":"a","value":"b"})"; }
    body += "]}";
    TEST_ASSERT_FALSE(parseSubmission(body, s));
}
void test_confirmation_only_once() {
    TicketGate gate; const char* token = "1234567890abcdef";
    gate.issue(token, 100);
    TEST_ASSERT_TRUE(gate.valid(token, 200));
    TEST_ASSERT_FALSE(gate.consume("bad", 200));
    TEST_ASSERT_TRUE(gate.consume(token, 200));
    TEST_ASSERT_FALSE(gate.consume(token, 201));
}
void test_confirmation_expiry_and_session_reset() {
    TicketGate gate; const char* token = "1234567890abcdef";
    gate.issue(token, 100); TEST_ASSERT_FALSE(gate.consume(token, 300101));
    gate.issue(token, UINT32_MAX - 100); TEST_ASSERT_TRUE(gate.consume(token, 50));
    gate.issue(token, 0); gate.clear(); TEST_ASSERT_FALSE(gate.consume(token, 1));
}
void test_history_retains_latest_96() {
    History h;
    for (uint32_t i = 0; i < 130; ++i) h.add({1, i});
    TEST_ASSERT_EQUAL(96, h.size()); TEST_ASSERT_EQUAL(129, h.newest(0).seconds);
    TEST_ASSERT_EQUAL(34, h.newest(95).seconds); TEST_ASSERT_EQUAL(0, h.newest(96).session);
}
void test_log_is_strict_timestamp_data() {
    Event e; TEST_ASSERT_TRUE(parseEvent("12,345", e)); TEST_ASSERT_EQUAL(12, e.session); TEST_ASSERT_EQUAL(345, e.seconds);
    TEST_ASSERT_FALSE(parseEvent("session,elapsed_seconds", e)); TEST_ASSERT_FALSE(parseEvent("1,2,3", e));
    TEST_ASSERT_FALSE(parseEvent("0,10", e)); TEST_ASSERT_FALSE(parseEvent("1,-10", e));
    TEST_ASSERT_FALSE(parseEvent("4294967296,1", e)); TEST_ASSERT_FALSE(parseEvent("1,4294967296", e));
    TEST_ASSERT_FALSE(parseEvent("1,hello", e)); TEST_ASSERT_EQUAL_STRING("60:01", elapsedLabel(3601).c_str());
}
int main() {
    UNITY_BEGIN(); RUN_TEST(test_ap_name_limits); RUN_TEST(test_template_validation);
    RUN_TEST(test_html_template_keeps_fields_and_binds_submission);
    RUN_TEST(test_submission_retains_repeated_names_and_special_values); RUN_TEST(test_submission_rejects_invalid_and_oversized_payloads); RUN_TEST(test_confirmation_only_once);
    RUN_TEST(test_confirmation_expiry_and_session_reset); RUN_TEST(test_history_retains_latest_96);
    RUN_TEST(test_log_is_strict_timestamp_data); return UNITY_END();
}
