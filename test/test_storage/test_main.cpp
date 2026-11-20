#include <unity.h>
#include <SD.h>
#include "storage.h"
#include "../../src/storage.cpp"
using namespace portalis;
void setUp() { SD.reset(); } void tearDown() {}
const char* html = "<!doctype html><form><input name=feedback><button>Save</button></form>";
void test_browse_starts_at_root_and_includes_folders() {
    SD.seed("/a.html", html); SD.seed("/folder", "", true); SD.seed("/notes.txt", "ignore");
    Storage s; s.begin(); BrowserPage b;
    TEST_ASSERT_TRUE(s.browse("/", 0, b)); TEST_ASSERT_EQUAL(3, b.count);
    TEST_ASSERT_EQUAL_STRING("", b.entries[0].path.c_str());
    TEST_ASSERT_TRUE(b.entries[1].directory); TEST_ASSERT_EQUAL_STRING("/a.html", b.entries[2].path.c_str());
}
void test_nested_directory_pagination_survives_result_alias() {
    for (int i = 0; i < 30; ++i) SD.seed("/folder/" + std::to_string(i) + ".html", html);
    Storage s; s.begin(); BrowserPage b;
    s.browse("/folder", 0, b); TEST_ASSERT_EQUAL(12, b.count); TEST_ASSERT_EQUAL(30, b.total);
    TEST_ASSERT_TRUE(s.browse(b.directory, 12, b)); // same object supplies the path and receives results
    TEST_ASSERT_EQUAL_STRING("/folder", b.directory.c_str()); TEST_ASSERT_EQUAL(12, b.offset);
    TEST_ASSERT_EQUAL_STRING("/folder/2.html", b.entries[0].path.c_str());
    s.browse(b.directory, 24, b); TEST_ASSERT_EQUAL(6, b.count);
}
void test_browser_alphabetical_order_across_pages() {
    SD.seed("/z-folder", "", true); SD.seed("/A-folder", "", true);
    for (char letter = 'z'; letter >= 'a'; --letter)
        SD.seed("/" + std::string(1, letter == 'b' ? 'B' : letter) + ".html", html);
    Storage storage; storage.begin(); BrowserPage page;
    TEST_ASSERT_TRUE(storage.browse("/", 0, page)); TEST_ASSERT_EQUAL(29, page.total);
    TEST_ASSERT_EQUAL_STRING("", page.entries[0].path.c_str());
    TEST_ASSERT_EQUAL_STRING("A-folder", page.entries[1].label.c_str());
    TEST_ASSERT_EQUAL_STRING("z-folder", page.entries[2].label.c_str());
    TEST_ASSERT_EQUAL_STRING("a.html", page.entries[3].label.c_str());
    TEST_ASSERT_EQUAL_STRING("B.html", page.entries[4].label.c_str());
    TEST_ASSERT_EQUAL_STRING("i.html", page.entries[11].label.c_str());
    TEST_ASSERT_TRUE(storage.browse("/", 12, page));
    TEST_ASSERT_EQUAL_STRING("j.html", page.entries[0].label.c_str());
    TEST_ASSERT_TRUE(storage.browse("/", 24, page)); TEST_ASSERT_EQUAL(5, page.count);
    TEST_ASSERT_EQUAL_STRING("z.html", page.entries[4].label.c_str());
}
void test_absent_sd_has_builtin_fallback() {
    SD.present = false; Storage s; TEST_ASSERT_FALSE(s.begin()); BrowserPage b;
    TEST_ASSERT_TRUE(s.browse("/", 0, b)); TEST_ASSERT_EQUAL(1, b.count);
    PageTemplate p; std::string error; TEST_ASSERT_TRUE(s.loadTemplate("", p, error));
    TEST_ASSERT_FALSE(s.loadTemplate("/missing.html", p, error));
}
void test_html_template_load_and_limits() {
    Storage s; s.begin(); PageTemplate p; std::string error;
    SD.seed("/demo.HTML", html); TEST_ASSERT_TRUE(s.loadTemplate("/demo.HTML", p, error));
    TEST_ASSERT_EQUAL_STRING("demo.HTML", p.name.c_str()); TEST_ASSERT_EQUAL_STRING(html, p.html.c_str());
    SD.seed("/large.html", std::string(kTemplateLimit + 1, 'x')); TEST_ASSERT_FALSE(s.loadTemplate("/large.html", p, error));
    SD.seed("/empty.htm", ""); TEST_ASSERT_FALSE(s.loadTemplate("/empty.htm", p, error));
    SD.seed("/bad.htm", std::string("a\0b", 3)); TEST_ASSERT_FALSE(s.loadTemplate("/bad.htm", p, error));
    SD.seed("/old.json", "{}"); TEST_ASSERT_FALSE(s.loadTemplate("/old.json", p, error));
}
void test_submission_log_round_trip_and_write_failure() {
    Storage s; s.begin();
    const std::string fields = R"([{"name":"answer","value":"a\nb"},{"name":"answer","value":"c"}])";
    TEST_ASSERT_TRUE(s.appendSubmission({9, 12}, fields));
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, SD.nodes["/portalis/submissions.jsonl"]->content));
    TEST_ASSERT_EQUAL(9, doc["session"].as<int>());
    TEST_ASSERT_EQUAL(2, doc["fields"].size());
    TEST_ASSERT_EQUAL_STRING("a\nb", doc["fields"][0]["value"].as<const char*>());
    SD.nodes["/portalis/submissions.jsonl"]->content = std::string(256 * 1024 - 2, 'x');
    TEST_ASSERT_FALSE(s.appendSubmission({9, 13}, fields));
    TEST_ASSERT_FALSE(s.logHealthy());
    TEST_ASSERT_EQUAL(256 * 1024 - 2, SD.nodes["/portalis/submissions.jsonl"]->content.size());
    SD.present = false; s.begin(); TEST_ASSERT_FALSE(s.appendSubmission({9, 14}, fields));
}
void test_logs_restore_only_session_timestamps() {
    Storage s; s.begin(); TEST_ASSERT_TRUE(s.append({7, 15})); TEST_ASSERT_TRUE(s.append({8, 20}));
    TEST_ASSERT_EQUAL_STRING("session,elapsed_seconds\n7,15\n8,20\n", SD.nodes["/portalis/events.csv"]->content.c_str());
    History h; s.restore(h); TEST_ASSERT_EQUAL(2, h.size()); TEST_ASSERT_EQUAL(8, h.newest(0).session);
    SD.nodes["/portalis/events.csv"]->content += "1,unexpected,field\n";
    History h2; s.restore(h2); TEST_ASSERT_EQUAL(2, h2.size());
}
void test_full_log_is_preserved_and_reports_failure() {
    SD.seed("/portalis/events.csv", std::string(256 * 1024 - 2, 'x'));
    Storage storage; storage.begin();
    TEST_ASSERT_FALSE(storage.append({1, 1}));
    TEST_ASSERT_FALSE(storage.logHealthy());
    TEST_ASSERT_EQUAL(256 * 1024 - 2, SD.nodes["/portalis/events.csv"]->content.size());
}
void test_ap_config_optional_and_reload() {
    Storage storage; PortalConfig config; std::string error;
    TEST_ASSERT_TRUE(storage.loadConfig(config, error)); TEST_ASSERT_EQUAL(0, config.count);
    storage.begin(); TEST_ASSERT_TRUE(storage.loadConfig(config, error));
    SD.seed("/portalis.json", R"({"ap_names":["Guest","Workshop"]})");
    TEST_ASSERT_TRUE(storage.loadConfig(config, error)); TEST_ASSERT_EQUAL(2, config.count);
    TEST_ASSERT_EQUAL_STRING("Workshop", config.apNames[1].c_str());
    SD.nodes["/portalis.json"]->content = R"({"ap_names":["Updated"]})";
    TEST_ASSERT_TRUE(storage.loadConfig(config, error)); TEST_ASSERT_EQUAL(1, config.count);
    TEST_ASSERT_EQUAL_STRING("Updated", config.apNames[0].c_str());
    SD.nodes["/portalis.json"]->content = R"({"ap_names":[]})";
    TEST_ASSERT_TRUE(storage.loadConfig(config, error)); TEST_ASSERT_EQUAL(0, config.count);
}
void test_ap_config_rejects_bad_names_and_limits() {
    Storage storage; storage.begin(); PortalConfig config; std::string error;
    for (const char* body : {"{", "{}", R"({"ap_names":"Guest"})", R"({"ap_names":["Valid",42]})",
                            R"({"ap_names":[" leading"]})", R"({"ap_names":["trailing "]})",
                            R"({"ap_names":[""]})", R"({"ap_names":["a\u0000b"]})",
                            R"({"ap_names":["ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567"]})"}) {
        SD.seed("/portalis.json", body);
        config.count = 1;
        TEST_ASSERT_FALSE(storage.loadConfig(config, error));
        TEST_ASSERT_EQUAL(0, config.count); TEST_ASSERT_FALSE(error.empty());
    }
    std::string body = "{\"ap_names\":[";
    for (int i = 0; i < 32; ++i) { if (i) body += ','; body += "\"Guest\""; }
    SD.seed("/portalis.json", body + "]}");
    TEST_ASSERT_TRUE(storage.loadConfig(config, error)); TEST_ASSERT_EQUAL(32, config.count);
    SD.seed("/portalis.json", body + ",\"Extra\"]}");
    TEST_ASSERT_FALSE(storage.loadConfig(config, error));
    SD.seed("/portalis.json", std::string(4097, ' '));
    TEST_ASSERT_FALSE(storage.loadConfig(config, error));
}
void test_submission_viewer_reads_old_records_and_repeated_fields() {
    Storage storage; storage.begin(); std::vector<SubmissionEntry> entries; std::string error;
    TEST_ASSERT_TRUE(storage.listSubmissions(entries, error)); TEST_ASSERT_EQUAL(0, entries.size());
    TEST_ASSERT_TRUE(storage.appendSubmission({1, 2}, R"([{"name":"comment","value":"first\nsecond"},{"name":"comment","value":"again"}])"));
    TEST_ASSERT_TRUE(storage.appendSubmission({2, 9}, R"([{"name":"file","value":"aGk=","filename":"hi.txt","type":"text/plain","encoding":"base64"}])"));
    TEST_ASSERT_TRUE(storage.listSubmissions(entries, error)); TEST_ASSERT_EQUAL(2, entries.size());
    TEST_ASSERT_EQUAL(2, entries.back().event.session);
    SubmissionView view;
    TEST_ASSERT_TRUE(storage.readSubmission(entries.front(), view, error));
    TEST_ASSERT_EQUAL_STRING("comment:", view.lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("first", view.lines[1].c_str());
    TEST_ASSERT_EQUAL_STRING("second", view.lines[2].c_str());
    TEST_ASSERT_EQUAL_STRING("comment:", view.lines[4].c_str());
    TEST_ASSERT_EQUAL_STRING("again", view.lines[5].c_str());
    TEST_ASSERT_TRUE(storage.readSubmission(entries.back(), view, error));
    TEST_ASSERT_EQUAL_STRING("File: hi.txt", view.lines[1].c_str());
    for (const auto& line : view.lines) TEST_ASSERT_TRUE(line.find("aGk=") == std::string::npos);
    Storage rebooted; rebooted.begin();
    TEST_ASSERT_TRUE(rebooted.listSubmissions(entries, error)); TEST_ASSERT_EQUAL(2, entries.size());
}
void test_submission_viewer_handles_bad_logs_and_wrapping() {
    Storage storage; storage.begin(); std::vector<SubmissionEntry> entries; std::string error;
    SD.seed("/portalis/submissions.jsonl", "broken\n" + std::string(kSubmissionLimit + 300, 'x') + "\n");
    TEST_ASSERT_TRUE(storage.appendSubmission({3, 4}, R"([{"name":"long","value":"1234567890123456789012345678901234567890"}])"));
    SD.nodes["/portalis/submissions.jsonl"]->content += "{unfinished";
    TEST_ASSERT_TRUE(storage.listSubmissions(entries, error)); TEST_ASSERT_EQUAL(1, entries.size());
    SubmissionView view;
    TEST_ASSERT_TRUE(storage.readSubmission(entries[0], view, error));
    TEST_ASSERT_EQUAL_STRING("1234567890123456789", view.lines[1].c_str());
    TEST_ASSERT_EQUAL_STRING("0123456789012345678", view.lines[2].c_str());
    TEST_ASSERT_EQUAL_STRING("90", view.lines[3].c_str());
    for (const auto& line : view.lines) TEST_ASSERT_LESS_OR_EQUAL(19, line.size());
    SD.nodes["/portalis/submissions.jsonl"]->content.clear();
    TEST_ASSERT_FALSE(storage.readSubmission(entries[0], view, error));
    SD.present = false; storage.begin();
    TEST_ASSERT_FALSE(storage.listSubmissions(entries, error)); TEST_ASSERT_FALSE(error.empty());
}
int main() { UNITY_BEGIN(); RUN_TEST(test_browser_alphabetical_order_across_pages); RUN_TEST(test_submission_viewer_reads_old_records_and_repeated_fields); RUN_TEST(test_submission_viewer_handles_bad_logs_and_wrapping); RUN_TEST(test_ap_config_optional_and_reload); RUN_TEST(test_ap_config_rejects_bad_names_and_limits); RUN_TEST(test_browse_starts_at_root_and_includes_folders); RUN_TEST(test_nested_directory_pagination_survives_result_alias); RUN_TEST(test_absent_sd_has_builtin_fallback); RUN_TEST(test_html_template_load_and_limits); RUN_TEST(test_submission_log_round_trip_and_write_failure); RUN_TEST(test_logs_restore_only_session_timestamps); RUN_TEST(test_full_log_is_preserved_and_reports_failure); return UNITY_END(); }
