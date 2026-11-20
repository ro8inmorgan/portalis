#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include "SPI.h"
constexpr int CARD_NONE = 0;
constexpr const char* FILE_APPEND = "a";
struct Node { std::string path, content; bool directory = false; std::vector<std::shared_ptr<Node>> children; };
class File {
    std::shared_ptr<Node> node_;
    size_t pos_ = 0, child_ = 0;
public:
    File() = default;
    File(std::shared_ptr<Node> node, bool append = false) : node_(node), pos_(append ? node->content.size() : 0) {}
    explicit operator bool() const { return bool(node_); }
    bool isDirectory() const { return node_ && node_->directory; }
    const char* name() const { return node_->path.c_str(); }
    bool seek(uint32_t offset) { if (!node_ || offset > node_->content.size()) return false; pos_ = offset; return true; }
    size_t size() const { return node_->content.size(); }
    File openNextFile() { return node_ && child_ < node_->children.size() ? File(node_->children[child_++]) : File(); }
    int available() const { return node_ && pos_ < node_->content.size(); }
    int read() { return available() ? static_cast<unsigned char>(node_->content[pos_++]) : -1; }
    size_t readBytes(char* out, size_t n) { size_t count = 0; while (count < n && available()) out[count++] = read(); return count; }
    size_t write(const uint8_t* in, size_t n) { if (!node_) return 0; node_->content.append(reinterpret_cast<const char*>(in), n); return n; }
    size_t print(const char* in) { return write(reinterpret_cast<const uint8_t*>(in), strlen(in)); }
    void flush() {} void close() { node_.reset(); }
};
class FakeSD {
public:
    bool present = true;
    std::map<std::string, std::shared_ptr<Node>> nodes;
    FakeSD() { reset(); }
    void reset() { nodes.clear(); present = true; nodes["/"] = std::make_shared<Node>(Node{"/", "", true, {}}); }
    void seed(const std::string& path, const std::string& contents, bool dir = false) {
        auto node = std::make_shared<Node>(Node{path, contents, dir, {}});
        nodes[path] = node;
        const auto slash = path.find_last_of('/');
        const auto parent = slash == 0 ? "/" : path.substr(0, slash);
        if (!nodes.count(parent)) seed(parent, "", true);
        nodes[parent]->children.push_back(node);
    }
    bool begin(int, FakeSPI&, int) { return present; } void end() {}
    int cardType() const { return present ? 1 : CARD_NONE; }
    bool exists(const char* path) const { return nodes.count(path); }
    bool remove(const char* path) { return nodes.erase(path) != 0; }
    bool mkdir(const char* path) { seed(path, "", true); return true; }
    File open(const char* path, const char* mode = "r") {
        if (!present) return {};
        if (!nodes.count(path) && std::string(mode) == "a") seed(path, "");
        return nodes.count(path) ? File(nodes.at(path), std::string(mode) == "a") : File();
    }
};
inline FakeSD SD;
