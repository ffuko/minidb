#ifndef COMMON_ERROR_H
#define COMMON_ERROR_H

#include <cstdint>

enum class ErrorCode : uint8_t {
    Success,
    Failure,
    KeyNotFound,
    NodeNotFound,
    KeyAlreadyExist
};

#endif // !COMMON_ERROR_H
