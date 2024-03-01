/*
 * b+ tree implementation.
 * */
#ifndef STORAGE_INCLUDE_INDEX_H
#define STORAGE_INCLUDE_INDEX_H

#include "error.h"
#include "index_node.h"
#include "record.h"
#include "types.h"
#include <functional>
#include <tl/expected.hpp>

namespace storage {

// Index is the logical structure of a clustered index in the database which is
// a B+ tree in fact.
// a Index is built on leaf nodes and non-leaf nodes, both of
// which are instances of class IndexNode.
// TODO: bulk-loading
// FIXME: only allow auto-increment keys.
class Index {
public:
#ifdef DEBUG
#endif // DEBUG
    using RecordTraverseFunc = std::function<void(Key, Record *)>;
    using NodeTraverseFunc = std::function<void(IndexNode *)>;

    static inline index_id_t number_of_indexes = 0;

public:
    Index()
        : id_(number_of_indexes++), root_(new IndexNode(id_, 0, true)),
          depth_(1), number_of_records_(0) {}
    ~Index() = default;

    tl::expected<Record *, ErrorCode> search_record(const Key &key);
    // insert a clusterd leaf record.
    ErrorCode insert_record(Record *record);
    // remove a clusterd leaf record.
    ErrorCode remove_record(const Key &key);

    ErrorCode full_node_scan(NodeTraverseFunc func);
    ErrorCode full_scan(RecordTraverseFunc func);

    int depth() const { return depth_; }

    index_id_t id() const { return id_; }
    RecordMeta *node_meta() const { return node_meta_.get(); }

    int number_of_records() const { return number_of_records_; }

private:
    tl::expected<IndexNode *, ErrorCode> search_leaf(const Key &key);
    void rebalance(IndexNode *node);
    void rebalance_internal(IndexNode *node);

    ErrorCode insert_not_full(IndexNode *node, Record *insert_record);
    void safe_node_split(IndexNode *node, IndexNode *parent);

    IndexNode *union_node(IndexNode *left_node, IndexNode *right_node);
    IndexNode *sibling_union_check(IndexNode *node);

private:
    index_id_t id_;

    IndexNode *root_;
    // the depth of the B+ tree, starting from 1 even if the tree is empty.
    // if a index page's level == the tree's depth, it is a leaf node.
    int depth_;
    int number_of_records_;

    std::unique_ptr<RecordMeta> node_meta_;
};

} // namespace storage
#endif
