#ifndef STORAGE_INCLUDE_BUFFER_BUFFER_POOL_H
#define STORAGE_INCLUDE_BUFFER_BUFFER_POOL_H

#include "buffer/frame.h"
#include "buffer/lru_cache.h"
#include "error.h"
#include "log.h"
#include "noncopyable.h"
#include "tl/expected.hpp"
#include "types.h"
#include <cstddef>
#include <format>
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
    BufferPoolManager(size_t pool_size,
                      std::shared_ptr<DiskManager> disk_manager)
        : pool_size_(pool_size), pool_(), cache_(pool_size),
          disk_manager_(std::move(disk_manager)) {
        pool_.reserve(pool_size_);
        for (size_t i = 0; i < pool_size_; i++) {
            pool_.push_back(Frame(this, i));
            free_list_.push_back(i);
        }
    }

    ~BufferPoolManager() { // page_table_.clear();
        free_list_.clear();
    }

    // get an existing page from the disk file and put into the buffer.
    tl::expected<Frame *, ErrorCode> get_frame(page_id_t pgno);

    // create a new page in the file and put it into the buffer.
    // if the file has free pages, pick and use one; else, extends  the file.
    // every new page allocated is marked dirty.
    tl::expected<Frame *, ErrorCode> allocate_frame();

    // dispose a page and set it free.
    ErrorCode remove_frame(Frame *frame);

    // pin an in-use page so that it can't get page-out.
    ErrorCode pin_frame(page_id_t pgno);
    // unpin an in-use page so that it can get page-out.
    ErrorCode unpin_frame(page_id_t pgno);

    // flush the dirty page, if not dirty, do nothing.
    ErrorCode flush_frame(Frame *frame);

    // flush all dirty pages.
    ErrorCode flush_all();

    ErrorCode for_each(const TraverseFunc &func);
    // Frame **pool() { return pool_.get(); }

private:
    tl::expected<frame_id_t, ErrorCode> get_free_frame_id();
    tl::expected<Frame *, ErrorCode> get_free_frame(std::shared_ptr<Page> page);

    using PageTable = std::unordered_map<page_id_t, frame_id_t>;
    using FrameLRUCache = LRUCacheWithPin<page_id_t, Frame *>;
    using MemPool = std::vector<Frame>;

    size_t pool_size_;
    MemPool pool_;
    FrameLRUCache cache_;
    // PageTable page_table_;

    std::shared_ptr<DiskManager> disk_manager_;

    // available frame_id_t
    std::list<frame_id_t> free_list_;
    // use MemPool instead.
    // std::list<frame_id_t> free_list_;
};

} // namespace storage

#endif // STORAGE_INCLUDE_BUFFER_BUFFER_POOL_H
