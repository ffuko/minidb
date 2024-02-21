#ifndef STORAGE_INCLUDE_RECORD_H
#define STORAGE_INCLUDE_RECORD_H

#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <sys/types.h>
#include <vector>

namespace storage {

class IndexNode;

// FieldMeta contains the metadata of a kind of field, shared by all field
// instances of the same kind.
struct FieldMeta {
    enum class Type {
        // a field of Infimum is smaller than any other field in a index node.
        Infimum,
        // a field of Supremum is larger than any other field in a index node.
        Supremum,
        Number /* TODO:integer for now; allow floating point */,
        Boolean,
        String,
        // NOTE: is Undefined type necessary?
    };

    Type type;
    std::string name;
    // the position of the column in the table;
    // table id of which it belongs to;

    bool is_primary;

    FieldMeta(Type type, const std::string &name, bool is_primary)
        : type(type), name(name), is_primary(is_primary) {}

    static FieldMeta register_string_field(const std::string &name,
                                           bool is_primary) {
        return FieldMeta(Type::String, name, is_primary);
    }

    static FieldMeta register_number_field(const std::string &name,
                                           bool is_primary) {
        return FieldMeta(Type::Number, name, is_primary);
    }

    static FieldMeta register_boolean_field(const std::string &name,
                                            bool is_primary) {
        return FieldMeta(Type::Boolean, name, is_primary);
    }
};

// Field is a instance of one kind of field.
// NOTE:the caller of the constructor is responsible to ensure the field meta is
// consistant with the provided field value.
struct Field {
    // the value of the field
    struct Value {
        union {
            int int_value;
            bool bool_value;
        } small_value;
        std::string string_value;

        Value() {}
        Value(const std::string &value) : string_value(value) {}
        Value(const int value) : small_value({.int_value = value}) {}
        Value(const bool value) : small_value({.bool_value = value}) {}
    };

    const FieldMeta *meta;
    Value value;

    Field(const FieldMeta *meta) : meta(meta) {}
    Field(const FieldMeta *meta, const std::string &value)
        : meta(meta), value(value) {}
    // union initialization best practice?
    Field(const FieldMeta *meta, int value) : meta(meta), value(value) {}
    Field(const FieldMeta *meta, bool value) : meta(meta), value(value) {}

    // FIXME: make sure lhs and rhs are of the same field type
    bool operator==(const Field &rhs) const {
        switch (meta->type) {
        case FieldMeta::Type::Number:
            return this->value.small_value.int_value ==
                   rhs.value.small_value.int_value;
        case FieldMeta::Type::Boolean:
            return this->value.small_value.bool_value ==
                   rhs.value.small_value.bool_value;
        case FieldMeta::Type::String:
            return this->value.string_value == rhs.value.string_value;
        case FieldMeta::Type::Infimum:
            return false;
        case FieldMeta::Type::Supremum:
            return false;
        default:
            return false;
        }
    }

    bool operator!=(const Field &rhs) const { return !(*this == rhs); }

    bool operator<(const Field &rhs) const {
        switch (meta->type) {
        case FieldMeta::Type::Number:
            return this->value.small_value.int_value <
                   rhs.value.small_value.int_value;
        case FieldMeta::Type::Boolean:
            return this->value.small_value.bool_value <
                   rhs.value.small_value.bool_value;
        case FieldMeta::Type::String:
            return this->value.string_value < rhs.value.string_value;
        case FieldMeta::Type::Infimum:
            return true;
        case FieldMeta::Type::Supremum:
            return false;
        default:
            return false;
        }
    }

    bool operator>(const Field &rhs) const {
        return !((*this == rhs) || (*this < rhs));
    }
    bool operator<=(const Field &rhs) const { return !(*this > rhs); }

    bool operator>=(const Field &rhs) const { return !(*this < rhs); }

    FieldMeta::Type type() const { return meta->type; }

    friend std::ostream &operator<<(std::ostream &os, const Field &field) {
        switch (field.type()) {
        case FieldMeta::Type::Number:
            os << std::format("[number {}]", field.value.small_value.int_value);
            break;
        case FieldMeta::Type::Boolean:
            os << std::format("[boolean {}]",
                              field.value.small_value.bool_value);
            break;
        case FieldMeta::Type::String:
            os << std::format("[number {}]", field.value.string_value);
            break;
        case FieldMeta::Type::Infimum:
            os << "[infimum]";
            break;
        case FieldMeta::Type::Supremum:
            os << "[supremum]";
            break;
        default:
            os << "[unknown???]";
            break;
        }
        return os;
    }
};

// Key is an alias of Field. FIXME: make sure the field's uniqueness
using Key = Field;
using FieldList = std::vector<Field>;

// RecordMeta contains the metadata of a kind of record; shared by all records
// of that kind.
struct RecordMeta {
    // type of the record.
    enum class Type { Common, Node, Infi, Supre };

