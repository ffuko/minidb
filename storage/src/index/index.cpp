#include "index/index.h"
#include "cereal/details/helpers.hpp"
#include "config.h"
#include "error.h"
#include "index/index_node.h"
#include "index/record.h"
#include "log.h"
#include "tl/expected.hpp"
#include "types.h"
#include <cmath>
#include <cstring>
#include <format>

namespace storage {

// get the cursor of the @key (whose key is <= @key).
tl::expected<Cursor<LeafClusteredRecord>, ErrorCode>
Index::get_cursor(const Key &key) {
    // Log::GlobalLog() << "going to get cursor on key " << key << std::endl;
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
        LeafIndexNode node(frame, comp_);
        // Log::GlobalLog() << "failed to search " << key << std::endl;
        // node.print();
        // Log::GlobalLog() << "prev: " << node.frame_->page()->hdr.prev_page
        //                  << "; next " << node.frame_->page()->hdr.next_page
        //                  << std::endl;
        // auto parent = frame->parent_frame();
        // if (parent && parent.value()) {
        //     InternalIndexNode parent_node(parent.value(), comp_);
        //     parent_node.print();
        // }
        return tl::unexpected(result.error());
    }
    // Log::GlobalLog() << "found record on key " << key << std::endl;
    return result.value();
}

tl::expected<Frame *, ErrorCode> Index::search_leaf(const Key &key) {
    auto result = get_root_frame();
    if (!result)
        return tl::unexpected(result.error());
    auto frame = result.value();
    while (!frame->is_leaf()) {
        InternalIndexNode node(frame, comp_);
        // Log::GlobalLog() << std::format(
        //     "page {} has {} childs\n", frame->pgno(),
        //     node.number_of_records());
        // node.print();
        auto result = node.get_cursor(key);
        if (!result)
            return tl::unexpected(result.error());
        auto cursor = result.value();

        auto child = pool_->get_frame(cursor.record.value);
        if (!child) {
            Log::GlobalLog() << std::format("error when reading page {}\n",
                                            cursor.record.value);
            return tl::unexpected(child.error());
        }
        frame = child.value();
        // Log::GlobalLog() << std::format("{}", frame->pgno()) << " ";
    }
    // Log::GlobalLog() << std::endl;
    // Log::GlobalLog() << "found leaf " << frame->pgno() << " on key " << key
    //                  << std::endl;

    LeafIndexNode node(frame, comp_);
    // node.print();

    return frame;
}

ErrorCode Index::insert_record(const Key &key, const Column &value) {
    // FIXME: key and column type check
    auto leaf = search_leaf(key);
    if (!leaf)
        return leaf.error();

    Frame *frame = leaf.value();
    balance_for_insert(frame);

    leaf = search_leaf(key);
    if (!leaf)
        return leaf.error();

    frame = leaf.value();

    tl::expected<LeafIndexNode::NodeCursor, ErrorCode> result;
    try {
        LeafIndexNode node(frame, comp_);
        result = node.insert_record(key, value);

        assert(node.number_of_records() <= config::max_number_of_records());
    } catch (cereal::Exception &exception) {
        // page write overflow
        // Log::GlobalLog() << "[Index] page overflow: " << exception.what()
        // << std::endl;
        auto new_frame = move_frame(frame);
        if (!new_frame)
            return new_frame.error();

        LeafIndexNode new_node(new_frame.value(), comp_);
        LeafIndexNode node(frame, comp_);

        // Log::GlobalLog() << std::format("moved page from {} to {}\n",
        //                                 frame->pgno(),
        // new_frame.value()->pgno());
        pool_->remove_frame(frame);
        frame = new_frame.value();

        result = new_node.insert_record(key, value);

        assert(new_node.number_of_records() <= config::max_number_of_records());
    } catch (...) {
        return ErrorCode::UnknownException;
    }

    if (!result) {
        // Log::GlobalLog() << "failed to insert record " << key << ": " <<
        // value
        //                  << " of " << result.error() << std::endl;
        return result.error();
    }

    // Log::GlobalLog() << "inserted record " << key << ": " << value
    //                  << " before offset " << result.value().offset
    //                  << " in page " << frame->pgno() << std::endl;

    return ErrorCode::Success;
}

