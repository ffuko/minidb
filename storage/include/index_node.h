#ifndef STORAGE_INCLUDE_INDEX_NODE_H
#define STORAGE_INCLUDE_INDEX_NODE_H

#include "error.h"
#include "record.h"
#include "types.h"
#include <cstdint>
#include <memory>
#include <tl/expected.hpp>

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
    // FIXME: invalid and inconsistant for now
    int level;

    bool is_leaf;

    IndexNodeHdr(IndexID id, int level, bool is_leaf)
        : id(id), level(level), number_of_records(0), is_leaf(false) {}
};

// A IndexNode is a node in the index, representing a physical general index
// page.
// NOTE: for a internal index node/page, the key of the first record is
// always set to Infimum (the field type is undefined), the key of the last
// record is always set to Supremum. so the layout of a index's internal level
// will be:
// [            Index Page              ] [             Index Page          ]
// [ Infi <-> key0 <-> ... <-> Supremum ] [ Infi <-> key0 <-> ... <-> Supremum ]
// > the interval of child node of keyN: [keyN, keyN+1)
class IndexNode {
public:
    friend class Index;

    // TODO: placeholder for now; more suitable option.
#ifdef DEBUG
    static constexpr int MAX_NUMBER_OF_KEYS = 16;
#else
    static constexpr int MAX_NUMBER_OF_KEYS = 256;
#endif // DEBUG

    static constexpr int max_number_of_keys() { return MAX_NUMBER_OF_KEYS; }
    static constexpr int min_number_of_keys() { return MAX_NUMBER_OF_KEYS / 2; }
    static constexpr int max_number_of_childs() {
        return max_number_of_keys() + 1;
    }
    static constexpr int min_number_of_childs() {
        return min_number_of_keys() + 1;
    }

public:
    IndexNode(IndexID index_id, int level, bool is_leaf)
        : hdr_(index_id, level, is_leaf),
          infimum_(std::make_unique<InfiRecord>()),
          supremum_(std::make_unique<SupreRecord>()), last_inserted_(infimum()),
          prev_node_(nullptr), next_node_(nullptr), parent_node_(nullptr) {
        infimum_->set_next_record(supremum());
    }
    virtual ~IndexNode() = default;

    IndexNode(const IndexNode &index) = delete;
    IndexNode &operator=(const IndexNode &index) = delete;

    tl::expected<Record *, ErrorCode> search_key(const Key &key) {
        Record *record = first_record();

        // inner index node search
        while (!record->is_supremum()) {
            if (key > record->key()) {
                record = record->next_record();
            } else if (key == record->key()) {
                return record;
            } else {
                return tl::unexpected(ErrorCode::KeyNotFound);
            }
        }
        return tl::unexpected(ErrorCode::KeyNotFound);
    }

    ErrorCode insert_record(Record *insert_record) {
        Record *record = infimum();
        while (!record->next_record()->is_supremum() &&
               record->next_record()->key() < insert_record->key()) {
            record = record->next_record();
        }

        if (!record->next_record()->is_supremum() &&
            record->next_record()->key() == insert_record->key()) {
            return ErrorCode::KeyAlreadyExist;
        }

        Record *after_insert = record->next_record();
        record->set_next_record(insert_record);
        insert_record->set_next_record(after_insert);

        hdr_.number_of_records++;
        return ErrorCode::Success;
    }

    ErrorCode remove_record(const Key &key) {
        Record *record = infimum();
        while (!record->next_record()->is_supremum() &&
               record->next_record()->key() != key) {
            record = record->next_record();
        }

        if (record->next_record()->key() != key) {
            return ErrorCode::KeyNotFound;
        } else {
            Record *to_delete = record->next_record();
            Record *after_deleted = record->next_record()->next_record();
            record->set_next_record(after_deleted);

            delete to_delete;
        }
        return ErrorCode::Success;
    }

    void set_parent_node(IndexNode *parent) { this->parent_node_ = parent; }
    IndexNode *parent_node() const { return parent_node_; }

    void set_parent_record(Record *record) { this->parent_record_ = record; }
    Record *parent_record() const { return parent_record_; }

    int level() const { return hdr_.level; }

    IndexNode *next_node() const { return next_node_; }
    IndexNode *prev_node() const { return prev_node_; }

    Record *infimum() const { return infimum_.get(); }
    Record *supremum() const { return supremum_.get(); }

    bool is_full() const {
        return hdr_.number_of_records >= IndexNode::max_number_of_keys();
    }

    bool is_half_full() const {
        return hdr_.number_of_records == (IndexNode::max_number_of_keys() / 2);
    }

    bool is_empty() const { return hdr_.number_of_records == 0; }

    bool is_leaf() const { return hdr_.is_leaf; }

    // return Supremum if the node is empty.
    Record *first_record() { return infimum()->next_record(); }

    // return Infimum if the node is empty. FIXME: use page directory?
    Record *last_record() {
        if (number_of_records() == 0)
            return nullptr;

        Record *record = infimum();
        while (!record->next_record()->is_supremum()) {
            record = record->next_record();
        }
        return record;
    }

    int number_of_records() const { return hdr_.number_of_records; }

    // find a record's previous neighbor; return nullptr if not found.
    Record *prev_record(Record *record) {
        Record *prev_record = first_record();
        while (!prev_record->next_record()->is_supremum() &&
               prev_record->next_record()->key() != record->key())
            prev_record = prev_record->next_record();
        if (prev_record->next_record()->key() == record->key())
            return prev_record;
        else
            return nullptr;
    }

private:
    IndexNodeHdr hdr_;

    // the start and the end of the singly linked list of all the records in the
    // index page. NOTE: only serves as a indicator now.
    std::unique_ptr<Record> supremum_, infimum_;

    // TODO: page directory

    // last inserted record; TODO: how to use it to insert more efficiently.
    Record *last_inserted_;

    IndexNode *parent_node_;
    Record *parent_record_;

    // sibiling index nodes in the same level.
    IndexNode *prev_node_, *next_node_;
};

// class LeafIndexNode : public IndexNode {
// public:
//     LeafIndexNode(IndexID id, int level) : IndexNode(id, level, false) {}
//
// private:
//
// };

} // namespace storage

#endif // !STORAGE_INCLUDE_INDEX_NODE_H
