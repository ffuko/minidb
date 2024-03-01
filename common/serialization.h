#ifndef COMMON_SERILIZATION_H
#define COMMON_SERILIZATION_H

#include "indexer.h"
#include <cereal/archives/binary.hpp>
#include <index/record.h>
#include <iostream>
#include <istream>
#include <ostream>
#include <variant>

namespace storage {

template <typename T, typename... Ts>
std::ostream &operator<<(std::ostream &os, const std::variant<T, Ts...> &v) {
    os << v.index();

    std::visit([&os](auto &&arg) { os << arg; }, v);
    return os;
}

// FIXME: unexpected behaviours
// template <typename T, typename... Ts>
// std::istream &operator>>(std::istream &is, std::variant<T, Ts...> &v) {
//     size_t index = 0;
//     is >> index;
//     std::cerr << "index: " << index << std::endl;
//     if (index >= sizeof...(Ts) + 1) {
//         std::cerr << "index: " << index << ", expected: < " << sizeof...(Ts)
//         + 1
//                   << std::endl;
//         throw std::out_of_range("variant index out of range");
//     }
//
//     if (index == 0) {
//         T value;
//         is >> value;
//         std::cerr << "value: " << value << std::endl;
//         v = value;
//     } else {
//         // FIXME: O(n) solution
//         int j = 1;
//         (
//             [&](Ts value) {
//                 if (j == index) {
//                     is >> value;
//                     v = std::move(value);
//                 } else {
//                     j++;
//                 }
//             },
//             ...);
//     }
//     return is;
// }
} // namespace storage

namespace serialization {

// cereal helper
template <typename T>
std::ostream &serialize(std::ostream &os, const T &value) {
    // os << record.hdr << record.key;
    // os /* << record.key */ << record.hdr;
    cereal::BinaryOutputArchive archive(os);
    archive(value);

    return os;
}

// cereal helper
template <typename T>
std::istream &deserialize(std::istream &is, T &value) {
    cereal::BinaryInputArchive archive(is);
    archive(value);
    return is;
}

} // namespace serialization

#endif // !COMMON_SERILIZATION_H
