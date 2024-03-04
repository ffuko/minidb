#ifndef STORAGE_INCLUDE_TABLE_RECORD_META_H
#define STORAGE_INCLUDE_TABLE_RECORD_META_H

#include "types.h"
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <string>
#include <vector>

namespace storage {

struct KeyMeta {
    std::string name;
    key_t type;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(name, type);
    }
};

struct FieldMeta {
    std::string name;
    value_t type;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(name, type);
    }
};

struct RecordMeta {
    KeyMeta key;

    std::vector<FieldMeta> fields;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(key, fields);
    }
};

} // namespace storage

#endif // !STORAGE_INCLUDE_TABLE_RECORD_META_HSTORAGE_INCLUDE_TABLE_RECORD_META_H