// move the page's record into a new page to make the page compact.
tl::expected<Frame *, ErrorCode> Index::move_frame(Frame *frame) {
    auto result =
        allocate_frame(frame->index(), frame->level(), frame->is_leaf());
    if (!result)
        return tl::unexpected(result.error());

    Frame *new_frame = result.value();
    // copy header
    new_frame->page()->hdr.prev_page = frame->page()->hdr.prev_page;
    new_frame->page()->hdr.next_page = frame->page()->hdr.next_page;
    new_frame->page()->hdr.parent_page = frame->page()->hdr.parent_page;
    new_frame->page()->hdr.parent_record_off =
        frame->page()->hdr.parent_record_off;

    // copy payload
    if (frame->is_leaf()) {
        LeafIndexNode node(frame, comp_);
        LeafIndexNode new_node(new_frame, comp_);
        node.node_move(new_node, pool_.get());
    } else {
        InternalIndexNode node(frame, comp_);
        InternalIndexNode new_node(new_frame, comp_);
        node.node_move(new_node, pool_.get());
    }
    // update parent record
    auto cursor = frame->parent_record();
    if (!cursor)
        return tl::unexpected(cursor.error());

    cursor.value().record.value = new_frame->pgno();
    auto parent_frame = pool_->get_frame(cursor.value().page);
    if (!parent_frame)
        return tl::unexpected(parent_frame.error());

    parent_frame.value()->dump_at(cursor.value().offset -
                                      cursor.value().record.len(),
                                  cursor.value().record);
    // InternalIndexNode parent_node(parent_frame.value(), comp_);

    // update page list
    auto prev_page = pool_->get_frame(frame->page()->hdr.prev_page);
    auto next_page = pool_->get_frame(frame->page()->hdr.next_page);

    if (prev_page && prev_page.value()) {
        prev_page.value()->page()->hdr.next_page = new_frame->pgno();
        prev_page.value()->mark_dirty();
    }
    if (next_page && next_page.value()) {
        next_page.value()->page()->hdr.prev_page = new_frame->pgno();
        next_page.value()->mark_dirty();
    }

    return new_frame;
}

// move the page's record into a new page to make the page compact.
tl::expected<Frame *, ErrorCode> Index::move_frame(Frame *frame,
                                                   size_t number) {
    auto result =
        allocate_frame(frame->index(), frame->level(), frame->is_leaf());
    if (!result)
        return tl::unexpected(result.error());

    Frame *new_frame = result.value();
    // copy header
    new_frame->page()->hdr.prev_page = frame->page()->hdr.prev_page;
    new_frame->page()->hdr.next_page = frame->page()->hdr.next_page;
    new_frame->page()->hdr.parent_page = frame->page()->hdr.parent_page;
    new_frame->page()->hdr.parent_record_off =
        frame->page()->hdr.parent_record_off;

    // copy payload
    if (frame->is_leaf()) {
        LeafIndexNode node(frame, comp_);
        LeafIndexNode new_node(new_frame, comp_);
        node.node_move(new_node, number, pool_.get());
    } else {
        InternalIndexNode node(frame, comp_);
        InternalIndexNode new_node(new_frame, comp_);
        node.node_move(new_node, number, pool_.get());
    }
    // update parent record
    auto cursor = frame->parent_record();
    if (!cursor)
        return tl::unexpected(cursor.error());

    cursor.value().record.value = new_frame->pgno();
    auto parent_frame = pool_->get_frame(cursor.value().page);
    if (!parent_frame)
        return tl::unexpected(parent_frame.error());

    parent_frame.value()->dump_at(cursor.value().offset -
                                      cursor.value().record.len(),
                                  cursor.value().record);
    // InternalIndexNode parent_node(parent_frame.value(), comp_);

    // update page list
    auto prev_page = pool_->get_frame(frame->page()->hdr.prev_page);
    auto next_page = pool_->get_frame(frame->page()->hdr.next_page);
    if (prev_page && prev_page.value()) {
        prev_page.value()->page()->hdr.next_page = new_frame->pgno();
        prev_page.value()->mark_dirty();
    }
    if (next_page && next_page.value()) {
        next_page.value()->page()->hdr.prev_page = new_frame->pgno();
        next_page.value()->mark_dirty();
    }

    return new_frame;
}

