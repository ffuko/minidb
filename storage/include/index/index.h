/*
 * b+ tree implementation.
 * */
#ifndef STORAGE_INCLUDE_INDEX_H
#define STORAGE_INCLUDE_INDEX_H

#include "buffer/buffer_pool.h"
#include "config.h"
#include "disk/disk_manager.h"
#include "error.h"
#include "index/index_meta.h"
#include "index/index_node.h"
#include "log.h"
#include "types.h"
#include <functional>
#include <memory>
#include <stdexcept>
#include <tl/expected.hpp>
#include <vector>

namespace storage {

// Index is the logical structure of a clustered index in the database which is
// a B+ tree in fact.
// a Index is built on leaf nodes and non-leaf nodes, both of which are
// instances of class IndexNode.
// NOTE: the first record of every index page has the minimum and has no
// meaning, only to serve as a placeholder for navigation.
// TODO: bulk-loading
class Index {
public:
#ifdef DEBUG
#endif // DEBUG
    using RecordTraverseFunc = std::function<void(LeafClusteredRecord &)>;
    template <typename N, typename R>
    using NodeTraverseFunc = std::function<void(IndexNode<N, R> *)>;

public:
    Index(const std::string &db_file, const IndexMeta &meta, std::ostream &os)
        : pool_(std::make_unique<BufferPoolManager>(
              config::DEFAULT_POOL_SIZE,
              std::make_shared<DiskManager>(db_file))),
          meta_(meta), log_(os) {}

    // Index(std::shared_ptr<DiskManager> disk, const std::string &config_file)
    //     :
    //     pool_(std::make_unique<BufferPoolManager>(config::DEFAULT_POOL_SIZE,
    //                                                 std::move(disk))),
    //       meta_{} {
    //     std::ifstream is;
    //     is.open(config_file, std::ios::in | std::ios::binary);
    //     if (!is.is_open())
    //         throw std::runtime_error("failed to open config file for index");
    //     serialization::deserialize(is, meta_);
    //
    // }

    // construction for new indices.
    Index(index_id_t id, const std::string &db_file, std::ostream &log)
        : pool_(std::unique_ptr<BufferPoolManager>(
              new BufferPoolManager(config::DEFAULT_POOL_SIZE,
                                    std::make_shared<DiskManager>(db_file)))),
          meta_{}, log_(log) {}

    static std::shared_ptr<Index>
    make_index(index_id_t id, const std::string &db_file, const KeyMeta &key,
               std::vector<FieldMeta> &fields, std::ostream &log) {
        // FIXME: new_root_frame().
        auto index = std::make_shared<Index>(id, db_file, log);
        auto result = index->allocate_frame(id, 0, true);
        if (!result) {
            Log::GlobalLog()
                << "[index]: failed to allocate root page for the new index "
                << id << std::endl;

            throw std::runtime_error(
                "[index]: failed to allocate root page for the new index");
        }

        index->meta_ = IndexMeta::make_index_meta(id, key, fields);
        index->meta_.root_page = result.value()->pgno();
        Log::GlobalLog() << "[index]: make new index of id " << id << std::endl;
        return index;
    }

    ~Index() = default;

    // get the left sibling or the desired record, whose is <= disired recor.
    tl::expected<Cursor<LeafClusteredRecord>, ErrorCode>
    get_cursor(const Key &key);
    // search for the desired record.
    tl::expected<LeafClusteredRecord, ErrorCode> search_record(const Key &key);
    // insert a clusterd leaf record.
    ErrorCode insert_record(const Key &key, const Column &value);
    // remove a clusterd leaf record.
    ErrorCode remove_record(const Key &key);

    template <typename N, typename R>
    ErrorCode full_node_scan(NodeTraverseFunc<N, R> func);
    ErrorCode full_scan(RecordTraverseFunc func);

    void traverse(const RecordTraverseFunc &func);
    void traverse_r(const RecordTraverseFunc &func);

    int depth() const { return meta_.depth; }

    index_id_t id() const { return meta_.id; }

    int number_of_records() const { return meta_.number_of_records; }

private:
    tl::expected<Frame *, ErrorCode> new_nonleaf_root(Frame *child);
    auto get_root_frame() { return pool_->get_frame(meta_.root_page); }

    tl::expected<Frame *, ErrorCode> search_leaf(const Key &key);
    // void rebalance(LeafIndexNode *node);
    template <typename N>
    ErrorCode balance_for_delete(Frame *frame);
    ErrorCode balance_for_insert(Frame *frame);
    ErrorCode rebalance_internal(Frame *frame);
    ErrorCode safe_node_split(Frame *frame, Frame *parent_frame);
    bool sibling_union_check(Frame *frame);
    void union_frame(Frame *, Frame *);

    // responsible to init a new frame. @child is only used when initing a
    // internal frame.
    // FIXME: use 2 separate functions
    tl::expected<Frame *, ErrorCode>
    allocate_frame(index_id_t id, uint8_t order, bool is_leaf);
    //
    // LeafIndexNode *union_node(LeafIndexNode *left_node,
    //                           LeafIndexNode *right_node);
    // LeafIndexNode *sibling_union_check(LeafIndexNode *node);

private:
    IndexMeta meta_;
    std::unique_ptr<BufferPoolManager> pool_;

    Comparator comp_;
    std::ostream &log_;
};

} // namespace storage
#endif
