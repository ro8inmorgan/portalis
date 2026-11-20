#include "model.h"
#include <ArduinoJson.h>

namespace portalis {
bool parseSubmission(const std::string& body, Submission& result) {
    if (body.size() > kSubmissionLimit) return false;
    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::NestingLimit(4)) || !doc.is<JsonObject>()) return false;
    if (!doc["ticket"].is<std::string>() || !doc["fields"].is<JsonArray>()) return false;
    const auto ticket = doc["ticket"].as<std::string>();
    const auto fields = doc["fields"].as<JsonArrayConst>();
    if (!validTicket(ticket) || fields.size() > kFieldLimit) return false;
    size_t fileBytes = 0;
    for (JsonVariantConst field : fields) {
        if (!field.is<JsonObjectConst>() || !field["name"].is<std::string>() || !field["value"].is<std::string>()) return false;
        const auto name = field["name"].as<JsonString>();
        if (name.size() == 0 || name.size() > 256) return false;
        const auto object = field.as<JsonObjectConst>();
        const bool file = !field["filename"].isNull();
        if (file && (!field["filename"].is<std::string>() || !field["type"].is<std::string>() || field["encoding"] != "base64")) return false;
        if (file) {
            const auto encoded = field["value"].as<JsonString>();
            const size_t length = encoded.size();
            if (length % 4 != 0) return false;
            size_t padding = 0;
            if (length && encoded.c_str()[length - 1] == '=') ++padding;
            if (length > 1 && encoded.c_str()[length - 2] == '=') ++padding;
            for (size_t i = 0; i < length - padding; ++i) {
                const char c = encoded.c_str()[i];
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '+' || c == '/')) return false;
            }
            fileBytes += length / 4 * 3 - padding;
            if (fileBytes > 8192) return false;
        }
        for (JsonPairConst entry : object) {
            const std::string key = entry.key().c_str();
            if (key != "name" && key != "value" && !(file && (key == "filename" || key == "type" || key == "encoding"))) return false;
        }
    }
    Submission parsed;
    parsed.ticket = ticket;
    serializeJson(fields, parsed.fields);
    result = std::move(parsed);
    return true;
}
} // namespace portalis