    Type type;
    int number_of_columns;
    // FIXME: does the table this record belongs to matter?

    RecordMeta(const Type type, const int number_of_columns)
        : type(type), number_of_columns(number_of_columns) {}

    // static new_string_record_meta();
    // static new_string_record_meta()
    // static new_string_record_meta()
};

class Record {
public:
public:
    friend class Index;
    friend class IndexNode;

    // only set meta when constructing
    Record(const RecordMeta *meta)
        : meta_(meta), order_(0), next_record_offset_(0),
          next_(nullptr) /* , prev_(nullptr) */
    {}

    virtual ~Record() = default;

    virtual const Key &key() const = 0;
    virtual void set_key(const Key &key) = 0;

    RecordMeta::Type type() const { return meta_->type; }

    int order() { return order_; }
    void set_order(int order) { this->order_ = order; }

    uint16_t next_record_offset() { return next_record_offset_; }
    void set_next_record_offset(uint16_t offset) {
        this->next_record_offset_ = offset;
    }

    Record *next_record() const { return next_; }
    void set_next_record(Record *next) { this->next_ = next; }
    // Record *prev_record() const { return prev_; }
    // void set_prev_record(Record *prev) { this->prev_ = prev; }

    virtual bool is_leaf() const = 0;

    const RecordMeta *meta() const { return meta_; }

    bool is_supremum() const { return type() == RecordMeta::Type::Supre; }
    bool is_infimum() const { return type() == RecordMeta::Type::Infi; }

protected:
    const RecordMeta *meta_;

    // user record starts from 2; Infimum is always order 0, supremum is always
    // order 1.
    // this field is decided when the record is inserted into the page.
    int order_;

    // NOTE:status of the record, as a placeholder for now.
    uint8_t flag_;

    // number of records owned
    // uint8_t num_records_owned;

    // NOTE: the len of the record must be smaller than 2^16 bytes for now.
    // this field is decided when the record is inserted into the page.
    uint16_t next_record_offset_;
    // a link to the next record, creating a singly linked list for all records
    // in the same index node.
    Record *next_;
    // Record *prev_;
};

struct InfiRecord : public Record {
    static inline const RecordMeta InfiMeta =
        RecordMeta(RecordMeta::Type::Infi, 0);
    static inline const FieldMeta InfiKeyMeta =
        FieldMeta(FieldMeta::Type::Infimum, "infimum", false);
    static inline const Key InfiKey = Key(&InfiKeyMeta);

    InfiRecord() : Record(&InfiMeta) {}

    virtual const Key &key() const { return InfiKey; }

    virtual bool is_leaf() const { return false; }

private:
    virtual void set_key(const Key &key) {}
};

struct SupreRecord : public Record {
    // virtual const Key &key() const { return key_; }
    static inline const RecordMeta SupreMeta =
        RecordMeta(RecordMeta::Type::Supre, 0);
    static inline const FieldMeta SupreKeyMeta =
        FieldMeta(FieldMeta::Type::Supremum, "supremum", false);
    static inline const Key SupreKey = Key(&SupreKeyMeta);
    // Key key_;

    SupreRecord() : Record(&SupreMeta) {}

    virtual const Key &key() const { return SupreKey; }

    virtual bool is_leaf() const { return false; }

private:
    virtual void set_key(const Key &key) {}
};

// ClusteredRecord represents a record in a clustered index's leaf page.
class ClusteredRecord : public Record {
public:
    ClusteredRecord(const RecordMeta *meta, const Key &key)
        : Record(meta), key_(key) {
        non_key_fields.reserve(meta->number_of_columns);
    }

    ~ClusteredRecord() = default;

    // TODO: use emplace?
    void add_column(Field column) { non_key_fields.push_back(column); }

    virtual const Key &key() const { return key_; }
    virtual void set_key(const Key &key) { key_ = key; }

    virtual bool is_leaf() const { return true; }

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

    void set_child_node(IndexNode *child_node) {
        this->child_node_ = child_node;
    }
    IndexNode *child_node() const { return child_node_; }

    virtual const Key &key() const { return key_; }
    virtual void set_key(const Key &key) { key_ = key; }

    virtual bool is_leaf() const { return false; }

private:
    Key key_;
    IndexNode *child_node_;
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
    virtual void set_key(const Key &key) { key_ = key; }

    virtual bool is_leaf() const { return true; }

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

    virtual void set_key(const Key &key) { key_ = key; }

private:
    Field secondary_record_;
    // provided to ensure uniqueness
    Key key_;
    IndexNode *child_node;
};

} // namespace storage

#endif
