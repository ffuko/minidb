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
    auto result = node.search_key(key);
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

        auto child = pool_->get_frame(cursor.record.child);
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

    rebalance_from_leaf(leaf.value());

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

ErrorCode Index::remove_record(const Key &key) { return ErrorCode::Success; }

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

ErrorCode Index::rebalance_from_leaf(Frame *frame) {
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
        auto offset = newRootNode.insert_record(frame->key(), frame->pgno());
        if (!offset)
            return offset.error();

        frame->set_parent(parent_frame->pgno(), offset.value());
    }
    safe_node_split(frame, parent_frame);

    return ErrorCode::Success;
}

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

ErrorCode Index::safe_node_split(Frame *frame, Frame *parent_frame) {
    int n1 = std::ceil(config::max_number_of_records() / 2);
    int n2 = std::floor(config::max_number_of_records() / 2);
    auto result =
        allocate_frame(frame->index(), frame->level(), frame->is_leaf());
    if (!result)
        return result.error();
    Frame *new_frame = result.value();

    // TODO: memcpy from frame[record:n1, n1 + n2) to new_frame

    auto parent_cursor = frame->parent_record();
    if (!parent_cursor)
        return parent_cursor.error();

    InternalIndexNode node(parent_frame, comp_);
    node.insert_record_after(parent_cursor.value().record, new_frame->key(),
                             new_frame->pgno());

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

} // namespace storage
  //
