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
    index_id_t index;
    // the number of the page
    page_id_t pgno;

    // the total number of user records in the index page.
    uint16_t number_of_records;
    uint16_t last_inserted;

    // sibiling index pages in the same level.
    page_id_t prev_page;
    page_id_t next_page;

    // the level of this index page in the index.
    // serves also as an indicator whether the page is a leaf or not.
    // FIXME: invalid and inconsistant for now
    uint8_t level;
    // TODO: use page type instead.
    bool is_leaf;
    page_id_t parent_page;
    page_off_t parent_record_off;

    // default construction for later deserizaliztion.
    PageHdr(page_id_t pgno)
        : index(0), pgno(pgno), number_of_records(0),
          last_inserted(config::INDEX_PAGE_FIRST_RECORD_OFFSET), prev_page(0),
          next_page(0), level(0), is_leaf(false), parent_page(0),
          parent_record_off(0) {}

    template <class Archive>
    void serialize(Archive &archive) {
        archive(pgno, number_of_records, last_inserted, prev_page, next_page,
                level, is_leaf, parent_page, parent_record_off);
    }
};

// Page is the in-disk representation of a record page.
// FIXME: use a memory pool for payload field.
struct Page {
    PageHdr hdr;
    // payload stores all the records of a page; size: number_of_records *
    // RECORD_LEN
    char *payload;

    // for deserizalize
    explicit Page(const char *raw) : hdr(0), payload(new char[payload_len()]) {
        deserizalize(raw);
    }

    explicit Page(page_id_t pgno)
        : hdr(pgno), payload(new char[payload_len()]) {}

    ~Page() {
        if (payload != nullptr)
            delete[] payload;
        payload = nullptr;
    }

    page_id_t pgno() const { return hdr.pgno; }

    // return the length of the payload(all records).
    static constexpr page_off_t payload_len() {
        return config::PAGE_SIZE - sizeof(PageHdr);
    }

    // FIXME: const leads to relocation linking error?
    static size_t HdrOffset;
    static size_t PayloadOffset;

    // deserizalize a page-size byte stream to struct Page.
    // NOTE: ensure @param data is as large as a page-size.
    void deserizalize(const char *data);

    // @return a shared_ptr to a page-size char block
    tl::expected<std::shared_ptr<char>, ErrorCode> serialize() const;

    // template<class Archive>
    // void serialize(Archive &archive) {
    //     archive(hdr, ;)
    // }
};

} // namespace storage

#endif // !STORAGE_INCLUDE_PAGE_H
