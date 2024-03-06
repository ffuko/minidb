#ifndef STORAGE_INCLUDE_INDEX_NODE_H
#define STORAGE_INCLUDE_INDEX_NODE_H

#include "buffer/buffer_pool.h"
#include "buffer/frame.h"
#include "config.h"
#include "error.h"
#include "index/cursor.h"
#include "log.h"
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

    using TraverseFunc = std::function<void(LeafClusteredRecord &record)>;

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
    // record, or the first user record.
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
        // Log::GlobalLog() << i << std::endl;

        // return the first user record.
        if (i == 0)
            return Cursor<R>{frame_->pgno(), cursor.offset, cursor.record};

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
    tl::expected<NodeCursor, ErrorCode> insert_record(const Key &key,
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
        // Log::GlobalLog() << "the number of records after insert: "
        //  << frame_->number_of_records() << std::endl;
        return result;
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
                // Log::GlobalLog() << "the number of records after remove: "
                //<< frame_->number_of_records() << std::endl;

                return result;
            } else if (record.key > key)
                break;

            cursor = next_cursor(cursor);
            i++;
        }

        return tl::unexpected(ErrorCode::KeyNotFound);
    }

    template <typename V>
    NodeCursor push_back(const Key &key, const V &value) {
        auto supre = last_cursor();
        auto result = insert_record_before(supre, key, value);
        return result;
    }

    template <typename V>
    NodeCursor push_front(const Key &key, const V &value) {
        auto first = first_user_cursor();

        auto result = insert_record_before(first, key, value);
        return result;
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
        right_record.hdr.prev_record_offset = offset(right, left);
        // FIXME: lazy delete;
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

        NodeCursor inserted{0, R{}};
        R &record = inserted.record;
        record.hdr = {
            // .order = left_record.hdr.order + 1,
        };
        record.key = key;
        record_value_assign(record, value);

        record.hdr.length = frame_->dump_at(frame_->last_inserted(), record);

        inserted.offset = frame_->ppos();
        // Log::GlobalLog() << "the end of the last dump: " << frame_->ppos()
        // << std::endl;
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

        // Log::GlobalLog() << "the end of the inserted record: " <<
        // frame_->ppos()
        //                  << std::endl;
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

    NodeCursor parent_cursor() {
        // FIXME;
    }

    ErrorCode node_split(N &node, int n1, int n2, BufferPoolManager *pool) {
        // page header update
        // Log::GlobalLog() << std::format("[{}; {}]\n", n1, n2);
        if (!frame_->is_full())
            return ErrorCode::NodeNotFull;

        auto frame = node.frame_;

        int i = 0;
        while (i < n2) {
            auto result = pop_back();
            if (!result)
                return result.error();
            auto &record = result.value();
            record.hdr.status = 0;

            auto cursor = node.push_front(record.key, record.value);
            if (!frame_->is_leaf()) {
                update_record_parent(node, cursor.record, cursor.offset, pool);
            }

            //  DEBUG
            i++;
        }

        // Log::GlobalLog() << "[IndexNode] left node has "
        //                  << this->number_of_records() << "; right node has "
        //                  << node.number_of_records() << std::endl;
        // record copy
        return ErrorCode::Success;
    }

    ErrorCode node_move(N &node, BufferPoolManager *pool) {
        auto frame = node.frame_;

        auto cursor = first_user_cursor();
        int i = 0;
        while (i < number_of_records()) {
            auto &record = cursor.record;

            auto inserted = node.push_back(record.key, record.value);

            if (!frame_->is_leaf()) {
                update_record_parent(node, inserted.record, inserted.offset,
                                     pool);
            }
            cursor = next_cursor(cursor);
            i++;
        }

        return ErrorCode::Success;
    }
    NodeCursor remove_record(NodeCursor &cursor) {
        NodeCursor left_cursor = prev_cursor(cursor);
        NodeCursor right_cursor = next_cursor(cursor);

        // Log::GlobalLog() << "the end offset of the to-be-deleted record: "
        // << cursor.offset << std::endl;
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

#ifdef DEBUG
    void print() {
        Log::GlobalLog() << std::format("printing page {}: ", frame_->pgno());
        if (number_of_records() == 0)
            return;
        auto cursor = first_user_cursor();
        int i = 0;
        while (i < number_of_records()) {
            Log::GlobalLog()
                << cursor.record.key << ": " << cursor.record.value << ", ";
            // Log::GlobalLog() << cursor.record.key << ", ";

            cursor = next_cursor(cursor);
            i++;
        }
        Log::GlobalLog() << std::endl;
    }
#endif // DEBUG

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
    virtual ErrorCode update_record_parent(N &node, R &record,
                                           page_off_t offset,
                                           BufferPoolManager *pool) = 0;

protected:
    Frame *frame_;
    // *placeholder* now, reserved to add comparison support for secondary
    // record.
    Comparator &comp_;

    // TODO: page directory

    // IndexNode *parent_node_;
};

class LeafIndexNode : public IndexNode<LeafIndexNode, LeafClusteredRecord> {
public:
    LeafIndexNode(Frame *frame, Comparator &comp) : IndexNode(frame, comp) {}
    virtual ~LeafIndexNode() = default;

    ErrorCode update_record_parent(LeafIndexNode &new_parent,
                                   LeafClusteredRecord &record,
                                   page_off_t offset,
                                   BufferPoolManager *pool) override {
        return ErrorCode::Success;
    }

    void traverse(const TraverseFunc &func) {

        auto cursor = first_user_cursor();

        int i = 0;
        while (i < frame_->number_of_records()) {
            func(cursor.record);
            cursor = next_cursor(cursor);
            i++;
        }
    }
};

class InternalIndexNode
    : public IndexNode<InternalIndexNode, InternalClusteredRecord> {
public:
    InternalIndexNode(Frame *frame, Comparator &comp)
        : IndexNode(frame, comp) {}
    virtual ~InternalIndexNode() = default;

    ErrorCode update_record_parent(InternalIndexNode &new_parent,
                                   InternalClusteredRecord &record,
                                   page_off_t offset,
                                   BufferPoolManager *pool) override {
        auto result = pool->get_frame(record.value);
        if (!result)
            return result.error();

        result.value()->set_parent(new_parent.frame_->pgno(),
                                   offset - record.len());

        return ErrorCode::Success;
    }

    void traverse(const TraverseFunc &func, BufferPoolManager *pool) {
        // Log::GlobalLog() << "---------------------------------------------"
        //                  << std::endl;
        // Log::GlobalLog() << "at level " << int(frame_->level()) << std::endl;
        // print();
        // Log::GlobalLog() << "---------------------------------------------"
        // << std::endl;

        auto cursor = first_user_cursor();

        int i = 0;
        while (i < number_of_records()) {
            auto child = pool->get_frame(cursor.record.value);
            if (!child)
                return;

            if (child.value()->is_leaf()) {
                LeafIndexNode node(child.value(), comp_);
                node.traverse(func);
            } else {
                InternalIndexNode node(child.value(), comp_);
                node.traverse(func, pool);
            }
            cursor = next_cursor(cursor);
            i++;
        }
    }
};
} // namespace storage

#endif // !STORAGE_INCLUDE_INDEX_NODE_H
