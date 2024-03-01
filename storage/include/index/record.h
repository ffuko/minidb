#ifndef STORAGE_INCLUDE_INDEX_RECORD_H
#define STORAGE_INCLUDE_INDEX_RECORD_H

#include "types.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>
#include <istream>
#include <ostream>
#include <string>

// binary dump of the record's value: <index of the variant> <value of the
// variant's value>

namespace storage {
struct RecordMeta {
    key_t key_type;
    value_t value_type;
};

// RecordHdr is the common header for all records.
struct RecordHdr {
    // the type of a record.
    record_t type = 0;

    // the order of the record in a index page.
    int order = 0;

    // the status of a record, placeholder for now.
    uint8_t status = 0;

    // used so that multiple columns/varidic-length value is allowed
    uint16_t next_record_offset = 0;
    // FIXME: current implementation only allows one column for now; can be
    // extended to multiple columns number_of_columns
    // uint8_t number_of_columns;

    // RecordHdr() : type(0), order(0), status(0), next_record_offset(0) {}

    template <class Archive>
    void serialize(Archive &archive) {
        archive(type, order, status, next_record_offset);
    }

    friend std::ostream &operator<<(std::ostream &os, const RecordHdr &hdr) {
        os << hdr.type << hdr.order << hdr.status << hdr.next_record_offset;
        return os;
    }

    friend std::istream &operator>>(std::istream &is, RecordHdr &hdr) {
        std::string s;
        is >> hdr.type >> hdr.order >> hdr.status >> hdr.next_record_offset;
        return is;
    }

    friend bool operator==(const RecordHdr &lhs, const RecordHdr &rhs) {
        return lhs.type == rhs.type && lhs.order == rhs.order &&
               lhs.status == rhs.status &&
               lhs.next_record_offset == rhs.next_record_offset;
    }
};

struct LeafClusteredRecord {
    RecordHdr hdr;
    Key key;
    Value value;
    LeafClusteredRecord(const RecordHdr &hdr, const Key &key,
                        const Value &value) {}
    LeafClusteredRecord() {}

    template <class Archive>
    void serialize(Archive &archive) {
        archive(hdr, key, value);
    }

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

    // register
    template <class Archive>
    void serialize(Archive &archive) {
        archive(hdr, key);
    }

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

} // namespace storage
#endif // !STORAGE_INCLUDE_INDEX_RECORD_H
