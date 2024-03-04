#ifndef COMMON_ERROR_H
#define COMMON_ERROR_H

#include <string>
#include <vector>

enum class ErrorCode : size_t {
    Success = 0,
    Failure,
    KeyNotFound,
    NodeNotFound,
    KeyAlreadyExist,

    // index error.
    InvalidKeyType,
    KeyNotPinned,
    KeyAlreadyPinned,
    InvalidInsertPos,

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
    DeletedPageNotExist,

    // node error
    NodeNotFull,
};

class ErrorHandler {
public:
    ErrorHandler() = default;
    ~ErrorHandler() = default;

    static std::string print_error(ErrorCode ec) {
        return messages[static_cast<size_t>(ec)];
    }

private:
    // FIXME: update
    static const inline std::vector<std::string> messages =
        std::vector<std::string>{"Success", "Failure", "KeyNotFound",
                                 "NodeNotFound", "KeyAlreadyExist",

                                 "KeyNotPinned", "KeyAlreadyPinned",

                                 "DiskWriteError", "DiskReadError",
                                 "DiskReadOverflow",

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
