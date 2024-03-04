#include "buffer/buffer_pool.h"
#include "disk/disk_manager.h"
#include "error.h"
#include "tl/expected.hpp"
#include "types.h"

namespace storage {
tl::expected<Frame *, ErrorCode> BufferPoolManager::get_frame(page_id_t pgno) {
    Frame *frame;
    if (cache_.get(pgno, frame) == ErrorCode::Success) {
        return &pool_[frame->id()];
    }
    auto result = disk_manager_->read_page(pgno);
    if (result) {
        return get_free_frame(result.value());
    } else {
        return tl::unexpected(result.error());
    }
}

tl::expected<Frame *, ErrorCode> BufferPoolManager::allocate_frame() {
    auto result = disk_manager_->get_free_page();
    if (!result)
        return tl::unexpected(result.error());

    auto frame = get_free_frame(result.value());
    if (frame) {
        // every new page is dirty
        frame.value()->mark_dirty();
        return frame;
    }

    return tl::unexpected(frame.error());
}

ErrorCode BufferPoolManager::remove_frame(Frame *frame) {
    free_list_.insert(free_list_.end(), frame->id());
    auto ec = cache_.remove(frame->pgno());
    if (ec != ErrorCode::Success)
        return ec;

    ec = disk_manager_->set_page_free(frame->pgno());
    if (ec != ErrorCode::Success)
        return ec;

    return ErrorCode::Success;
}

ErrorCode BufferPoolManager::pin_frame(page_id_t pgno) {
    auto ec = cache_.pin(pgno);
    if (ec != ErrorCode::Success)
        return ec;

    return ErrorCode::Success;
}

ErrorCode BufferPoolManager::unpin_frame(page_id_t pgno) {
    auto ec = cache_.unpin(pgno);
    if (ec != ErrorCode::Success)
        return ec;
    return ErrorCode::Success;
}

// flush the dirty page, if not dirty, do nothing.
// if pinned, return PageAlreadyPinned error.
ErrorCode BufferPoolManager::flush_frame(Frame *frame) {
    if (frame->is_dirty()) {
        auto ec = disk_manager_->write_page(frame->page());
        if (ec != ErrorCode::Success) {
            Log::GlobalLog() << "[BufferPoolManager]: failed to flush page "
                             << frame->pgno() << std::endl;
            return ec;
        }
    }

    Log::GlobalLog() << "[BufferPoolManager]: flushed page " << frame->pgno()
                     << std::endl;
    frame->clear_dirty();
    return ErrorCode::Success;
}

// flush all dirty pages. if the page is pinned, do noting on it.
ErrorCode BufferPoolManager::flush_all() {
    auto ec = for_each([this](Frame *frame) -> ErrorCode {
        if (frame->is_dirty()) {
            auto ec = disk_manager_->write_page(frame->page());
            return ErrorCode::Success;
        }
        return ErrorCode::Success;
    });

    Log::GlobalLog() << "[BufferPoolManager]: flushed all frames " << std::endl;
    return ErrorCode::Success;
}

ErrorCode BufferPoolManager::for_each(const TraverseFunc &func) {
    for (int i = 0; i < pool_size_; i++) {
        auto ec = func(&pool_[i]);
        if (ec != ErrorCode::Success)
            return ec;
    }
    return ErrorCode::Success;
}
} // namespace storage
