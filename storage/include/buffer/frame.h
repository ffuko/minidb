#ifndef STORAGE_INCLUDE_BUFFER_FRAME_H
#define STORAGE_INCLUDE_BUFFER_FRAME_H

// #include "buffer/buffer_pool.h"
#include "config.h"
#include "disk/page.h"
#include "index/cursor.h"
#include "index/record.h"
#include "log.h"
#include "membuf.h"
#include "serialization.h"
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

    bool is_leaf() const { return page()->hdr.is_leaf; }
    bool is_root(int depth) const { return depth == page()->hdr.level + 1; }

    template <typename R>
    void init_list() {
        R infi{};
        infi.hdr.order = 0;
        R supre{};
        supre.hdr.order = -1;
        infi.hdr.length = dump_at(0, infi);
        supre.hdr.length = dump(supre);

        infi.hdr.next_record_offset = 0;
        supre.hdr.prev_record_offset = -supre.hdr.length - infi.hdr.length;
        Log::GlobalLog() << "infi: " << infi.hdr.length << std::endl;
        Log::GlobalLog() << "supre: " << supre.hdr.length << std::endl;

        // FIXME: serialization determination for infi and supre.
        dump_at(0, infi);
        dump(supre);
        set_last_inserted(ppos());

        Log::GlobalLog()
            << "[Frame]: dump infimum and supremum in the new frame"
            << std::endl;
    }

    template <typename T>
    page_off_t load(T &value) {
        Log::GlobalLog() << "	going to load from " << gpos() << std::endl;
        page_off_t before = gpos();
        std::istream is(&membuf_);
        serialization::deserialize(is, value);
        return gpos() - before;
    }

    // a generic function to load a Type from the physical page after the
    // specific offset(compared with current input pointer) using cereal's
    // serialization support.
    // @return is the size of the load.
    template <typename T>
    page_off_t load(int offset, T &value) {
        membuf_.seekg(offset);
        return load(value);
    }

    template <typename T>
    page_off_t dump(const T &value) {
        Log::GlobalLog() << "	going to dump at " << ppos() << std::endl;
        page_off_t before = ppos();
        std::ostream os(&membuf_);
        serialization::serialize(os, value);
        mark_dirty();
        return ppos() - before;
    }

    // a generic function to dump a Type into the physical page at the specific
    // offset using cereal's serialization support.
    // @return is the size of the dump.
    template <typename T>
    page_off_t dump(page_off_t offset, const T &value) {
        membuf_.seekp(offset);
        return dump(value);
    }

    // load the @value at the global offset @absolute by @lib cereal.
    // @return is the size of the load.
    template <typename T>
    page_off_t load_at(page_off_t absolute, T &value) {
        membuf_.setg(absolute);
        Log::GlobalLog() << "	going to load from " << gpos() << std::endl;
        std::istream is(&membuf_);
        serialization::deserialize(is, value);
        return gpos() - absolute;
    }

    // dump the @value at the absolute offset @absolute by @lib cereal.
    // @return is the size of the dump.
    template <typename T>
    page_off_t dump_at(page_off_t absolute, const T &value) {
        membuf_.setp(absolute);
        Log::GlobalLog() << "	going to dump at " << ppos() << std::endl;
        std::ostream os(&membuf_);
        serialization::serialize(os, value);
        mark_dirty();
        return ppos() - absolute;
    }

    // put pointer's position for the page's memory buffer
    page_off_t ppos() const { return membuf_.tellp(); }
    // get pointer's position for the page's memory buffer
    page_off_t gpos() const { return membuf_.tellg(); }

    void mark_dirty() { dirty_ = true; }
    void clear_dirty() { dirty_ = false; }
    bool is_dirty() { return dirty_; }

    bool is_full() const {
        return page()->hdr.number_of_records >= config::max_number_of_childs();
    }

    bool is_half_full() const {
        return page()->hdr.number_of_records == config::min_number_of_childs();
    }

    std::shared_ptr<Page> page() const { return page_; }
    page_id_t pgno() const { return page()->pgno(); }
    index_id_t index() const { return page()->hdr.index; }
    index_id_t level() const { return page()->hdr.level; }
    auto number_of_records() const { return page()->hdr.number_of_records; }

    // the start of next inserted record.
    page_off_t last_inserted() const { return page()->hdr.last_inserted; }
    void set_last_inserted(page_off_t pos) {
        page()->hdr.last_inserted = pos;
        mark_dirty();
    }

    Key key();

    tl::expected<Frame *, ErrorCode> parent_frame() const;

    tl::expected<Cursor<InternalClusteredRecord>, ErrorCode>
    parent_record() const;

    BufferPoolManager *pool() const { return pool_; }

    tl::expected<Frame *, ErrorCode> prev_frame();
    tl::expected<Frame *, ErrorCode> next_frame();
    void set_parent(page_id_t parent, page_off_t offset);

    // static tl::expected<Frame *, ErrorCode>
    // allocate_frame(index_id_t id, uint8_t order, bool is_leaf);

private:
    // NOTE: no boundry check, set carefully.
    BufferPoolManager *pool_;
    frame_id_t id_;
    std::shared_ptr<Page> page_;

    // make page payload field a membuf so that it's easier to do serialization.
    common::MemBuf membuf_;
    bool dirty_;
};

} // namespace storage
#endif // !STORAGE_INCLUDE_BUFFER_FRAME_H
