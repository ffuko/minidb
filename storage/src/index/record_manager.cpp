#include "index/record_manager.h"
#include "error.h"

namespace storage {
ErrorCode RecordManager::insert_record(const Key &key, const Column &value,
                                       page_id_t pgno, page_off_t) {
    return ErrorCode::Success;
}

ErrorCode RecordManager::remove_record(const Key &key, page_id_t pgno,
                                       page_off_t) {
    return ErrorCode::Success;
}

} // namespace storage
