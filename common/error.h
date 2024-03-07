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
    DiskWriteOverflow,

    FrameNotPinned,

    // cache error
    CacheNoMoreVictim,
    CacheEntryNotFound,

    // page related
    InvalidPageNum,
    InvalidPagePayload,
    GetRootPage,

    // pool error
    PoolNoFreeFrame,
    DeletedPageNotExist,
    GetRootParent,

    // node error
    NodeNotFull,
    PopEmptyNode,
    RootHeightDecrease,

    UnknownException,
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
        std::vector<std::string>{
            "Success", "Failure", "KeyNotFound", "NodeNotFound",
            "KeyAlreadyExist", "InvalidKeyType",

            "KeyNotPinned", "KeyAlreadyPinned", "InvalidInsertPos",

            "DiskWriteError", "DiskReadError", "DiskReadOverflow",
            "DiskWriteOverflow",

            "FrameNotPinned",

            // cache error
            "CacheNoMoreVictim", "CacheEntryNotFound",

            // page related
            "InvalidPageNum", "InvalidPagePayload", "GetRootPage",

            // pool error
            "PoolNoFreeFrame", "DeletedPageNotExist", "GetRootParent",
            "NodeNotFull", "PopEmptyNode", "RootHeightDecrease",
            "UnknownException"};
};

// class MyException : public std::runtime_error {
// public:
//     const char *what() const override { return msg_.c_str(); }
//
// private:
//     std::string msg_;
// };
#endif // !COMMON_ERROR_H
