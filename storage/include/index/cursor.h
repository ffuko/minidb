#ifndef STORAGE_INCLUDE_INDEX_CURSOR_H
#define STORAGE_INCLUDE_INDEX_CURSOR_H

#include "types.h"
namespace storage {

template <typename R>
struct Cursor {
    page_id_t page;
    page_off_t offset;
    R record;

    Cursor(page_id_t page, page_off_t offset, const R &record)
        : page(page), offset(offset), record(record) {}
};

} // namespace storage
#endif // !STORAGE_INCLUDE_INDEX_CURSOR_H
