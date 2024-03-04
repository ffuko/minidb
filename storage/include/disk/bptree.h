#ifndef STORAGE_INCLUDE_DISK_BPTREE_H
#define STORAGE_INCLUDE_DISK_BPTREE_H

#include "disk/pager.h"
#include "noncopyable.h"
#include "types.h"
#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace storsge {
namespace external /* on disk-based b+tree */ {

class Record {};
// any comparable value for bpree comparsion, including bool, int, float,
// std::string, Record*.
// NOTE: any is a dressed-up void*. variant is a dressed-up
// union any cannot store non-copy or non-move able types. variant can.
using Value = std::variant<bool, int, double, std::string, Record *>;

using Comparator = std::function<int(const Value &, const Value &)>;

class BpTree : NonCopyable {
public:
public:
    BpTree();
    ~BpTree() = default;

    void set_key_comparator(const Comparator &p);

private:
    pgno_t root_; // root page
    pgno_t last_inserted_;

    Comparator comp_; // comparison function
    std::unique_ptr<pager> pager_;
};
} // namespace external
} // namespace storsge

#endif // !STORAGE_INCLUDE_DISK_BPTREE_H
