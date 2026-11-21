#pragma once
#include "workbench_model.h"
#include "radar.h"
#include <SD.h>
namespace portalis {
// Commit a fully written temporary file, keeping an existing target until rename succeeds.
bool commitSdFile(const std::string& temporary,const std::string& target,bool overwrite);
bool loadEditor(ScriptEditor&,const std::string& name,bool create);
bool saveEditor(ScriptEditor&);
class SurveyRecorder {
    File file_;SurveyStatus status_;uint32_t started_=0,generation_=0,bytes_=0;
    bool write(const std::string& row);
public:
    bool start(uint32_t now,uint32_t generation);
    void observe(const RadarSnapshot&,uint32_t now);
    void stop();
    const SurveyStatus& status() const {return status_;}
};
} // namespace portalis
