#include "index/index.h"
#include "config.h"
#include "error.h"
#include "index/index_node.h"
#include "index/record.h"
#include "tl/expected.hpp"
#include <cmath>
#include <format>

namespace storage {

// get the cursor of the @key (whose key is <= @key).
tl::expected<Cursor<LeafClusteredRecord>, ErrorCode>
Index::get_cursor(const Key &key) {
    Log::GlobalLog() << "going to get cursor on key " << key << std::endl;
    auto leaf = search_leaf(key);
    if (!leaf)
        return tl::unexpected(leaf.error());
    auto frame = leaf.value();

    LeafIndexNode node(frame, comp_);
    auto result = node.get_cursor(key);
    if (!result)
        return tl::unexpected(result.error());
    return result.value();
}

tl::expected<LeafClusteredRecord, ErrorCode>
Index::search_record(const Key &key) {
    auto leaf = search_leaf(key);
    if (!leaf)
        return tl::unexpected(leaf.error());
    auto frame = leaf.value();

    LeafIndexNode node(frame, comp_);
    auto result = node.search_record(key);
    if (!result) {
        Log::GlobalLog() << "don't find key " << key << std::endl;
        return tl::unexpected(result.error());
    }
    Log::GlobalLog() << "found record on key " << key << std::endl;
    return result.value();
}

tl::expected<Frame *, ErrorCode> Index::search_leaf(const Key &key) {
    Log::GlobalLog() << "going to search leaf on key " << key << std::endl;
    auto result = get_root_frame();
    if (!result)
        return tl::unexpected(result.error());
    auto frame = result.value();
    while (!frame->is_leaf()) {
        InternalIndexNode node(frame, comp_);
        auto result = node.get_cursor(key);
        if (!result)
            return tl::unexpected(result.error());
        auto cursor = result.value();

        auto child = pool_->get_frame(cursor.record.value);
        if (!child)
            return tl::unexpected(result.error());
        frame = child.value();
    }
    return frame;
}

ErrorCode Index::insert_record(const Key &key, const Column &value) {
    // FIXME: key and column type check
    auto leaf = search_leaf(key);
    if (!leaf)
        return leaf.error();

    balance_for_insert(leaf.value());

    LeafIndexNode node(leaf.value(), comp_);
    auto result = node.insert_record(key, value);
    if (!result) {
        Log::GlobalLog() << "failed to insert record " << key << ": " << value
                         << " of " << result.error() << std::endl;
        return result.error();
    }

    Log::GlobalLog() << "inserted record " << key << ": " << value << std::endl;

    return ErrorCode::Success;
}

ErrorCode Index::remove_record(const Key &key) {
    auto leaf = search_leaf(key);
    if (!leaf)
        return ErrorCode::KeyNotFound;

    auto frame = leaf.value();

    balance_for_delete<LeafIndexNode>(frame);
    LeafIndexNode node(frame, comp_);

    auto ec = node.remove_record(key);
    Log::GlobalLog() << "removed record of " << key << std::endl;
    if (!ec)
        return ec.error();
    return ErrorCode::Success;
}

tl::expected<Frame *, ErrorCode> Index::new_nonleaf_root(Frame *child) {
    // auto result = allocate_frame(meta_.id, meta_.depth, false);
    // if (!result)
    //     return tl::unexpected(result.error());
    // Frame *root_frame = result.value();
    //
    // InternalClusteredRecord first_record;
    // first_record.child = child;
    //
    // root_frame->load_at(config::INDEX_PAGE_FIRST_RECORD_OFFSET,
    // first_record);
    //
    // // FIXME: store the placeholder record in the header so that no need to
    // load
    // // and dump
    // first_record.child = child->pgno();
    // root_frame->dump_at(config::INDEX_PAGE_FIRST_RECORD_OFFSET,
    // first_record);
    //
    // child->set_parent(root_frame->pgno(),
    //                   config::INDEX_PAGE_FIRST_RECORD_OFFSET);
    //
    // meta_.root_page = root_frame->pgno();
    // meta_.depth++;
    // return root_frame;
    return nullptr;
}

template <typename N>
ErrorCode Index::balance_for_delete(Frame *frame) {
    if (frame->is_half_full()) {
        // case 1: ok to union the frame and one of its neighbor
        bool u = sibling_union_check(frame);

        // case 2: reconstruct the frame by borrowing one record from one of its
        // neighbors.
        if (!u) {
            auto result = frame->prev_frame();
            if (!result)
                return result.error();
            auto left_frame = result.value();
            result = frame->next_frame();
            if (!result)
                return result.error();
            auto right_frame = result.value();

            N node(frame, comp_);
            if (right_frame->number_of_records() >
                // borrow from right sibling frame.
                config::min_number_of_records()) {

                N right_node(right_frame, comp_);
                auto borrowed = right_node.pop_front();

                if (!borrowed)
                    return borrowed.error();
                node.push_back(borrowed.value().key, borrowed.value().value);

            } else if (left_frame->number_of_records() >
                       config::min_number_of_records()) {
                // borrow fromt the left sibling frame.
                N left_node(left_frame, comp_);
                auto borrowed = left_node.pop_back();
                if (!borrowed)
                    return borrowed.error();
                auto &to_move = borrowed.value();
                node.push_front(to_move.key, to_move.value);
            } else {
                return ErrorCode::Failure;
            }
        }
    }
    return ErrorCode::Success;
}

// if necessary to do a union, do it and return true; else, return false.
bool Index::sibling_union_check(Frame *frame) {
    if (!frame->is_half_full()) {
        return false;
    }

    auto result = frame->prev_frame();
    if (!result)
        return false;
    auto left_frame = result.value();
    result = frame->next_frame();
    if (!result)
        return false;
    auto right_frame = result.value();

    LeafIndexNode node(frame, comp_);

    if (left_frame != nullptr &&
        left_frame->number_of_records() + frame->number_of_records() <=
            config::max_number_of_records()) {
        union_frame(left_frame, frame);
    } else if (right_frame != nullptr &&
               right_frame->number_of_records() + frame->number_of_records() <=
                   config::max_number_of_records()) {
        union_frame(frame, right_frame);
    } else {
        return false;
    }

    return true;
}

void Index::union_frame(Frame *left_frame, Frame *right_frame) {

    auto left_parent_cursor = left_frame->parent_record();
    auto right_parent_cursor = right_frame->parent_record();
    Frame *left_parent, *right_parent;

    if (!left_parent_cursor) {
        return;
    } else if (!right_parent_cursor)
        return;
    auto result = pool_->get_frame(left_parent_cursor.value().page);
    if (!result)
        return;
    right_parent = result.value();

    if (left_frame->is_leaf()) {
        LeafIndexNode left_node(left_frame, comp_);
        LeafIndexNode right_node(right_frame, comp_);

        left_node.node_union(right_node);
    } else {
        InternalIndexNode left_node(left_frame, comp_);
        InternalIndexNode right_node(right_frame, comp_);

        left_node.node_union(right_node);
    }

    left_frame->page()->hdr.next_page = right_frame->page()->hdr.next_page;
    auto after_right = pool_->get_frame(right_frame->page()->hdr.next_page);
    if (after_right) {
        auto after_right_frame = after_right.value();
        after_right_frame->page()->hdr.prev_page = left_frame->pgno();
        after_right_frame->mark_dirty();
    }

    pool_->remove_frame(right_frame);
    balance_for_delete<InternalIndexNode>(right_parent);

    InternalIndexNode right_parent_node(right_parent, comp_);
    IndexNode<InternalIndexNode, InternalClusteredRecord>::NodeCursor cursor{
        right_parent_cursor.value().offset, right_parent_cursor.value().record};
    right_parent_node.remove_record(cursor);
}

ErrorCode Index::balance_for_insert(Frame *frame) {
    if (!frame->is_full())
        return ErrorCode::Success;

    auto result = frame->parent_frame();
    if (!result)
        return result.error();

    Frame *parent_frame = result.value();
    if (parent_frame != nullptr && parent_frame->is_full()) {
        // recursively rebalance a internal index page.
        rebalance_internal(frame);
    } else if (parent_frame == nullptr) {
        // rebalancing from a full root leaf page, create a new root.
        auto result = allocate_frame(meta_.id, meta_.depth, false);
        if (!result)
            return result.error();
        Frame *parent_frame = result.value();

        InternalIndexNode newRootNode(parent_frame, comp_);
        LeafIndexNode node(frame, comp_);
        auto offset = newRootNode.insert_record(node.key(), frame->pgno());
        if (!offset)
            return offset.error();

        frame->set_parent(parent_frame->pgno(), offset.value());
        meta_.depth++;
    }
    safe_node_split(frame, parent_frame);

    return ErrorCode::Success;
}

// recursively rebalance a internal index page.
ErrorCode Index::rebalance_internal(Frame *frame) {
    Frame *parent_frame;
    if (frame != nullptr && frame->is_full()) {
        auto result = frame->parent_frame();
        if (!result)
            return result.error();
        parent_frame = result.value();

        if (parent_frame != nullptr && parent_frame->is_full())
            rebalance_internal(parent_frame);
        else if (parent_frame == nullptr) {
            // rebalancing from a full root leaf page, create a new root.
            auto result = allocate_frame(meta_.id, meta_.depth, false);
            if (!result)
                return result.error();
            Frame *parent_frame = result.value();

            InternalIndexNode newRootNode(parent_frame, comp_);
            auto offset =
                newRootNode.insert_record(frame->key(), frame->pgno());
            if (!offset)
                return offset.error();

            frame->set_parent(parent_frame->pgno(), offset.value());
        }
        safe_node_split(frame, parent_frame);
    }
    return ErrorCode::Success;
}

// the parent frame of @frame is ensured to have enough space to make child
// split.
ErrorCode Index::safe_node_split(Frame *frame, Frame *parent_frame) {
    int n1 = std::ceil(config::max_number_of_records() / 2);
    int n2 = std::floor(config::max_number_of_records() / 2);
    auto result =
        allocate_frame(frame->index(), frame->level(), frame->is_leaf());
    if (!result)
        return result.error();
    Frame *new_frame = result.value();
    new_frame->page()->hdr.next_page = frame->page()->hdr.next_page;
    new_frame->page()->hdr.prev_page = frame->pgno();
    frame->page()->hdr.next_page = new_frame->pgno();

    // TODO: memcpy from frame[record:n1, n1 + n2) to new_frame
    if (frame->is_leaf()) {
        LeafIndexNode left(frame, comp_);
        LeafIndexNode right(new_frame, comp_);

        auto ec = left.node_split(right, n1, n2);
        if (ec != ErrorCode::Success)
            return ec;
    } else {
        InternalIndexNode left(frame, comp_);
        InternalIndexNode right(new_frame, comp_);

        auto ec = left.node_split(right, n1, n2);
        if (ec != ErrorCode::Success)
            return ec;
    }

    auto parent_cursor = frame->parent_record();
    if (!parent_cursor)
        return parent_cursor.error();

    InternalIndexNode node(parent_frame, comp_);
    IndexNode<InternalIndexNode, InternalClusteredRecord>::NodeCursor cursor{
        parent_cursor.value().offset, parent_cursor.value().record};
    node.insert_record_after(cursor, new_frame->key(), new_frame->pgno());

    return ErrorCode::Success;
}

tl::expected<Frame *, ErrorCode>
Index::allocate_frame(index_id_t index, uint8_t level, bool is_leaf) {
    Log::GlobalLog()
        << std::format("[index]: allocate new frame for index {} at level {}",
                       index, level)
        << std::endl;

    return pool_->allocate_frame()
        .and_then([&](Frame *frame) -> tl::expected<Frame *, ErrorCode> {
            auto page = frame->page();
            page->hdr.index = index;
            page->hdr.level = level;
            page->hdr.is_leaf = is_leaf;

            // placeholder record.
            if (is_leaf) {
                frame->init_list<LeafClusteredRecord>();
            } else {
                frame->init_list<InternalClusteredRecord>();
            }
            frame->mark_dirty();
            return frame;
        })
        .or_else([](ErrorCode ec) -> tl::expected<Frame *, ErrorCode> {
            return tl::unexpected(ec);
        });
}

void Index::traverse(const RecordTraverseFunc &func) {
    auto result = get_root_frame();
    if (!result)
        return;
    auto frame = result.value();
    while (!frame->is_leaf()) {
        InternalIndexNode node(frame, comp_);
        auto first_record = node.first_user_cursor();

        auto child = pool_->get_frame(first_record.record.value);
        if (!child)
            return;
        frame = child.value();
    }

    LeafIndexNode node(frame, comp_);
    node.traverse(func);
}

// FIXME: current impl only reverse traversion inside a node, not the whole
// tree.
void Index::traverse_r(const RecordTraverseFunc &func) {
    auto result = get_root_frame();
    if (!result)
        return;
    auto frame = result.value();
    while (!frame->is_leaf()) {
        InternalIndexNode node(frame, comp_);
        auto first_record = node.first_user_cursor();

        auto child = pool_->get_frame(first_record.record.value);
        if (!child)
            return;
        frame = child.value();
    }

    LeafIndexNode node(frame, comp_);
    node.traverse_r(func);
}
} // namespace storage
  //
