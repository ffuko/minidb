#include "index.h"
#include "record.h"
#include "gtest/gtest.h"
#include <cmath>
#include <iostream>
#include <memory>

namespace storage {

// search the leaf node where the key is stored in, return nullptr if not found.
IndexNode *Index::search_leaf(Key key) {
    IndexNode *node = this->root_;
    Record *record;

    // inter level search downwards the tree
    for (int i = 1;
         i <
         this->depth_ /* && node != nullptr */ /* when dynamic_cast fails */;
         i++) {
#ifdef DEBUG
        assert(!node->is_leaf());
#endif // !DEBUG
        if (node->is_empty() || node->first_record()->is_supremum())
            // TODO: return node not found error?
            return nullptr;

        record = node->first_record();
        // inner index node search, from infi record to supre record
        while (record->next_record()->type() != RecordMeta::Type::Supre &&
               key >= record->next_record()->key()) {
            record = record->next_record();
        }

        // FIXME: dangeous cast; if it is ensured to be a internel node
        // record, use static_cast.
        node = dynamic_cast<NodeRecord *>(record)->child_node();
    }
    return node;
}

// search the record of the corresponding key, return nullptr if not found.
Record *Index::search_record(Key key) {
    IndexNode *node = search_leaf(key);

    if (node == nullptr)
        return nullptr;

    Record *record = node->first_record();

    // inner index node search
    while (!record->is_supremum()) {
        if (key > record->key()) {
            record = record->next_record();
        } else if (key == record->key()) {
            return record;
        } else {
            return nullptr;
        }
    }

    // TODO: return key-not-found error?
    return nullptr;
}

// TODO: use one-way-down insert instead by ensuring the characteristics of the
// tree when performing searching.
Index::OpStatus Index::insert_record(Record *insert_record) {
    IndexNode *node = search_leaf(insert_record->key());

#ifdef DEBUG
    assert(node->is_leaf() && node != nullptr);
#endif // DEBUG

    if (node == nullptr)
        return OpStatus::Failure;

    // case 1: the node is not full, insert directly.
    if (!node->is_full()) {
        return insert_not_full(node, insert_record);
    }
    // case 2: the node is full: split the node, create a new parent node,
    // repeat until all nodes are not full.
    rebalance(node);
    // now the parent node is not full and not null, split directly.
    safe_node_split(node, node->parent_node());
    insert_not_full(node, insert_record);

    return Index::OpStatus::Success;
}

// insert a record into a not full node.
Index::OpStatus Index::insert_not_full(IndexNode *node, Record *insert_record) {
    auto status = node->insert_record(insert_record);
    if (status == IndexNode::OpStatus::Failure)
        return Index::OpStatus::Failure;
    number_of_records_++;
    return Index::OpStatus::Success;
}

// try to rebalance the tree from a full leaf node.
void Index::rebalance(IndexNode *node) {
    IndexNode *parent_node = node->parent_node();
    if (parent_node != nullptr && parent_node->is_full()) {
        // recursively rebalance a internal node.
        rebalance_internal(node);
    } else if (parent_node == nullptr) {
        // rebalancing from a full root_ leaf node, create a new root.
        IndexNode *newRootNode = new IndexNode(id_, this->depth_, false);
        this->node_meta_ =
            std::make_unique<RecordMeta>(RecordMeta::Type::Node, 0);
        NodeRecord *new_parent_record =
            new NodeRecord(node_meta(), node->first_record()->key());
        newRootNode->insert_record(new_parent_record);
        new_parent_record->set_child_node(node);
        node->set_parent_node(newRootNode);
        node->set_parent_record(new_parent_record);

        this->root_ = newRootNode;
        this->depth_++;
    }
    // the parent node is not full, return
}

// rebalance a internal node recursively.
void Index::rebalance_internal(IndexNode *node) {
    IndexNode *parent_node;
    if (node != nullptr && node->is_full()) {
        parent_node = node->parent_node();
        if (parent_node != nullptr && parent_node->is_full()) {
            rebalance_internal(parent_node);
        } else if (parent_node == nullptr) {
            // rebalancing a full root non-leaf node.
            IndexNode *newRootNode = new IndexNode(id_, this->depth_, false);
#ifdef DEBUG
            assert(node_meta() != nullptr);
#endif // DEBUG
            NodeRecord *new_parent_record =
                new NodeRecord(node_meta(), node->first_record()->key());
            newRootNode->insert_record(new_parent_record);
            new_parent_record->set_child_node(node);
            node->set_parent_node(newRootNode);
            node->set_parent_record(new_parent_record);

            this->root_ = newRootNode;
            this->depth_++;
        }
        safe_node_split(node, parent_node);
    }
}

// split a full node into [0, n1) and [n1, n1 + n2) and insert the new node into
// the *not-full* parent node.
void Index::safe_node_split(IndexNode *node, IndexNode *parent) {
    int n1 = std::ceil(node->max_number_of_keys() / 2);
    int n2 = std::floor(node->max_number_of_keys() / 2);

    IndexNode *new_node = new IndexNode(id_, node->level(), node->is_leaf());
    Record *record = node->first_record();
    int i = 0;
    while (i < n1 && !record->is_supremum()) {
        record = record->next_record();
    }

    i = 0;
    Key new_key = record->key();
    while (i < n2 && !record->is_supremum()) {
        new_node->insert_record(record);
        record = record->next_record();
    }

    Record *parent_record = node->parent_record();
#ifdef DEBUG
    assert(parent_record != nullptr);
#endif // DEBUG

    NodeRecord *new_parent_record = new NodeRecord(record->meta(), new_key);
    Record *after_new_parent = parent_record->next_record();

    parent_record->set_next_record(new_parent_record);
    new_parent_record->set_next_record(after_new_parent);
    new_parent_record->set_child_node(new_node);
}

Index::OpStatus Index::full_scan(RecordTraverseFunc func) {
    IndexNode *node = this->root_;
    Record *record;

    // inter level search downwards the tree
    for (int i = 1; i < this->depth_; i++) {
        record = node->first_record();
        if (record->is_leaf())
            return Index::OpStatus::Failure;
        // FIXME: dangeous cast
        node = dynamic_cast<NodeRecord *>(record)->child_node();
    }

    // now record is the first leaf node of the whole tree.
    while (node != nullptr) {
        record = node->first_record();
        while (!record->is_supremum()) {
            func(record->key(), record);
            record = record->next_record();
        }

        node = node->next_node();
    }

    return Index::OpStatus::Success;
}

// TODO: for debug and memory cleanup
Index::OpStatus Index::full_node_scan(NodeTraverseFunc func) {
    return Index::OpStatus::Success;
}

Index::OpStatus Index::remove_record(Key key) {
    IndexNode *node = search_leaf(key);

    Record *record;
    if (node->is_half_full()) {
        // case 1: ok to union the node and one of its neighbor
        bool u = sibling_union_check(node);

        // case 2: reconstruct the node by borrowing one record from one of its
        // neighbors.
        if (!u) {
            IndexNode *left_node = node->prev_node(),
                      *right_node = node->next_node();
            if (right_node->number_of_records() >
                // borrow from right sibling node.
                IndexNode::min_number_of_keys()) {
                Record *to_move = right_node->first_record();
                right_node->infimum()->set_next_record(to_move->next_record());

                Record *last_record = node->last_record();
                last_record->set_next_record(to_move);
                to_move->set_next_record(node->supremum());

                Record *right_parent = right_node->parent_record();
                right_parent->set_key(right_node->first_record()->key());
            } else if (left_node->number_of_records() >
                       IndexNode::min_number_of_keys()) {
                // borrow fromt he left sibling node.
                Record *prev_to_last_record = left_node->first_record();
                while (!prev_to_last_record->next_record()
                            ->next_record()
                            ->is_supremum())
                    prev_to_last_record = prev_to_last_record->next_record();

                Record *to_move = prev_to_last_record->next_record();
                Record *first_record = node->first_record();

                prev_to_last_record->set_next_record(to_move->next_record());

                node->infimum()->set_next_record(to_move);
                to_move->set_next_record(first_record);
            } else {
                return Index::OpStatus::Failure;
            }
        }
    }

    // a normal delete
    if (!node->is_half_full()) {
        auto status = node->remove_record(key);
        if (status == IndexNode::OpStatus::Failure) {
            return OpStatus::Failure;
        } else {
            number_of_records_--;
            return OpStatus::Success;
        }
    }

    return OpStatus::Failure;
}

// if necessary to do union with a sibling, do it and return the node after
// union; if not necessary/unable to do it, return nullptr.
IndexNode *Index::sibling_union_check(IndexNode *node) {
    if (!node->is_half_full()) {
        return nullptr;
    }

    IndexNode *left_node = node->prev_node(), *right_node = node->next_node();
    if (left_node != nullptr &&
        left_node->number_of_records() + node->number_of_records() <=
            IndexNode::max_number_of_keys()) {
        node = union_node(left_node, node);
    } else if (right_node != nullptr &&
               right_node->number_of_records() + node->number_of_records() <=
                   IndexNode::max_number_of_keys()) {
        node = union_node(node, right_node);
    } else {
        return nullptr;
    }

    return node;
}

// union the left node and the right node and do sibling union check on the
// parent node.
IndexNode *Index::union_node(IndexNode *left_node, IndexNode *right_node) {
    Record *left_parent = left_node->parent_record();
    Record *right_parent = right_node->parent_record();
    IndexNode *right_parent_node = right_node->parent_node();

    Record *record = right_node->first_record();
    while (!record->is_supremum()) {
        left_node->insert_record(record);
        record = record->next_record();
    }
    delete right_node;

    if (left_parent->next_record()->is_supremum()) {
        // case 1: if both parents not in the same internal node
        IndexNode *right_parent_node = right_node->parent_node();

        right_parent_node->infimum()->set_next_record(
            right_parent->next_record());

        delete right_parent;
    } else {
        // case 2: both parents in the same internal node;
        left_parent->set_next_record(right_parent->next_record());

        delete right_parent;
    }

    sibling_union_check(right_parent_node);
    return left_node;
}

} // namespace storage
