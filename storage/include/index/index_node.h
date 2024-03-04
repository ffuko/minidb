#ifndef STORAGE_INCLUDE_INDEX_NODE_H
#define STORAGE_INCLUDE_INDEX_NODE_H

#include "buffer/buffer_pool.h"
#include "buffer/frame.h"
#include "config.h"
#include "error.h"
#include "index/cursor.h"
#include "noncopyable.h"
#include "record.h"
#include "types.h"
#include <pthread.h>
#include <tl/expected.hpp>

namespace storage {

template <typename REC, typename V>
void record_value_assign(REC &record, const V &value);

template <>
void inline record_value_assign<LeafClusteredRecord, Column>(
    LeafClusteredRecord &record, const Column &value) {
    record.value = value;
}

template <>
void inline record_value_assign<InternalClusteredRecord, page_id_t>(
    InternalClusteredRecord &record, const page_id_t &value) {
    record.child = value;
}

// A IndexNode is a index page handler in the index, responsible for logical
// operations on a index page.
// NOTE: for a internal index page, the
// key of the first record is always set to Infimum, the key of the last record
// is always set to Supremum.
template <typename N, typename R>
class IndexNode : public NonCopyable {
public:
    friend class Index;

    static constexpr int max_number_of_records() {
        return config::MAX_NUMBER_OF_RECORDS_PER_PAGE;
    }
    static constexpr int min_number_of_records() {
        return config::MAX_NUMBER_OF_RECORDS_PER_PAGE / 2;
    }

    // NOTE: in this implementation, the number of records == the number of
    // childs.
    static constexpr int max_number_of_childs() {
        return max_number_of_records() + 1;
    }
    static constexpr int min_number_of_childs() {
        return min_number_of_records() + 1;
    }

public:
    IndexNode(Frame *frame, Comparator &comp) : frame_(frame), comp_(comp) {}
    virtual ~IndexNode() = default;

    // ErrorCode remove_record(const Key &key) {
    //     Record *record = infimum();
    //     while (!record->next_record()->is_supremum() &&
    //            record->next_record()->key() != key) {
    //         record = record->next_record();
    //     }
    //
    //     if (record->next_record()->key() != key) {
    //         return ErrorCode::KeyNotFound;
    //     } else {
    //         Record *to_delete = record->next_record();
    //         Record *after_deleted = record->next_record()->next_record();
    //         record->set_next_record(after_deleted);
    //
    //         delete to_delete;
    //     }
    //     return ErrorCode::Success;
    // }
    //
    // void set_parent_node(IndexNode *parent) { this->parent_node_ = parent; }
    // IndexNode *parent_node() const { return parent_node_; }
    //
    // void set_parent_record(Record *record) { this->parent_record_ = record; }

    // InternalClusteredRecord parent_record() {
    //     auto parent_page = frame_->page()->hdr.parent_page;
    //     auto offset = frame_->page()->hdr.parent_record_off;
    // }

    int level() const { return frame_->page()->hdr.level; }

    bool is_full() const {
        return number_of_records() >= IndexNode::max_number_of_records();
    }

    bool is_half_full() const {
        return number_of_records() == (IndexNode::max_number_of_records() / 2);
    }

    bool is_empty() const { return frame_->page()->hdr.number_of_records == 0; }

    bool is_leaf() const { return frame_->page()->hdr.is_leaf; }

    int number_of_records() const {
        return frame_->page()->hdr.number_of_records;
    }

    // search for the left sibling or the desired key, whose is <= desired
    // record.
    // FIXME: use binary search?
    tl::expected<Cursor<R>, ErrorCode> get_cursor(const Key &key) {
        R record;
        page_off_t cur;
        // NOTE: first record of an internal index page is always the minimal
        // and meaningless.
        first_record(record);
        next_record(record);

        int i = 0;
        // found the right sibling
        while (i < number_of_records()) {
            if (record.key.index() != key.index())
                return tl::unexpected(ErrorCode::InvalidKeyType);
            else if (record.key == key)
                return Cursor<R>{frame_->pgno(), frame_->gpos() - record.len(),
                                 record};
            else if (record.key > key)
                break;

            next_record(record);
            i++;
        }

        prev_record(record);
        return Cursor<R>{frame_->pgno(), frame_->gpos() - record.len(), record};
    }

    // get the record whose key == @key
    tl::expected<R, ErrorCode> search_key(const Key &key) {
        R record;
        // NOTE: first record of an internal index page is always the minimal
        // and meaningless.
        first_record(record);
        next_record(record);

        int i = 0;
        // found the the desired record.
        while (i < number_of_records()) {
            if (record.key.index() != key.index())
                return tl::unexpected(ErrorCode::InvalidKeyType);
            else if (record.key == key)
                return record;
            else if (record.key > key)
                break;

            next_record(record);
            i++;
        }

        return tl::unexpected(ErrorCode::KeyNotFound);
    }

