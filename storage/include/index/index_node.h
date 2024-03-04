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
    record.value = value;
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

    struct NodeCursor {
        // the end of the record.
        page_off_t offset;
        R record;
    };

    using TraverseFunc = std::function<void(R &record)>;

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

    // return the first key of the node.
    // if the node is empty, return zero value.
    Key key() {
        if (number_of_records() == 0)
            return Key{};
        auto first = first_user_cursor();
        return first.record.key;
    }

    // search for the left sibling or the desired key, whose is <= desired
    // record.
    // FIXME: use binary search?
    tl::expected<Cursor<R>, ErrorCode> get_cursor(const Key &key) {
        auto cursor = first_user_cursor();

        int i = 0;
        // found the right sibling
        while (i < number_of_records()) {
            auto &record = cursor.record;
            if (record.key.index() != key.index())
                return tl::unexpected(ErrorCode::InvalidKeyType);
            else if (record.key == key)
                return Cursor<R>{frame_->pgno(), cursor.offset, cursor.record};
            else if (record.key > key)
                break;

            cursor = next_cursor(cursor);
            i++;
        }

        cursor = prev_cursor(cursor);
        return Cursor<R>{frame_->pgno(), cursor.offset, cursor.record};
    }

    // get the record whose key == @key
    tl::expected<R, ErrorCode> search_record(const Key &key) {
        R record;
        // NOTE: first record of an internal index page is always the minimal
        // and meaningless.
        auto cursor = first_user_cursor();

        int i = 0;
        // found the the desired record.
        while (i < number_of_records()) {
            auto &record = cursor.record;
            if (record.key.index() != key.index())
                return tl::unexpected(ErrorCode::InvalidKeyType);
            else if (record.key == key)
                return record;
            else if (record.key > key)
                break;

            cursor = next_cursor(cursor);
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
        auto cursor = first_user_cursor();

        int i = 0;
        // found the right sibling
        while (i < number_of_records()) {
            auto &record = cursor.record;
            if (record.key.index() != key.index())
                return tl::unexpected(ErrorCode::InvalidKeyType);
            else if (record.key == key)
                return tl::unexpected(ErrorCode::KeyAlreadyExist);
            else if (record.key > key)
                break;

            cursor = next_cursor(cursor);
            i++;
        }

        auto result = insert_record_before(cursor, key, value);
        Log::GlobalLog() << "the number of records after insert: "
                         << frame_->number_of_records() << std::endl;
        return result.offset;
    }

    tl::expected<NodeCursor, ErrorCode> remove_record(const Key &key) {
        R record;
        // NOTE: first record of an internal index page is always the minimal
        // and meaningless.
        auto cursor = first_user_cursor();

        int i = 0;
        // found the the desired record.
        while (i < number_of_records()) {
            R &record = cursor.record;
            if (record.key.index() != key.index())
                return tl::unexpected(ErrorCode::InvalidKeyType);
            else if (record.key == key) {
                auto result = remove_record(cursor);
                Log::GlobalLog() << "the number of records after remove: "
                                 << frame_->number_of_records() << std::endl;

                return result;
            } else if (record.key > key)
                break;

            cursor = next_cursor(cursor);
            i++;
        }

        return tl::unexpected(ErrorCode::KeyNotFound);
    }

    template <typename V>
    page_off_t push_back(const Key &key, const V &value) {
        auto supre = last_cursor();
        auto result = insert_record_before(supre, key, value);
        return result.offset;
    }

    template <typename V>
    page_off_t push_front(const Key &key, const V &value) {
        auto first = first_user_cursor();

        auto result = insert_record_before(first, key, value);
        return result.offset;
    }

    tl::expected<R, ErrorCode> pop_back() {
        if (number_of_records() == 0)
            return tl::unexpected(ErrorCode::PopEmptyNode);
        auto last = last_user_cursor();
        auto left = prev_cursor(last);
        auto right = next_cursor(last);

        auto &record = last.record;
        auto &right_record = right.record;
        auto &left_record = left.record;

        left_record.hdr.next_record_offset = offset(left, right);
        right_record.hdr.next_record_offset = offset(right, left);
        record.hdr.status = uint8_t(config::RecordStatus::Deleted);

        dump(last);
        dump(left);
        dump(right);
        frame_->page()->hdr.number_of_records--;

        return record;
    }

    tl::expected<R, ErrorCode> pop_front() {
        if (number_of_records() == 0)
            return tl::unexpected(ErrorCode::PopEmptyNode);
        auto last = first_user_cursor();
        auto left = prev_cursor(last);
        auto right = next_cursor(last);
        auto &record = last.record;
        auto &right_record = right.record;
        auto &left_record = left.record;

        left_record.hdr.next_record_offset = offset(left, right);
        right_record.hdr.next_record_offset = offset(right, left);
        record.hdr.status = uint8_t(config::RecordStatus::Deleted);

        dump(last);
        dump(left);
        dump(right);
        frame_->page()->hdr.number_of_records--;

        return record;
    }
    // void set_parent_node(IndexNode *parent) { this->parent_node_ = parent; }
    // IndexNode *parent_node() const { return parent_node_; }
    //
    // void set_parent_record(Record *record) { this->parent_record_ = record; }

    // InternalClusteredRecord parent_record() {
    //     auto parent_page = frame_->page()->hdr.parent_page;
    //     auto offset = frame_->page()->hdr.parent_record_off;
    // }

protected:
    template <typename V>
    NodeCursor insert_record_after(NodeCursor &left, const Key &key,
                                   const V &value) {
        auto right = next_cursor(left);
        R &right_record = right.record;
        R &left_record = left.record;

        NodeCursor inserted{};
        R &record = inserted.record;
        record.hdr = {
            // .order = left_record.hdr.order + 1,
        };
        record.key = key;
        record_value_assign(record, value);

        record.hdr.length = frame_->dump_at(frame_->last_inserted(), record);

        inserted.offset = frame_->ppos();
        Log::GlobalLog() << "the end of the last dump: " << frame_->ppos()
                         << std::endl;
        record.hdr.next_record_offset = offset(inserted, right);
        record.hdr.prev_record_offset = offset(inserted, left);
        left_record.hdr.next_record_offset = offset(left, inserted);
        right_record.hdr.prev_record_offset = offset(right, inserted);

        // FIXME: cereal serialization determination.
        dump(inserted);
        dump(left);
        dump(right);

        frame_->set_last_inserted(inserted.offset);
        frame_->page()->hdr.number_of_records++;

        return inserted;
    }

    template <typename V>
    NodeCursor insert_record_before(NodeCursor &right_cursor, const Key &key,
                                    const V &value) {
        NodeCursor left_cursor = prev_cursor(right_cursor);
        auto &left_record = left_cursor.record;
        auto &right_record = right_cursor.record;

        NodeCursor inserted{0, R{}};
        R &record = inserted.record;
        record.hdr = {
            // .order = left_record.hdr.order + 1,
        };
        record.key = key;
        record_value_assign(record, value);

        record.hdr.length = frame_->dump_at(frame_->last_inserted(), record);
        inserted.offset = frame_->ppos();

        Log::GlobalLog() << "the end of the inserted record: " << frame_->ppos()
                         << std::endl;
        record.hdr.next_record_offset = offset(inserted, right_cursor);
        record.hdr.prev_record_offset = offset(inserted, left_cursor);
        left_record.hdr.next_record_offset = offset(left_cursor, inserted);
        right_record.hdr.prev_record_offset = offset(right_cursor, inserted);

        // FIXME: cereal serialization determination.
        dump(left_cursor);
        dump(inserted);
        dump(right_cursor);

        frame_->set_last_inserted(inserted.offset);
        frame_->page()->hdr.number_of_records++;

        return inserted;
    }

    ErrorCode node_split(N &node, int n1, int n2) {
        // page header update
        if (!node.frame_->is_full())
            return ErrorCode::NodeNotFull;
        {
            auto frame = node.frame_;

            int i = 0;
            while (i < n2) {
                auto result = pop_back();
                if (!result)
                    return result.error();
                auto &record = result.value();
                node.insert_record(record.key, record.value);
                i++;
            }
        }
        // record copy
        return ErrorCode::Success;
    }

    void remove_record(R &record) {
        auto record_end = frame_->ppos();

        R left_record = make_prev_record(record, record_end);
        page_off_t left_start = frame_->gpos() - left_record.len();

        R right_record = make_next_record(record, record_end);
        page_off_t right_start = frame_->gpos() - right_record.len();

        Log::GlobalLog() << "the end of the to-be-deleted record: "
                         << record_end << std::endl;
        Log::GlobalLog() << "the start of the left record: " << left_start
                         << std::endl;
        Log::GlobalLog() << "the start of the right record: " << right_start
                         << std::endl;
        left_record.hdr.next_record_offset =
            right_start - (left_start + left_record.len());
        right_record.hdr.prev_record_offset =
            left_start - (right_start + right_record.hdr.length);
        // FIXME: lazy delete, need a pruning procedure.
        record.hdr.status = uint8_t(config::RecordStatus::Deleted);

        // FIXME: cereal serialization determination.
        frame_->dump_at(record_end - record.len(), record);
        frame_->dump_at(left_start, left_record);
        frame_->dump_at(right_start, right_record);

        frame_->page()->hdr.number_of_records--;
    }

    NodeCursor remove_record(NodeCursor &cursor) {
        NodeCursor left_cursor = prev_cursor(cursor);
        NodeCursor right_cursor = next_cursor(cursor);

        Log::GlobalLog() << "the end offset of the to-be-deleted record: "
                         << cursor.offset << std::endl;
        auto &left_record = left_cursor.record;
        auto &right_record = right_cursor.record;
        left_record.hdr.next_record_offset = offset(left_cursor, right_cursor);
        right_record.hdr.prev_record_offset = offset(right_cursor, left_cursor);
        // FIXME: lazy delete, need a pruning procedure.
        cursor.record.hdr.status = uint8_t(config::RecordStatus::Deleted);

        // FIXME: cereal serialization determination.
        dump(cursor);
        dump(left_cursor);
        dump(right_cursor);

        frame_->page()->hdr.number_of_records--;
        return cursor;
    }

    void node_union(N &node) {
        auto cursor = first_user_cursor();
        int i = 0;
        while (i < node.frame_->number_of_records()) {
            auto &record = cursor.record;
            push_back(record.key, record.value);
            i++;
        }
    }

    // record forward traverse in a node.
    void traverse(const TraverseFunc &func) {
        auto cursor = first_user_cursor();

        int i = 0;
        while (i < frame_->number_of_records()) {
            func(cursor.record);
            cursor = next_cursor(cursor);
            i++;
        }
    }

    // record reverse traverse in a node.
    void traverse_r(const TraverseFunc &func) {

        auto cursor = last_user_cursor();

        int i = 0;
        while (i < frame_->number_of_records()) {
            func(cursor.record);
            cursor = prev_cursor(cursor);
            i++;
        }
    }

protected:
    // // retur Infimum record.
    // void first_record(R &record) { frame_->load_at(0, record); }
    // // return Supreme record.
    // void last_record(R &record) {
    //     frame_->load_at(0, record);
    //     frame_->load(record);
    // }
    // // NOTE: it is the caller's responsibility to ensure the node is not
    // // empty(no any user records).
    // R first_user_record() {
    //     R record;
    //     first_record(record);
    //     next_record(record);
    //     return record;
    // }
    //
    // // make @record point to its prev record.
    // void prev_record(R &record) {
    //     int offset = record.hdr.prev_record_offset;
    //     frame_->load(offset, record);
    // }
    //
    // R make_prev_record(R &record, page_off_t record_end) {
    //     R result;
    //     int offset = record.hdr.prev_record_offset;
    //     frame_->load_at(record_end + offset, result);
    //     return result;
    // }
    //
    // // make @record point to its next record.
    // void next_record(R &record) {
    //     int offset = record.hdr.next_record_offset;
    //     frame_->load(offset, record);
    // }
    //
    // R make_next_record(R &record, page_off_t record_end) {
    //     R result;
    //     int offset = record.hdr.next_record_offset;
    //     frame_->load_at(record_end + offset, result);
    //     return result;
    // }

    NodeCursor first_cursor() {
        R first;
        // FIXME: change next_record_offset to diff[record.start,
        // next_record.start]
        frame_->load_at(0, first);

        return {frame_->gpos(), first};
    }

    // return supremum if the node is empty.
    NodeCursor first_user_cursor() {
        auto infi = first_cursor();
        return next_cursor(infi);
    }

    // return infimum if the node is empty.
    NodeCursor last_user_cursor() {
        auto supre = last_cursor();
        return prev_cursor(supre);
    }

    NodeCursor last_cursor() {
        R last;
        // FIXME: change next_record_offset to diff[record.start,
        // next_record.start]
        frame_->load_at(0, last);
        frame_->load(last);

        return {frame_->gpos(), last};
    }

    NodeCursor next_cursor(NodeCursor &cur) {
        R next;
        // FIXME: change next_record_offset to diff[record.start,
        // next_record.start]
        frame_->load_at(cur.offset + cur.record.hdr.next_record_offset, next);

        return {frame_->gpos(), next};
    }

    NodeCursor prev_cursor(NodeCursor &cur) {
        R prev;
        // FIXME: change next_record_offset to diff[record.start,
        // next_record.start]
        frame_->load_at(cur.offset + cur.record.hdr.prev_record_offset, prev);

        return {frame_->gpos(), prev};
    }

    // prev/next_record_offset from @l to @r
    int offset(NodeCursor &l, NodeCursor &r) {
        return r.offset - r.record.hdr.length - l.offset;
    }

    void dump(NodeCursor &cursor) {
        frame_->dump_at(cursor.offset - cursor.record.hdr.length,
                        cursor.record);
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
};

class LeafIndexNode : public IndexNode<LeafIndexNode, LeafClusteredRecord> {
public:
    LeafIndexNode(Frame *frame, Comparator &comp) : IndexNode(frame, comp) {}
    virtual ~LeafIndexNode() = default;
};

} // namespace storage

#endif // !STORAGE_INCLUDE_INDEX_NODE_H
