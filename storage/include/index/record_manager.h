#ifndef STORAGE_INCLUDE_INDEX_RECORD_MANAGER_H
#define STORAGE_INCLUDE_INDEX_RECORD_MANAGER_H

#include "buffer/buffer_pool.h"
#include "error.h"
#include "noncopyable.h"
#include "types.h"
namespace storage {

class RecordManager : public NonCopyable {
public:
    RecordManager(BufferPoolManager *pool) : pool_(pool) {}
    ~RecordManager() = default;

    ErrorCode insert_record(const Key &key, const Column &value, page_id_t pgno,
                            page_off_t);
    ErrorCode remove_record(const Key &key, page_id_t pgno, page_off_t);

private:
    BufferPoolManager *pool_;
};
} // namespace storage

#endif // !STORAGE_INCLUDE_INDEX_RECORD_MANAGER_H
