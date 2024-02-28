#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

namespace storage {

// type for frame id in buffer management
using frame_id_t = size_t;
// type for page number
using page_id_t = uint32_t;
// type for record number in a page
using record_id_t = uint32_t;
using index_id_t = uint8_t;

class Record;

// any comparable value for bpree comparsion, including bool, int, float,
// std::string, Record*.
// NOTE: any is a dressed-up void*. variant is a dressed-up
// union any cannot store non-copy or non-move able types. variant can.
using Value = std::variant<bool, int, double, std::string, Record *>;
using Comparator = std::function<int(const Value &, const Value &)>;
} // namespace storage

#endif // !COMMON_TYPES_H
