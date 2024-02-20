/*
 * b+ tree implementation.
 * */
#pragma once

#include "index_node.h"
#include "record.h"
#include "types.h"
#include <functional>

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

    // NOTE: for debug
    enum class OpStatus { Success, Failure, TreeGrow, TreeShrink };

    static inline IndexID number_of_indexes = 0;

public:
    Index()
        : id_(number_of_indexes++), root_(new IndexNode(id_, 0, true)),
          depth_(1), number_of_records_(0) {}
    ~Index() = default;

    Record *search_record(Key key);
    // insert a clusterd leaf record.
    OpStatus insert_record(Record *record);
    // remove a clusterd leaf record.
    OpStatus remove_record(Key key);

    Index::OpStatus full_node_scan(NodeTraverseFunc func);
    OpStatus full_scan(RecordTraverseFunc func);

    int depth() const { return depth_; }

    IndexID id() const { return id_; }

    RecordMeta *node_meta() const { return node_meta_.get(); }

    int number_of_records() const { return number_of_records_; }

private:
    IndexNode *search_leaf(Key key);
    void rebalance(IndexNode *node);
    void rebalance_internal(IndexNode *node);

    OpStatus insert_not_full(IndexNode *node, Record *insert_record);
    void safe_node_split(IndexNode *node, IndexNode *parent);

    IndexNode *union_node(IndexNode *left_node, IndexNode *right_node);
    IndexNode *sibling_union_check(IndexNode *node);

private:
    IndexID id_;

    IndexNode *root_;
    // the depth of the B+ tree, starting from 1 even if the tree is empty.
    // if a index page's level == the tree's depth, it is a leaf node.
    int depth_;
    int number_of_records_;

    std::unique_ptr<RecordMeta> node_meta_;
};

} // namespace storage
