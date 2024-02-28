#ifndef COMMON_ERROR_H
#define COMMON_ERROR_H

#include <string>
#include <vector>

enum class ErrorCode : size_t {
    Success,
    Failure,
    KeyNotFound,
    NodeNotFound,
    KeyAlreadyExist,

    KeyNotPinned,
    KeyAlreadyPinned,

    DiskWriteError,
    DiskReadError,
    DiskReadOverflow,

    FrameNotPinned,

    // cache error
    CacheNoMoreVictim,
    CacheEntryNotFound,

    // page related
    InvalidPageNum,
    InvalidPagePayload,

    // pool error
    PoolNoFreeFrame,
    DeletedPageNotExist
};

class ErrorHandler {
public:
    ErrorHandler() = default;
    ~ErrorHandler() = default;

    std::string print_error(ErrorCode ec) {
        return messages[static_cast<size_t>(ec)];
    }

private:
    std::vector<std::string> messages = {
        "Success", "Failure", "KeyNotFound", "NodeNotFound", "KeyAlreadyExist",

        "KeyNotPinned", "KeyAlreadyPinned",

        "DiskWriteError", "DiskReadError", "DiskReadOverflow",

        "FrameNotPinned",

        // cache error
        "CacheNoMoreVictim", "CacheEntryNotFound",

        // page related
        "InvalidPageNum", "InvalidPagePayload",

        // pool error
        "PoolNoFreeFrame", "DeletedPageNotExist"};
};

// class MyException : public std::runtime_error {
// public:
//     const char *what() const override { return msg_.c_str(); }
//
// private:
//     std::string msg_;
// };
#endif // !COMMON_ERROR_H
