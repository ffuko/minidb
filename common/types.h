#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace storage {
// types for table
using table_id_t = uint16_t;

// type for index info, FIXME: use 2 bit?
using index_t = uint8_t;

// type for frame id in buffer management
using frame_id_t = size_t;
// type for page number
using page_id_t = uint32_t;
// page offset in bytes
using page_off_t = uint32_t;
// type for record number in a page
using record_id_t = uint32_t;
using index_id_t = uint8_t;

using key_t = uint8_t;
using value_t = uint8_t;
using record_t = uint8_t;
enum class KeyType : key_t { Int = 0, Double, String };
enum class ValueType : value_t { Int = 0, Double, String, Bool };

enum class RecordType : record_t { Infi, Supre, Leaf, Internal };

// any comparable value for bpree comparsion, including bool, int, float,
// std::string, Record*.
// NOTE: any is a dressed-up void*. variant is a dressed-up
// union any cannot store non-copy or non-move able types. variant can.
using Key = std::variant<int, double, std::string>;
using Value = std::variant<bool, int, double, std::string>;
using Column = std::vector<Value>;
using Comparator = std::function<int(const Value &, const Value &)>;
} // namespace storage

#endif // !COMMON_TYPES_H
