#ifndef STORAGE_INCLUDE_TABLE_TABLE_META_H
#define STORAGE_INCLUDE_TABLE_TABLE_META_H

#include "index/record.h"
#include "types.h"
#include <vector>

namespace storage {
struct TableMeta {
    table_id_t id;
    std::string name;
    RecordMeta record_meta;
    std::vector<IndexMeta> indices;
};
} // namespace storage

#endif // !STORAGE_INCLUDE_TABLE_TABLE_META_H
