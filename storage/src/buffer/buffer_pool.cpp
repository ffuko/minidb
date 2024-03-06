#include "buffer/buffer_pool.h"
#include "disk/disk_manager.h"
#include "error.h"
#include "tl/expected.hpp"
#include "types.h"

namespace storage {
tl::expected<Frame *, ErrorCode> BufferPoolManager::get_frame(page_id_t pgno) {
    Frame *frame;
    if (cache_.get(pgno, frame) == ErrorCode::Success) {
        // Log::GlobalLog() << "[BufferPoolManager] got cached frame for page "
        //                  << frame->pgno() << std::endl;
        return frame;
    }
    if (pgno == 0)
        return tl::unexpected(ErrorCode::GetRootPage);
    auto result = disk_manager_->read_page(pgno);
    if (result) {
        // Log::GlobalLog()
        //     << "[BufferPoolManager] page not cached, read and cache page "
        //     << frame->pgno() << std::endl;
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
        // Log::GlobalLog() << "[BufferPoolManager] allocated frame for new page
        // "
        //                  << frame.value()->pgno() << std::endl;
        return frame;
    }

    return tl::unexpected(frame.error());
}

ErrorCode BufferPoolManager::remove_frame(Frame *frame) {
    free_list_.push_back(frame->id());
    auto ec = cache_.remove(frame->pgno());
    if (ec != ErrorCode::Success)
        return ec;

    Frame *test;
    assert(cache_.get(frame->pgno(), test) != ErrorCode::Success);
    ec = disk_manager_->set_page_free(frame->pgno());
    if (ec != ErrorCode::Success)
        return ec;

    // Log::GlobalLog() << "[BufferPoolManager] removed frame of page"
    //                  << frame->pgno() << std::endl;
    return ErrorCode::Success;
}

tl::expected<frame_id_t, ErrorCode> BufferPoolManager::get_free_frame_id() {
    if (free_list_.empty()) {
        auto result = cache_.victim();
        if (!result) {
            return tl::unexpected(result.error());
        }

        // Log::GlobalLog() << "[LRU] get a victim " << result.value()->id()
        //                  << std::endl;
        return result.value()->id();
    }

    // NOTE: return a reference which will be invalid after erasion
    // frame_id_t id = free_list_.front();

    frame_id_t id = free_list_.front();
    free_list_.pop_front();
    return id;
}

tl::expected<Frame *, ErrorCode>
BufferPoolManager::get_free_frame(std::shared_ptr<Page> page) {
    Frame *frame;
    auto free = get_free_frame_id();
    if (!free)
        return tl::unexpected(free.error());
    frame = &pool_[free.value()];
    frame->reassign(page);

    // Log::GlobalLog() << "[LRU] put " << page->pgno() << std::endl;
    ErrorCode ec = cache_.put(page->pgno(), frame);
    if (ec == ErrorCode::Success) {
        // Log::GlobalLog()
        //     << std::format(
        //            "[BufferPoolManager]: get a free frame {} for page {}",
        //            frame->id(), frame->pgno())
        //     << std::endl;
        return frame;
    } else {
        // Log::GlobalLog() << std::format(
        //                         "[BufferPoolManager]: failed get a free
        //                         frame")
        //                  << std::endl;
        return tl::unexpected(ec);
    }
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
            // Log::GlobalLog() << "[BufferPoolManager]: failed to flush page "
            //                  << frame->pgno() << std::endl;
            return ec;
        }
    }

    // Log::GlobalLog() << "[BufferPoolManager]: flushed page " << frame->pgno()
    //                  << std::endl;
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

    // Log::GlobalLog() << "[BufferPoolManager]: flushed all frames " <<
    // std::endl;
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
