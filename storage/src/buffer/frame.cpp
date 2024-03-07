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

// return nullptr if the frame is the root frame.
// FIXME: return an error instead of nullptr when the frame is the root.
tl::expected<Frame *, ErrorCode> Frame::parent_frame() const {
    if (page()->hdr.parent_page == 0)
        return nullptr;

    return pool_->get_frame(page()->hdr.parent_page);
}

tl::expected<Cursor<InternalClusteredRecord>, ErrorCode>
Frame::parent_record() const {
    InternalClusteredRecord record;
    return parent_frame()
        .and_then(
            [this](Frame *parent)
                -> tl::expected<Cursor<InternalClusteredRecord>, ErrorCode> {
                if (!parent) {
                    return tl::unexpected(ErrorCode::GetRootParent);
                }

                InternalClusteredRecord record;
                parent->load_at(this->page()->hdr.parent_record_off, record);

                return Cursor<InternalClusteredRecord>{parent->pgno(),
                                                       parent->gpos(), record};
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