ErrorCode Index::remove_record(const Key &key) {
    auto leaf = search_leaf(key);
    if (!leaf)
        return ErrorCode::KeyNotFound;

    auto frame = leaf.value();

    auto error = balance_for_delete<LeafIndexNode>(frame);
    if (error != ErrorCode::Success) {
        Log::GlobalLog() << "failed to balance for delete, reason: " << error
                         << std::endl;
    }

    // FIXME: better solution to handle the result of search_leaf went invalid?
    leaf = search_leaf(key);
    if (!leaf)
        return ErrorCode::KeyNotFound;

    frame = leaf.value();
    LeafIndexNode node(frame, comp_);

    auto ec = node.remove_record(key);
    if (!ec) {
        LeafIndexNode node(frame, comp_);
        Log::GlobalLog() << "failed to search " << key << std::endl;
        node.print();
        auto parent = frame->parent_frame();
        if (parent && parent.value()) {
            InternalIndexNode parent_node(parent.value(), comp_);
            parent_node.print();
        }
        return ec.error();
    }
    Log::GlobalLog() << "removed record of " << key << std::endl;
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
    if (frame->is_half_full() && frame->pgno() != meta_.root_page) {
        Log::GlobalLog() << "[Index] balance for delete" << std::endl;
        // case 1: ok to union the frame and one of its neighbor
        bool u = sibling_union_check(frame);

        // case 2: reconstruct the frame by borrowing one record from one of its
        // neighbors.
        if (!u) {
#ifdef DEBUG
            Log::GlobalLog() << "choose to borrow" << std::endl;
#endif // DEBUG
            auto prev_result = frame->prev_frame();
            Frame *left_frame, *right_frame;
            auto next_result = frame->next_frame();

            N node(frame, comp_);

            if (prev_result) {
                left_frame = prev_result.value();
                if (left_frame->number_of_records() >
                    config::min_number_of_records()) {
                    // borrow fromt the left sibling frame.
                    N left_node(left_frame, comp_);
                    node.print();
                    auto borrowed = left_node.pop_back();
                    if (!borrowed)
                        return borrowed.error();
                    auto &to_move = borrowed.value();
                    auto inserted = node.push_front(to_move.key, to_move.value);
                    // FIXME: if the node is internal, update its child's parent
                    // record
                    if (!frame->is_leaf()) {
                        // FIXME: make type safe
                        auto &tmp = reinterpret_cast<InternalIndexNode &>(node);
                        tmp.update_record_parent(
                            tmp,
                            reinterpret_cast<InternalClusteredRecord &>(
                                inserted.record),
                            inserted.offset, pool_.get());
                    }
                    node.print();

                    InternalIndexNode parent(
                        node.frame_->parent_frame().value(), comp_);
                    parent.print();

                    left_node.print();
                }
            } else if (next_result) {
                right_frame = next_result.value();
                if (right_frame->number_of_records() >
                    // borrow from right sibling frame.
                    config::min_number_of_records()) {

                    N right_node(right_frame, comp_);
                    node.print();
                    auto borrowed = right_node.pop_front();

                    if (!borrowed)
                        return borrowed.error();
                    auto inserted =
                        node.push_back(borrowed.value().record.key,
                                       borrowed.value().record.value);
                    // FIXME: if the node is internal, update its child's parent
                    // record
                    if (!frame->is_leaf()) {
                        // FIXME: make type safe
                        auto &tmp = reinterpret_cast<InternalIndexNode &>(node);
                        tmp.update_record_parent(
                            tmp,
                            reinterpret_cast<InternalClusteredRecord &>(
                                inserted.record),
                            inserted.offset, pool_.get());
                    }

                    node.print();
                    right_node.print();
                }
            } else {
                return ErrorCode::Failure;
            }
        }
    } else if (frame->pgno() == meta_.root_page &&
               frame->number_of_records() == 2) {
        return ErrorCode::RootHeightDecrease;
    }
    return ErrorCode::Success;
}

