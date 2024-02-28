#ifndef STORAGE_INCLUDE_PAGE_H
#define STORAGE_INCLUDE_PAGE_H

#include "config.h"
#include "error.h"
#include "tl/expected.hpp"
#include "types.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace storage {
// PageHdr is a common header for all type of pages.
struct PageHdr {
    // uint16_t number_of_heap_records;

    // the number of the page
    page_id_t pgno;

    // the total number of user records in the index page.
    uint16_t number_of_records;
    uint16_t last_inserted;

    // the level of this index page in the index.
    // serves also as an indicator whether the page is a leaf or not.
    // FIXME: invalid and inconsistant for now
    uint8_t level;

    // FIXME: use page type instead.
    bool is_leaf;

    PageHdr(int level, bool is_leaf)
        : level(level), number_of_records(0), is_leaf(is_leaf) {}

    // default construction for later deserizaliztion.
    PageHdr(page_id_t pgno)
        : pgno(pgno), level(0), number_of_records(0), is_leaf(false) {}
};

// Page is the in-disk representation of a record page.
struct Page {
    PageHdr hdr;
    // payload stores all the records of a page.
    char *payload;

    // for deserizalize
    explicit Page(const char *raw) : hdr(0), payload(nullptr) {
        deserizalize(raw);
    }

    explicit Page(page_id_t pgno) : hdr(pgno) {}
    Page(int level, bool is_leaf) : hdr(level, is_leaf), payload(nullptr) {}

    ~Page() {
        if (payload != nullptr)
            delete[] payload;
        payload = nullptr;
    }

    page_id_t pgno() const { return hdr.pgno; }

    // FIXME: const leads to relocation linking error?
    static size_t HdrOffset;

    // deserizalize a page-size byte stream to struct Page.
    // NOTE: ensure @param data is as large as a page-size.
    void deserizalize(const char *data) {
        ::memcpy(this + HdrOffset, data, sizeof(PageHdr));
        size_t payload_len = 0;
        // TODO: only for clusterd index for now; secondary index cases?
        if (hdr.is_leaf) {
            payload_len =
                config::LEAF_CLUSTER_RECORD_LEN * hdr.number_of_records;
        } else {
            payload_len =
                config::INTERNAL_CLUSTER_RECORD_LEN * hdr.number_of_records;
        }
        payload = new char[payload_len];
        ::memcpy(payload, data + offsetof(Page, payload), payload_len);
    }

    // @return a shared_ptr to a page-size char block
    tl::expected<std::shared_ptr<char>, ErrorCode> serialize() const {
        std::shared_ptr<char> raw =
            std::shared_ptr<char>(new char[config::PAGE_SIZE]{0});
        ::memcpy(raw.get(), this + HdrOffset, sizeof(PageHdr));
        if (!payload)
            return tl::unexpected(ErrorCode::InvalidPagePayload);
        if (hdr.is_leaf) {
            ::memcpy(raw.get() + offsetof(Page, payload), payload,
                     config::LEAF_CLUSTER_RECORD_LEN * hdr.number_of_records);
        } else {
            ::memcpy(raw.get() + offsetof(Page, payload), payload,
                     config::INTERNAL_CLUSTER_RECORD_LEN *
                         hdr.number_of_records);
        }
        return raw;
    }
};

} // namespace storage

#endif // !STORAGE_INCLUDE_PAGE_H