    template <typename V>
    tl::expected<page_off_t, ErrorCode> insert_record(const Key &key,
                                                      const V &value) {
        R record;
        // NOTE: first record of an internal index page is always the minimal
        // and meaningless.
        first_record(record);
        next_record(record);

        int i = 0;
        // found the right sibling
        while (i < number_of_records()) {
            if (record.key.index() != key.index())
                return tl::unexpected(ErrorCode::InvalidKeyType);
            else if (record.key == key)
                return tl::unexpected(ErrorCode::KeyAlreadyExist);
            else if (record.key > key)
                break;

            next_record(record);
            i++;
        }

        prev_record(record);
        auto result = insert_record_after(record, key, value);
        return result;
    }

    template <typename V>
    page_off_t insert_record_after(R &left_record, const Key &key,
                                   const V &value) {

        page_off_t left_start = frame_->gpos() - left_record.len();

        R right_record = left_record;
        next_record(right_record);
        page_off_t right_start = frame_->gpos() - right_record.len();

        R record;
        record.hdr = {
            .order = left_record.hdr.order + 1,
        };
        record.key = key;
        record_value_assign(record, value);

        record.hdr.length = frame_->dump_at(frame_->last_inserted(), record);

        auto record_end = frame_->ppos();
        Log::GlobalLog() << "the end of the last dump: " << record_end
                         << std::endl;
        Log::GlobalLog() << "the start of the left record: " << left_start
                         << std::endl;
        Log::GlobalLog() << "the start of the right record: " << right_start
                         << std::endl;
        record.hdr.next_record_offset = right_start - record_end;
        record.hdr.prev_record_offset = left_start - record_end;
        left_record.hdr.next_record_offset =
            (record_end - record.len()) - (left_start + left_record.len());
        right_record.hdr.prev_record_offset =
            (record_end - record.hdr.length) -
            (right_start + right_record.hdr.length);

        // FIXME: cereal serialization determination.
        frame_->dump_at(frame_->last_inserted(), record);
        frame_->dump_at(left_start, left_record);
        frame_->dump_at(right_start, right_record);

        frame_->set_last_inserted(record_end);
        frame_->page()->hdr.number_of_records++;

        return record_end;
    }

    ErrorCode node_split(N &node, int n1, int n2) {
        // page header update
        if (!node.frame_->is_full())
            return ErrorCode::NodeNotFull;
        {
            auto frame = node.frame_;
            frame->page()->hdr.index = frame_->page()->hdr.index;
            frame->page()->hdr.level = frame_->page()->hdr.level;
            frame->page()->hdr.is_leaf = frame_->page()->hdr.is_leaf;

            int i = 0;
            R record;
            first_record(record);
            while (i < n1) {
                next_record(record);
                i++;
            }

            i = 0;
            while (i < n2) {
                node.insert_record(record);
                next_record(record);
                i++;
            }
        }
        // record copy
    }

protected:
    // get the first record of a page(a placeholder), and store to @record.
    void first_record(R &record) { frame_->load_at(0, record); }

    // make @record point to its prev record.
    void prev_record(R &record) {
        int offset = record.hdr.prev_record_offset;
        frame_->load(offset, record);
    }

    // make @record point to its next record.
    void next_record(R &record) {
        int offset = record.hdr.next_record_offset;
        frame_->load(offset, record);
    }

protected:
    Frame *frame_;
    // *placeholder* now, reserved to add comparison support for secondary
    // record.
    Comparator &comp_;

    // TODO: page directory

    // IndexNode *parent_node_;
};

class InternalIndexNode
    : public IndexNode<InternalIndexNode, InternalClusteredRecord> {
public:
    InternalIndexNode(Frame *frame, Comparator &comp)
        : IndexNode(frame, comp) {}
    virtual ~InternalIndexNode() = default;

    // insert the {@key, @value} pair into the page.
    // @return is the absolute offset of the inserted record.
    // NOTE: it is the caller(Index)'s responsibility to ensure the node is not
    // full before insert.
    // tl::expected<page_off_t, ErrorCode> insert_record(const Key &key,
    //                                                   page_id_t child);
    //
    // page_off_t insert_record_after(InternalClusteredRecord &left_record,
    //                                const Key &key, page_id_t value);
};

class LeafIndexNode : public IndexNode<LeafIndexNode, LeafClusteredRecord> {
public:
    LeafIndexNode(Frame *frame, Comparator &comp) : IndexNode(frame, comp) {}
    virtual ~LeafIndexNode() = default;

    // insert the {@key, @value} pair into the page.
    // @return is the absolute offset of the inserted record.
    // NOTE: it is the caller(Index)'s responsibility to ensure the node is not
    // full before insert.
    // tl::expected<page_off_t, ErrorCode> insert_record(const Key &key,
    //                                                   const Column &value);
    //
    // page_off_t insert_record_after(LeafClusteredRecord &left_record,
    //                                const Key &key, const Column &value);
};

} // namespace storage

#endif // !STORAGE_INCLUDE_INDEX_NODE_H