// if necessary to do a union, do it and return true; else, return false.
bool Index::sibling_union_check(Frame *frame) {
    if (!frame->is_half_full()) {
        return false;
    }

    auto prev_result = frame->prev_frame();
    auto next_result = frame->next_frame();

    LeafIndexNode node(frame, comp_);

    if (prev_result && prev_result.value() != nullptr &&
        prev_result.value()->number_of_records() + frame->number_of_records() <=
            config::max_number_of_records()) {
        union_frame(prev_result.value(), frame);
    } else if (next_result && next_result.value() != nullptr &&
               next_result.value()->number_of_records() +
                       frame->number_of_records() <=
                   config::max_number_of_records()) {
        union_frame(frame, next_result.value());
    } else {
        return false;
    }

    return true;
}

void Index::union_frame(Frame *left_frame, Frame *right_frame) {
#ifdef DEBUG
    Log::GlobalLog() << "choose to union" << std::endl;
#endif
    auto left_parent_cursor = left_frame->parent_record();
    auto right_parent_cursor = right_frame->parent_record();
    Frame *left_parent, *right_parent;

    if (!left_parent_cursor) {
        return;
    } else if (!right_parent_cursor)
        return;
    auto result = pool_->get_frame(right_parent_cursor.value().page);
    if (!result)
        return;
    right_parent = result.value();

    size_t left_size_before;
try_union:
    try {
        if (left_frame->is_leaf()) {
            LeafIndexNode left_node(left_frame, comp_);
            LeafIndexNode right_node(right_frame, comp_);
            left_size_before = left_node.number_of_records();

            left_node.print();
            right_node.print();
            left_node.node_union(right_node, pool_.get());
            left_node.print();
        } else {
            InternalIndexNode left_node(left_frame, comp_);
            InternalIndexNode right_node(right_frame, comp_);
            left_size_before = left_node.number_of_records();

            left_node.print();
            right_node.print();
            left_node.node_union(right_node, pool_.get());
            left_node.print();
        }
    } catch (cereal::Exception &exception) {
        // page write overflow
        Log::GlobalLog() << "[Index] page overflow: " << exception.what()
                         << std::endl;
        auto new_frame = move_frame(left_frame, left_size_before);
        if (!new_frame)
            throw;

        LeafIndexNode new_node(new_frame.value(), comp_);
        // LeafIndexNode node(frame, comp_);

        // Log::GlobalLog() << std::format("moved page from {} to {}\n",
        //                                 frame->pgno(),
        // new_frame.value()->pgno());
        pool_->remove_frame(left_frame);
        left_frame = new_frame.value();

        assert(new_node.number_of_records() <= config::max_number_of_records());
        goto try_union;
    }

    left_frame->page()->hdr.next_page = right_frame->page()->hdr.next_page;
    auto after_right = pool_->get_frame(right_frame->page()->hdr.next_page);
    if (after_right && after_right.value()) {
        auto after_right_frame = after_right.value();
        after_right_frame->page()->hdr.prev_page = left_frame->pgno();
        after_right_frame->mark_dirty();
    }
    // pool_->remove_frame(right_frame);

    auto ec = balance_for_delete<InternalIndexNode>(right_parent);
    // btree reduce height
    if (ec == ErrorCode::RootHeightDecrease) {
        pool_->remove_frame(right_parent);
        meta_.root_page = left_frame->pgno();
        return;
    } else if (ec != ErrorCode::Success)
        return;

    right_parent_cursor = right_frame->parent_record();
    if (!right_parent_cursor)
        return;
    result = pool_->get_frame(right_parent_cursor.value().page);
    if (!result)
        return;
    right_parent = result.value();

    InternalIndexNode right_parent_node(right_parent, comp_);
    right_parent_node.print();
    IndexNode<InternalIndexNode, InternalClusteredRecord>::NodeCursor cursor{
        right_parent_cursor.value().offset, right_parent_cursor.value().record};
    right_parent_node.remove_record(cursor);
    pool_->remove_frame(right_frame);
    right_parent_node.print();
}

