#include "buffer/frame.h"
#include "buffer/buffer_pool.h"
#include "index/cursor.h"
#include "types.h"

namespace storage {

Frame::~Frame() {
    if (is_dirty() && page_)
        pool_->flush_frame(this);
}

// template <typename T>
// page_off_t Frame::dump_at(page_off_t absolute, const T &value) {
//     membuf_.setp(absolute);
//     std::ostream os(&membuf_);
//     serialization::serialize(os, value);
//     return ppos() - absolute;
// }

void Frame::reassign(std::shared_ptr<Page> page) {
    if (is_dirty())
        pool_->flush_frame(this);
    page_ = std::move(page);
    clear_dirty();

    // membuf_ =
    //     std::make_shared<common::MemBuf>(page_->payload,
    //     page_->payload_len());
    membuf_.init(page_->payload, page_->payload_len());
}

Key Frame::key() {
    // FIXME: use a generic infimum record instead.
    if (is_leaf()) {
        LeafClusteredRecord record;
        load_at(0, record);
        load(record.hdr.next_record_offset - record.hdr.length, record);
        return record.key;
    } else {
        InternalClusteredRecord record;
        load_at(0, record);
        load(record.hdr.next_record_offset - record.hdr.length, record);
        return record.key;
    }
}

// return nullptr if the frame is the root frame.
tl::expected<Frame *, ErrorCode> Frame::parent_frame() const {
    if (page()->hdr.parent_page == 0)
        return nullptr;

    return pool_->get_frame(page()->hdr.parent_page)
        .and_then([this](Frame *frame) -> tl::expected<Frame *, ErrorCode> {
            return this->pool_->get_frame(frame->page()->hdr.parent_page);
        })
        .or_else([](auto error) -> tl::expected<Frame *, ErrorCode> {
            return tl::unexpected(error);
        });
}

tl::expected<Cursor<InternalClusteredRecord>, ErrorCode>
Frame::parent_record() const {
    InternalClusteredRecord record;
    return parent_frame()
        .and_then(
            [this](Frame *parent)
                -> tl::expected<Cursor<InternalClusteredRecord>, ErrorCode> {
                InternalClusteredRecord record;
                parent->load_at(this->page()->hdr.parent_record_off, record);
                return Cursor<InternalClusteredRecord>{
                    parent->pgno(), this->page()->hdr.parent_record_off,
                    record};
            })
        .or_else(
            [](ErrorCode ec)
                -> tl::expected<Cursor<InternalClusteredRecord>, ErrorCode> {
                return tl::unexpected(ec);
            });
}

void Frame::set_parent(page_id_t parent, page_off_t offset) {
    page()->hdr.parent_page = parent;
    page()->hdr.parent_record_off = offset;
    mark_dirty();
}

tl::expected<Frame *, ErrorCode> Frame::prev_frame() {
    return pool_->get_frame(page()->hdr.prev_page);
}

tl::expected<Frame *, ErrorCode> Frame::next_frame() {
    return pool_->get_frame(page()->hdr.next_page);
}

// tl::expected<Frame *, ErrorCode>
// Frame::allocate_frame(index_id_t id, uint8_t order, bool is_leaf) {
//     return pool_->allocate_frame()
// }

} // namespace storage
