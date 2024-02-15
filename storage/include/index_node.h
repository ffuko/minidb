#pragma once

#include "record.h"
#include "types.h"
#include <algorithm>
#include <cstdint>
#include <vector>

// #include "storage/common/error.h"
// FIXME: standardize index operation return code
namespace storage {

// IndexHdr contains the unique metadata of a index page.
struct IndexNodeHdr {
    // the id of the underlying index the index page belongs to.
    IndexID id;

    // uint16_t number_of_heap_records;

    // the total number of user records in the index page.
    uint16_t number_of_records;

    // the level of this index page in the index.
    // serves also as an indicator whether the page is a leaf or not.
    int level;

    IndexNodeHdr(IndexID id, int level)
        : id(id), level(level), number_of_records(0) {}
};

// A IndexNode is a node in the index, representing a physical general index
// page.
// NOTE: for a internal index node/page, the key of the first record is
// always set to Infimum (the field type is undefined), the key of the last
// record is always set to Supremum. so the layout of a index's internal level
// will be:
// [            Index Page              ] [             Index Page          ]
// [ Infi <-> key0 <-> ... <-> Supremum ] [ Infi <-> key0 <-> ... <-> Supremum ]
// > the interval of child node of keyN: [keyN, keyN-1)
class IndexNode {
public:
    friend class Index;
    // operation status
    enum class OpStatus { Success, Failure };

    // TODO: placeholder for now; more suitable option.
    static constexpr int MAX_NUMBER_OF_KEYS = 256;

    static constexpr int max_number_of_keys() { return MAX_NUMBER_OF_KEYS; }
    static constexpr int min_number_of_keys() { return MAX_NUMBER_OF_KEYS / 2; }
    static constexpr int max_number_of_childs() {
        return max_number_of_keys() + 1;
    }
    static constexpr int min_number_of_childs() {
        return min_number_of_keys() + 1;
    }

public:
    IndexNode(IndexID index_id, int level)
        : hdr_(index_id, level), infimum(&Infimum), supremum(&Supremum),
          prev_node(nullptr), next_node(nullptr) {}
    ~IndexNode() = default;

    OpStatus insert_record(Record *record);
    OpStatus remove_record(Key key);

private:
    IndexNodeHdr hdr_;

    // the start and the end of the singly linked list of all the records in the
    // index page. NOTE: only serves as a indicator now.
    const Record *infimum, *supremum;

    // TODO: page directory

    // sibiling index nodes in the same level.
    IndexNode *prev_node, *next_node;
};

} // namespace storage

// namespace storage