ErrorCode Index::balance_for_insert(Frame *frame) {
    if (!frame->is_full())
        return ErrorCode::Success;

    // Log::GlobalLog() << "[Index] balance for insert " << std::endl;
    auto result = frame->parent_frame();
    if (!result)
        return result.error();

    Frame *parent_frame = result.value();
    if (parent_frame != nullptr && parent_frame->is_full()) {
        // recursively rebalance a internal index page.
        balance_for_insert_internal(parent_frame);
    } else if (parent_frame == nullptr) {
        // rebalancing from a full root leaf page, create a new root.
        auto result = allocate_frame(meta_.id, meta_.depth, false);
        if (!result)
            return result.error();
        parent_frame = result.value();

        InternalIndexNode newRootNode(parent_frame, comp_);
        LeafIndexNode node(frame, comp_);
        auto cursor = newRootNode.insert_record(node.get_key(), frame->pgno());
        if (!cursor)
            return cursor.error();

        frame->set_parent(parent_frame->pgno(),
                          cursor.value().offset - cursor.value().record.len());

        meta_.root_page = parent_frame->pgno();
        meta_.depth++;

        // Log::GlobalLog() << std::format("[Index] new root page {}\n",
        // parent_frame->pgno());
    }
    // the parent may be updated during split, reget.
    result = frame->parent_frame();
    if (!result)
        return result.error();

    parent_frame = result.value();
    safe_node_split(frame, parent_frame);

    return ErrorCode::Success;
}

// recursively rebalance a internal index page.
ErrorCode Index::balance_for_insert_internal(Frame *frame) {
    if (!frame->is_full())
        return ErrorCode::Success;

    // Log::GlobalLog() << "[Index] balance for insert " << std::endl;
    auto result = frame->parent_frame();
    if (!result)
        return result.error();

    Frame *parent_frame = result.value();
    if (parent_frame != nullptr && parent_frame->is_full()) {
        // recursively rebalance a internal index page.
        balance_for_insert_internal(parent_frame);
    } else if (parent_frame == nullptr) {
        // rebalancing from a full root leaf page, create a new root.
        auto result = allocate_frame(meta_.id, meta_.depth, false);
        if (!result)
            return result.error();
        parent_frame = result.value();

        InternalIndexNode newRootNode(parent_frame, comp_);
        InternalIndexNode node(frame, comp_);
        auto cursor = newRootNode.insert_record(node.get_key(), frame->pgno());
        if (!cursor)
            return cursor.error();

        frame->set_parent(parent_frame->pgno(),
                          cursor.value().offset - cursor.value().record.len());

        meta_.root_page = parent_frame->pgno();
        meta_.depth++;

        // Log::GlobalLog() << std::format("[Index] new root page {}\n",
        //                                 parent_frame->pgno());
    }
    // the parent may be updated during split, reget.
    result = frame->parent_frame();
    if (!result)
        return result.error();

    parent_frame = result.value();
    safe_node_split(frame, parent_frame);

    return ErrorCode::Success;
}

