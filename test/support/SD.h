#pragma once
#include <map>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include "SPI.h"
constexpr int CARD_NONE = 0;
constexpr const char* FILE_APPEND = "a";
inline size_t fakeWriteLimit=static_cast<size_t>(-1);
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
    size_t write(const uint8_t* in, size_t n) { if (!node_) return 0; n=std::min(n,fakeWriteLimit); node_->content.append(reinterpret_cast<const char*>(in), n); return n; }
    size_t print(const char* in) { return write(reinterpret_cast<const uint8_t*>(in), strlen(in)); }
    void flush() {} void close() { node_.reset(); }
};
class FakeSD {
public:
    bool present = true;
    std::string rejectRenameTo;
    std::map<std::string, std::shared_ptr<Node>> nodes;
    FakeSD() { reset(); }
    void reset() { nodes.clear(); present = true; rejectRenameTo.clear(); nodes["/"] = std::make_shared<Node>(Node{"/", "", true, {}}); }
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
    bool remove(const char* path) {
        auto found=nodes.find(path);if(found==nodes.end())return false;auto node=found->second;
        for(auto& item:nodes) {auto& children=item.second->children;children.erase(std::remove(children.begin(),children.end(),node),children.end());}
        nodes.erase(found);return true;
    }
    bool rmdir(const char* path) {
        auto it=nodes.find(path);if(it==nodes.end() || !it->second->directory || !it->second->children.empty() || std::string(path)=="/")return false;
        return remove(path);
    }
    bool rename(const char* from,const char* to) {
        if(!present || rejectRenameTo==to || !nodes.count(from) || nodes.count(to))return false;
        std::string old=from,target=to;auto node=nodes.at(old);
        auto parent=[](const std::string& p){auto slash=p.find_last_of('/');return slash==0?std::string("/"):p.substr(0,slash);};
        if(!nodes.count(parent(target)))return false;
        auto& siblings=nodes.at(parent(old))->children;siblings.erase(std::remove(siblings.begin(),siblings.end(),node),siblings.end());
        nodes.at(parent(target))->children.push_back(node);
        std::vector<std::pair<std::string,std::shared_ptr<Node>>> moved;
        for(auto it=nodes.begin();it!=nodes.end();) {if(it->first==old || it->first.find(old+"/")==0) {moved.push_back({target+it->first.substr(old.size()),it->second});it=nodes.erase(it);}else ++it;}
        for(auto& item:moved) {item.second->path=item.first;nodes[item.first]=item.second;}return true;
    }
    bool mkdir(const char* path) { seed(path, "", true); return true; }
    File open(const char* path, const char* mode = "r") {
        if (!present) return {};
        if (!nodes.count(path) && std::string(mode) == "a") seed(path, "");
        return nodes.count(path) ? File(nodes.at(path), std::string(mode) == "a") : File();
    }
};
inline FakeSD SD;
