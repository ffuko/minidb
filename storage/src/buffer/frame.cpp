#include "buffer/frame.h"
#include "buffer/buffer_pool.h"

namespace storage {

Frame::~Frame() {
    if (is_dirty() && page_)
        pool_->flush_page(page_->pgno());
}

void Frame::reassign(std::shared_ptr<Page> page) {
    if (is_dirty())
        pool_->flush_page(page_->pgno());
    page_ = std::move(page);
    dirty_ = false;
}

} // namespace storage