// the parent frame of @frame is ensured to have enough space to make child
// split.
ErrorCode Index::safe_node_split(Frame *frame, Frame *parent_frame) {
    int n1 = std::ceil(config::max_number_of_records() / 2);
    int n2 = std::floor(config::max_number_of_records() / 2);
    //  new frame allocation
    auto result =
        allocate_frame(frame->index(), frame->level(), frame->is_leaf());
    if (!result)
        return result.error();
    Frame *new_frame = result.value();

    // update same-level node list
    new_frame->page()->hdr.next_page = frame->page()->hdr.next_page;
    new_frame->page()->hdr.prev_page = frame->pgno();
    auto after_new_frame = pool_->get_frame(frame->page()->hdr.next_page);
    if (after_new_frame && after_new_frame.value()) {
        after_new_frame.value()->page()->hdr.prev_page = new_frame->pgno();
        after_new_frame.value()->mark_dirty();
    }
    frame->page()->hdr.next_page = new_frame->pgno();

    // memcpy from frame[record:n1, n1 + n2) to new_frame
    Key new_key, old_key;
    if (frame->is_leaf()) {
        LeafIndexNode left(frame, comp_);
        LeafIndexNode right(new_frame, comp_);

        auto ec = left.node_split(right, n1, n2, pool_.get());
        if (ec != ErrorCode::Success)
            return ec;
        old_key = left.get_key();
        new_key = right.get_key();

    } else {
        InternalIndexNode left(frame, comp_);
        InternalIndexNode right(new_frame, comp_);

        auto ec = left.node_split(right, n1, n2, pool_.get());
        if (ec != ErrorCode::Success)
            return ec;
        old_key = left.get_key();
        new_key = right.get_key();
    }

    // update the parent cursor.
    // NOTE: parent cursor's offset is the end of the parent record, while
    // page.hdr.parent_record_off is the start of the parent record.
    auto parent_cursor = frame->parent_record();
    if (!parent_cursor)
        return parent_cursor.error();

    parent_cursor.value().record.key = old_key;
    InternalIndexNode parent_node(parent_frame, comp_);
    IndexNode<InternalIndexNode, InternalClusteredRecord>::NodeCursor cursor{
        parent_cursor.value().offset, parent_cursor.value().record};

    // maintain the first user record's split.
    // FIXME: better solution?
    parent_frame->dump_at(parent_cursor.value().offset -
                              parent_cursor.value().record.len(),
                          parent_cursor.value().record);

    // insert the new frame into the parent.
    auto inserted =
        parent_node.insert_record_after(cursor, new_key, new_frame->pgno());
    new_frame->set_parent(parent_frame->pgno(),
                          inserted.offset - inserted.record.len());

    return ErrorCode::Success;
}

tl::expected<Frame *, ErrorCode>
Index::allocate_frame(index_id_t index, uint8_t level, bool is_leaf) {
    // Log::GlobalLog() << std::format(
    //                         "[index]: allocate new frame for at level {}",
    //                         level)
    //                  << std::endl;

    return pool_->allocate_frame()
        .and_then([&](Frame *frame) -> tl::expected<Frame *, ErrorCode> {
            auto page = frame->page();
            page->hdr.index = index;
            page->hdr.level = level;
            page->hdr.number_of_records = 0;
            page->hdr.last_inserted = 0;
            page->hdr.is_leaf = is_leaf;
            page->hdr.parent_page = 0;
            memset(page->payload, 0, page->payload_len());

            // placeholder record.
            if (frame->is_leaf()) {
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

// FIXME:
void Index::traverse(const RecordTraverseFunc &func) {
    auto result = get_root_frame();
    if (!result)
        return;
    auto frame = result.value();
    if (frame->is_leaf()) {
        LeafIndexNode node(frame, comp_);
        node.traverse(func);
    } else {
        InternalIndexNode node(frame, comp_);
        node.traverse(func, pool_.get());
    }
}

// FIXME: current impl only reverse traversion inside a node, not the whole
// tree.
void Index::traverse_r(const RecordTraverseFunc &func) {
    // auto result = get_root_frame();
    // if (!result)
    //     return;
    // auto frame = result.value();
    // while (!frame->is_leaf()) {
    //     InternalIndexNode node(frame, comp_);
    //     auto first_record = node.first_user_cursor();
    //
    //     auto child = pool_->get_frame(first_record.record.value);
    //     if (!child)
    //         return;
    //     frame = child.value();
    // }
    //
    // LeafIndexNode node(frame, comp_);
    // node.traverse_r(func);
}
} // namespace storage
  //
