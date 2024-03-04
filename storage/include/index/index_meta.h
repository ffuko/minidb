#ifndef STORAGE_INCLUDE_INDEX_INDEX_META_H
#define STORAGE_INCLUDE_INDEX_INDEX_META_H

#include "table/record_meta.h"
#include "types.h"
#include <cereal/cereal.hpp>
#include <vector>

namespace storage {

struct IndexMeta {
    index_id_t id;
    bool is_primary;

    page_id_t root_page;
    // the depth of the B+ tree, starting from 1 even if the tree is empty.
    // if a index page's level == the tree's depth, it is a leaf node.
    int depth;

    // for primary index: key + value
    // for secondary index: value + key
    RecordMeta record_meta;
    int number_of_records;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(id, is_primary, root_page, depth, record_meta,
                number_of_records);
    }

    static IndexMeta make_index_meta(index_id_t id, const KeyMeta &key,
                                     const std::vector<FieldMeta> &fields) {
        return IndexMeta{
            .id = id,
            .is_primary = true,
            .root_page = 1,
            .depth = 1,
            .record_meta =
                {
                    .key = key,
                    .fields = fields,
                },
            .number_of_records = 0,
        };
    }
};

} // namespace storage

#endif // !STORAGE_INCLUDE_INDEX_INDEX_META_H
