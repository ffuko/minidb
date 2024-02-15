#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

// TODO: is it possible to reduce parameter copy in all record classes
// construction.
namespace storage {

class IndexNode;

enum class FieldType {
    // a field of Infimum is smaller than any other field in a index node.
    Infimum,
    // a field of Supremum is larger than any other field in a index node.
    Supremum,
    Number /* TODO:integer for now; allow floating point */,
    Boolean,
    String,
    // NOTE: is Undefined type necessary?
};

// FieldMeta contains the metadata of the column, shared by all column
// instances.
struct FieldMeta {
    FieldType type;
    std::string name;
    // the position of the column in the table;
    // table id of which it belongs to;

    bool is_primary;
};

// Field represents one instance of a field which type is specified by the meta.
// NOTE:the caller of the constructor is responsible to ensure the field meta is
// consistant with the provided field value.
struct Field {
    const FieldMeta *meta;

    // the value of the field
    union {
        int int_value;
        bool bool_value;
    } small_value;
    std::string string_value;

    Field(const FieldMeta *meta) : meta(meta) {}
    Field(const FieldMeta *meta, std::string value)
        : meta(meta), string_value(value) {}
    // union initialization best practice?
    Field(const FieldMeta *meta, int value)
        : meta(meta), small_value({.int_value = value}) {}
    Field(const FieldMeta *meta, bool value)
        : meta(meta), small_value({.bool_value = value}) {}

    bool operator==(const Field &rhs) const {
        switch (meta->type) {
        case FieldType::Number:
            return this->small_value.int_value == rhs.small_value.int_value;
        case FieldType::Boolean:
            return this->small_value.bool_value == rhs.small_value.bool_value;
        case FieldType::String:
            return this->string_value == rhs.string_value;
        case FieldType::Infimum:
            return false;
        case FieldType::Supremum:
            return false;
        }
    }

    bool operator!=(const Field &rhs) const { return !(*this == rhs); }

    bool operator<(const Field &rhs) const {
        switch (meta->type) {
        case FieldType::Number:
            return this->small_value.int_value < rhs.small_value.int_value;
        case FieldType::Boolean:
            return this->small_value.bool_value < rhs.small_value.bool_value;
        case FieldType::String:
            return this->string_value < rhs.string_value;
        case FieldType::Infimum:
            return true;
        case FieldType::Supremum:
            return false;
        }
    }

    bool operator>(const Field &rhs) const {
        return !((*this == rhs) || (*this < rhs));
    }
    bool operator<=(const Field &rhs) const { return !(*this > rhs); }

    bool operator>=(const Field &rhs) const { return !(*this < rhs); }
};

// FIXME: make sure the field's uniqueness
using Key = Field;

extern const FieldMeta InfiFieldMeta, SupreFieldMeta;
extern const Key InfimumKey, SupremumKey;

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
    const RecordMeta *meta;

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

    RecordHdr(const RecordMeta *meta) : meta(meta) {}
};

class Record {
public:
public:
    Record(const RecordMeta *meta) : record_hdr_(meta) {}

    virtual ~Record() = default;

    virtual const Key &key() const = 0;

    RecordType type() { return record_hdr_.meta->type; }
    int order() { return record_hdr_.order; }
    uint16_t next_record_offset() { return record_hdr_.next_record_offset; }

    void set_order(int order) { this->record_hdr_.order = order; }
    void set_next_record_offset(uint16_t offset) {
        this->record_hdr_.next_record_offset = offset;
    }

    Record *next_record() { return next; }

private:
    RecordHdr record_hdr_;
    // a link to the next record, creating a singly linked list for all records
    // in the same index page.
    Record *next;
};

struct InfiRecord : public Record {
    virtual const Key &key() const { return key_; }
    Key key_;

    InfiRecord(const RecordMeta *meta, Key key) : Record(meta), key_(key) {}
};

struct SupreRecord : public Record {
    virtual const Key &key() const { return key_; }
    Key key_;

    SupreRecord(const RecordMeta *meta, Key key) : Record(meta), key_(key) {}
};

extern const RecordMeta InfiRecordMeta;
extern const RecordMeta SupreRecordMeta;
// the global variable for all inifmum records
extern const InfiRecord Infimum;
// the global variable for all supremum records
extern const SupreRecord Supremum;

// ClusteredRecord represents a record in a clustered index's leaf page.
class ClusteredRecord : public Record {
public:
    ClusteredRecord(const RecordMeta *meta, Key key) : Record(meta), key_(key) {
        non_key_fields.reserve(meta->number_of_columns);
    }
    ~ClusteredRecord() {}

    void add_column(Field column) { non_key_fields.push_back(column); }

    virtual const Key &key() const { return key_; }

private:
    Key key_;
    FieldList non_key_fields;
    // transaction ID; roll pointer;
};

// NodeRecord represents a record in a clustered index's non-leaf page.
class NodeRecord : public Record {
public:
    NodeRecord(const RecordMeta *meta, Key key) : Record(meta), key_(key) {}
    ~NodeRecord() {}

    void set_child_page(IndexNode *child_page) {
        this->child_node = child_page;
    }

    virtual const Key &key() const { return key_; }

private:
    Key key_;
    IndexNode *child_node;
};

// SecondaryRecord represents a record in a secondary index's leaf page.
// secondary record may be non-unique; uniqueness is ensured by the primary key
// field (for both leaf and *internal node*).
class SecondaryRecord : public Record {
public:
    SecondaryRecord(const RecordMeta *meta, Field secondary_record, Key key)
        : Record(meta), secondary_record_(secondary_record), key_(key) {}
    ~SecondaryRecord() {}

    virtual const Key &key() const { return key_; }

private:
    Field secondary_record_;
    Key key_;
};

// SecondaryRecord represents a record in a secondary index's non-leaf page.
class SecondaryNodeRecord : public Record {
public:
    SecondaryNodeRecord(const RecordMeta *meta, Field secondary_record, Key key)
        : Record(meta), secondary_record_(secondary_record), key_(key) {}
    ~SecondaryNodeRecord() {}

    void set_child_page(IndexNode *child_page) {
        this->child_node = child_page;
    }

    virtual const Key &key() const { return key_; }

private:
    Field secondary_record_;
    // provided to ensure uniqueness
    Key key_;
    IndexNode *child_node;
};

} // namespace storage
