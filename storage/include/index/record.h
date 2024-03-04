#ifndef STORAGE_INCLUDE_INDEX_RECORD_H
#define STORAGE_INCLUDE_INDEX_RECORD_H

#include "serialization.h"
#include "types.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>
#include <cstdint>

namespace storage {

// RecordHdr is the common header for all records.
struct RecordHdr {
    // the order of the record in a index page.
    int order = 0;

    // the status of a record, placeholder for now.
    uint8_t status = 0;

    // make all records a double linked list
    int prev_record_offset = 0;
    int next_record_offset = 0;

    uint16_t length = 0;
    // FIXME: current implementation does not allow null column.
    // uint8_t number_of_columns;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(order, status, prev_record_offset, next_record_offset, length);
    }

    friend bool operator==(const RecordHdr &lhs, const RecordHdr &rhs) {
        return lhs.order == rhs.order && lhs.status == rhs.status &&
               lhs.prev_record_offset == rhs.prev_record_offset &&

               lhs.next_record_offset == rhs.next_record_offset &&
               lhs.length == rhs.length;
    }
};

// FIXME: use the same type of leaf/internel record for infimum and supremum for
// now -> bloated.
// struct Infimum {
//     RecordHdr hdr;
//     char flag[8];
//
//     Infimum() : hdr(), flag("infimum") {
//         hdr.order = 0;
//         // FIXME: determine the record size after cereal's serialization.
//         hdr.next_record_offset = sizeof(RecordHdr) + 8;
//     }
//
//     template <class Archive>
//     void serialize(Archive &archive) {
//         archive(hdr, flag);
//     }
// };
// struct Supremum {
//     RecordHdr hdr;
//     char flag[8];
//
//     Supremum() : hdr(), flag("supremu") {
//         hdr.order = 1;
//         // FIXME: determine the record size after cereal's serialization.
//         hdr.prev_record_offset = sizeof(RecordHdr) + 8;
//     }
//
//     template <class Archive>
//     void serialize(Archive &archive) {
//         archive(hdr, flag);
//     }
// };

struct LeafClusteredRecord {
    RecordHdr hdr;
    Key key;
    Column value;
    LeafClusteredRecord() {}

    template <class Archive>
    void serialize(Archive &archive) {
        archive(hdr, key, value);
    }

    uint16_t len() const { return hdr.length; }
    // FIXME:
    // friend std::ostream &operator<<(std::ostream &os,
    //                                 const LeafClusteredRecord &record) {
    //     os << record.hdr << record.key << record.value;
    //     return os;
    // }
    //
    // friend std::istream &operator>>(std::istream &is,
    //                                 LeafClusteredRecord &record) {
    //     is >> record.hdr >> record.key >> record.value;
    //     return is;
    // }
};

struct InternalClusteredRecord {
    RecordHdr hdr;
    Key key;
    page_id_t child;

    // register
    template <class Archive>
    void serialize(Archive &archive) {
        archive(hdr, key, child);
    }

    InternalClusteredRecord() {}
    InternalClusteredRecord(const Key &key, page_id_t child)
        : hdr(), key(key), child(child) {}

    uint16_t len() const { return hdr.length; }
    // friend std::ostream &operator<<(std::ostream &os,
    //                                 const InternalClusteredRecord &record) {
    //     os << record.hdr << record.key;
    //     return os;
    // }
    //
    // friend std::istream &operator>>(std::istream &is,
    //                                 InternalClusteredRecord &record) {
    //     is >> record.hdr >> record.key;
    //     return is;
    // }
};

std::ostream inline &operator<<(std::ostream &os,
                                const storage::LeafClusteredRecord &value) {
    serialization::serialize(os, value);
    return os;
}

std::istream inline &operator>>(std::istream &is,
                                storage::LeafClusteredRecord &value) {
    serialization::deserialize(is, value);
    return is;
}

std::ostream inline &operator<<(std::ostream &os,
                                const storage::InternalClusteredRecord &value) {
    serialization::serialize(os, value);
    return os;
}

std::istream inline &operator>>(std::istream &is,
                                storage::InternalClusteredRecord &value) {
    serialization::deserialize(is, value);
    return is;
}
} // namespace storage
#endif // !STORAGE_INCLUDE_INDEX_RECORD_H
