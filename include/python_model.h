#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace portalis {
enum class PythonPhase { Ready, Running, Stopping, Done, Failed, Stopped };
struct PythonStatus { PythonPhase phase=PythonPhase::Ready;bool input=false;char name[96]={},message[96]={}; };
struct ScriptCatalog { std::array<std::string,64> names;size_t count=0;bool limited=false;char message[80]={}; };
inline bool scriptName(const std::string& name) {
    return name.size()>3 && name.size()<=80 && name.find('/')==std::string::npos &&
        name.find('\\')==std::string::npos && name.find('\0')==std::string::npos && name.substr(name.size()-3)==".py";
}
} // namespace portalis
