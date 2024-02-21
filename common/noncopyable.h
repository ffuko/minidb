#ifndef COMMON_NONCOPYABLE_H
#define COMMON_NONCOPYABLE_H

class NonCopyable {
public:
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;

    virtual ~NonCopyable() = default;

protected:
    NonCopyable() = default;
};

#endif // !COMMON_NONCOPYABLE_H
