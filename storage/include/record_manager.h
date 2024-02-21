#ifndef STORAGE_INCLUDE_RECORD_MANAGER_H
#define STORAGE_INCLUDE_RECORD_MANAGER_H

#include "record.h"
#include <string>
#include <unordered_map>
namespace storage {
// libraries API, helper functions

class RecordManager {
public:
    RecordManager() {}
    ~RecordManager() = default;

    void register_record(RecordMeta::Type type, FieldMeta key_meta,
                         FieldMeta column_metas...) {}

private:
    std::unordered_map<std::string, RecordMeta *> records_;
};

} // namespace storage

#endif
