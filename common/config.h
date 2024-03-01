#ifndef CONFIG_H
#define CONFIG_H
// #include "disk/page.h"
#include "types.h"
#include <cstddef>
#include <cstdint>

// FIXME: placeholder
namespace config {
// record spec;
static constexpr storage::page_off_t INFI_SUPRE_LEN = 100;
static constexpr storage::page_off_t LEAF_CLUSTER_RECORD_LEN = 100;
static constexpr storage::page_off_t INTERNAL_CLUSTER_RECORD_LEN = 100;

// common pages spec
static constexpr storage::page_off_t PAGE_SIZE = 1024;
// FIXME: as a placeholder for now.
// static constexpr size_t PAGE_HEADER_OFFSET;
static constexpr storage::page_id_t MAX_PAGE_NUM_PER_FILE = 1000;

// index pages spec
static constexpr storage::page_off_t INDEX_PAGE_HDR_LEN = 100;
// the offset of infi record
static constexpr storage::page_off_t INDEX_PAGE_DATA_OFFSET =
    INDEX_PAGE_HDR_LEN;
static constexpr storage::page_off_t INDEX_PAGE_FIRST_RECORD_OFFSET =
    INDEX_PAGE_DATA_OFFSET + INFI_SUPRE_LEN;
#ifdef DEBUG
static constexpr size_t MAX_NUMBER_OF_RECORDS_PER_PAGE = 16;
#else
static constexpr size_t MAX_NUMBER_OF_RECORDS_PER_PAGE = 256;
#endif // DEBUG

} // namespace config

#endif
