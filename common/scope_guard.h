#ifndef COMMON_SCOPE_GUARD_H
#define COMMON_SCOPE_GUARD_H

#include "noncopyable.h"
#include <type_traits>
#include <utility>

namespace common {
template <typename F>
class ScopeGuard : public NonCopyable {
public:
    explicit ScopeGuard(const F &f) : f_(f) {}
    explicit ScopeGuard(F &&f) : f_(std::move(f)) {}
    ~ScopeGuard() { f_(); }

private:
    F f_;
};

template <typename F>
auto make_scope_guard(F &&f) {
    return ScopeGuard<std::remove_reference_t<F>>(std::forward<F>(f));
}
} // namespace common

#endif // !COMMON_DEFER_H
