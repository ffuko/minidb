#ifndef CONFIG_H
#define CONFIG_H
// #include "disk/page.h"
#include <cstddef>
#include <cstdint>

namespace config {
// page related
static constexpr uint32_t PAGE_SIZE = 1024;
// FIXME: as a placeholder for now.
// static constexpr size_t PAGE_HEADER_OFFSET;
static constexpr uint32_t MAX_PAGE_NUM_PER_FILE = 1000;

// records releated; FIXME: placeholder
static constexpr size_t LEAF_CLUSTER_RECORD_LEN = 100;
static constexpr size_t INTERNAL_CLUSTER_RECORD_LEN = 100;

} // namespace config

#endif
