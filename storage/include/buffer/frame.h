#ifndef STORAGE_INCLUDE_BUFFER_FRAME_H
#define STORAGE_INCLUDE_BUFFER_FRAME_H

#include "disk/page.h"
#include "error.h"
#include "tl/expected.hpp"
#include "types.h"
#include <memory>

namespace storage {

class BufferPoolManager;

// Frame is in-memory representation of a page.
class Frame {
public:
    // for buffer pool initialization only
    // Frame(frame_id_t id) : id_(id), page_(nullptr), dirty_(false) {}

    Frame(BufferPoolManager *pool, frame_id_t id)
        : pool_(pool), id_(id), page_(nullptr), dirty_(false) {}

    ~Frame();
    void reassign(std::shared_ptr<Page> page);

    void set_id(frame_id_t id) { id_ = id; }
    frame_id_t id() const { return id_; }

    ErrorCode serialize(const char *data) { return ErrorCode::Success; }
    // TODO:
    tl::expected<const char *, ErrorCode> deserialize() const {
        return nullptr;
    }

    void mark_dirty() { dirty_ = true; }
    void clear_dirty() { dirty_ = false; }
    bool is_dirty() { return dirty_; }

    std::shared_ptr<Page> page() const { return page_; }

    BufferPoolManager *pool() const { return pool_; }

private:
    // NOTE: no boundry check, set carefully.
    BufferPoolManager *pool_;
    frame_id_t id_;
    std::shared_ptr<Page> page_;
    bool dirty_;
};

} // namespace storage
#endif // !STORAGE_INCLUDE_BUFFER_FRAME_H
