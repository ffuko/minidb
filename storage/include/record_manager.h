#pragma once

#include "record.h"
#include <string>
#include <unordered_map>
namespace storage {
// libraries API, helper functions

class RecordManager {
public:
    RecordManager() {}
    ~RecordManager() = default;

    void register_record(RecordType type, FieldMeta key_meta,
                         FieldMeta column_metas...) {}

private:
    std::unordered_map<std::string, RecordMeta *> records_;
};

} // namespace storage
