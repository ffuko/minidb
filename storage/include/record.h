#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

// TODO: is it possible to reduce parameter copy in all record classes
// construction.
namespace storage {

enum class FieldType {
    Undefined,
    Number /* TODO:integer for now; allow floating point */,
    Boolean,
    String
}; // TODO: constants to declare the len of each type; default value

// ColumnMeta contains the metadata of the column, shared by all column
// instances.
struct FieldMeta {
    FieldType type;
    std::string name;
    // the position of the column in the table;
    // table id of which it belongs to;
    bool is_primary;
};

struct Field {
    FieldMeta *meta;

    // the value of the column
    union {
        int int_value;
        bool bool_value;
    } small_value;
    std::string string_value;

    // NOTE:the caller is responsible to ensure the column meta is consistant
    // with the value.
    Field(FieldMeta *meta, std::string value)
        : meta(meta), string_value(value) {}
    // FIXME: union initialization best practice
    Field(FieldMeta *meta, int value)
        : meta(meta), small_value({.int_value = value}) {}
    Field(FieldMeta *meta, bool value)
        : meta(meta), small_value({.bool_value = value}) {}
};

// FIXME: make sure the field's uniqueness
using Key = Field;

using FieldList = std::vector<Field>;

// type of the record.
enum class RecordType { Conventional, Node, Infi, Supre };

// RecordMeta contains the metadata of a kind of record; shared by all records
// of that kind.
struct RecordMeta {
    RecordType type;
    int number_of_columns;
    // FIXME: is the table this record belongs to important?
};

// a record presenets a row of data.
struct RecordHdr {
    RecordMeta *meta;

    // user record starts from 2; Infimum is always order 0, supremum is always
    // order 1.
    // this field is decided when the record is inserted into the page.
    int order;

    // NOTE:status of the record, as a placeholder for now.
    uint8_t flag;

    // number of records owned
    // uint8_t num_records_owned;

    // NOTE: the len of the record must be smaller than 2^16 bytes for now.
    // this field is decided when the record is inserted into the page.
    uint16_t next_record_offset;

    RecordHdr(RecordMeta *meta) : meta(meta) {}
};

class Record {
public:
    Record(RecordMeta *meta) : record_hdr_(meta) {}
    virtual ~Record() = default;

    RecordType type() { return record_hdr_.meta->type; }
    int order() { return record_hdr_.order; }
    uint16_t next_record_offset() { return record_hdr_.next_record_offset; }

    void set_order(int order) { this->record_hdr_.order = order; }
    void set_next_record_offset(uint16_t offset) {
        this->record_hdr_.next_record_offset = offset;
    }

private:
    RecordHdr record_hdr_;
};

// ClusteredRecord represents a record in a clustered index's leaf page.
class ClusteredRecord : public Record {
public:
    ClusteredRecord(RecordMeta *meta, Key key) : Record(meta), key_(key) {
        non_key_fields.reserve(meta->number_of_columns);
    }
    ~ClusteredRecord() {}

    void add_column(Field column) { non_key_fields.push_back(column); }

private:
    Key key_;
    FieldList non_key_fields;
    // transaction ID; roll pointer;
};

// NodeRecord represents a record in a clustered index's non-leaf page.
class NodeRecord : public Record {
public:
    NodeRecord(RecordMeta *meta, Key key) : Record(meta), key_(key) {}
    ~NodeRecord() {}

    void set_child_page_number(uint32_t child_page_number) {
        this->child_page_number_ = child_page_number;
    }

private:
    Key key_;
    uint32_t child_page_number_;
};

// SecondaryRecord represents a record in a secondary index's leaf page.
// secondary record may be non-unique; uniqueness is ensured by the primary key
// field (for both leaf and *internal node*).
class SecondaryRecord : public Record {
public:
    SecondaryRecord(RecordMeta *meta, Field secondary_record, Key key)
        : Record(meta), secondary_record_(secondary_record), key_(key) {}
    ~SecondaryRecord() {}

private:
    Field secondary_record_;
    Key key_;
};

// SecondaryRecord represents a record in a secondary index's non-leaf page.
class SecondaryNodeRecord : public Record {
public:
    SecondaryNodeRecord(RecordMeta *meta, Field secondary_record, Key key)
        : Record(meta), secondary_record_(secondary_record), key_(key) {}
    ~SecondaryNodeRecord() {}

    void set_child_page_number(uint32_t child_page_number) {
        this->child_page_number_ = child_page_number;
    }

private:
    Field secondary_record_;
    // provided to ensure uniqueness
    Key key_;
    uint32_t child_page_number_;
};

} // namespace storage
