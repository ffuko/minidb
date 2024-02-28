#ifndef STORAGE_INCLUDE_BUFFER_BUFFER_POOL_H
#define STORAGE_INCLUDE_BUFFER_BUFFER_POOL_H

#include "buffer/frame.h"
#include "buffer/lru_cache.h"
#include "error.h"
#include "noncopyable.h"
#include "tl/expected.hpp"
#include "types.h"
#include <cstddef>
#include <functional>
#include <list>
// #include <memory>
namespace storage {

class DiskManager;
class Frame;

class BufferPoolManager : NonCopyable {
public:
    using TraverseFunc = std::function<ErrorCode(Frame *)>;

public:
    BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
        : pool_size_(pool_size), pool_(), cache_(pool_size),
          disk_manager_(disk_manager) {
        pool_.reserve(pool_size_);
        for (size_t i = 0; i < pool_size_; i++) {
            pool_.push_back(Frame(this, i));
            free_list_.push_back(i);
        }
        free_list_.push_back(-1);
    }

    ~BufferPoolManager() { // page_table_.clear();
        // NOTE: not clearing a list will lead to segmentation fault.
        free_list_.clear();
    }

    // get an existing page from the disk file and put into the buffer.
    tl::expected<Frame *, ErrorCode> get_page(page_id_t pgno);

    // create a new page in the file and put it into the buffer.
    // if the file has free pages, pick and use one; else, extends  the file.
    // NOTE: new page's payload field is nullptr
    tl::expected<Frame *, ErrorCode> allocate_page();

    // dispose a page and set it free.
    ErrorCode remove_page(page_id_t pgno);

    // pin an in-use page so that it can't get page-out.
    ErrorCode pin_page(page_id_t pgno);
    // unpin an in-use page so that it can get page-out.
    ErrorCode unpin_page(page_id_t pgno);

    // flush the dirty page, if not dirty, do nothing.
    ErrorCode flush_page(page_id_t pgno);

    // flush all dirty pages.
    ErrorCode flush_all();

    ErrorCode for_each(const TraverseFunc &func);
    // Frame **pool() { return pool_.get(); }

    tl::expected<frame_id_t, ErrorCode> get_free_frame_id() {
        if (free_list_.empty()) {
            auto result = cache_.victim();
            if (!result)
                return tl::unexpected(result.error());
            return result.value()->id();
        }

        // NOTE: return a reference which will be invalid after erasion
        // frame_id_t id = free_list_.front();

        frame_id_t id = free_list_.front();
        free_list_.pop_front();
        return id;
    }

private:
    tl::expected<Frame *, ErrorCode>
    get_free_frame(std::shared_ptr<Page> page) {
        Frame *frame;
        auto free = get_free_frame_id();
        if (!free)
            return tl::unexpected(free.error());
        frame = &pool_[free.value()];
        frame->reassign(page);

        ErrorCode ec = cache_.put(page->pgno(), frame);
        if (ec == ErrorCode::Success)
            return frame;
        else
            return tl::unexpected(ec);
    }

    using PageTable = std::unordered_map<page_id_t, frame_id_t>;
    using FrameLRUCache = LRUCacheWithPin<page_id_t, Frame *>;
    using MemPool = std::vector<Frame>;

    size_t pool_size_;
    MemPool pool_;
    FrameLRUCache cache_;
    // PageTable page_table_;

    DiskManager *disk_manager_;

    // available frame_id_t
    std::list<frame_id_t> free_list_;
    // use MemPool instead.
    // std::list<frame_id_t> free_list_;
};

} // namespace storage

#endif // STORAGE_INCLUDE_BUFFER_BUFFER_POOL_H
