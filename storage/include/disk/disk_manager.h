#ifndef STORAGE_INCLUDE_BUFFER_DISK_MANAGER_H
#define STORAGE_INCLUDE_BUFFER_DISK_MANAGER_H

#include "buffer/buffer_pool.h"
#include "config.h"
#include "disk/page.h"
#include "error.h"
#include "scope_guard.h"
#include "tl/expected.hpp"
#include "types.h"
#include <bitset>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
namespace storage {

struct DBFileHeader {

    page_id_t page_count; // the number of allocated once pages.
    page_id_t use_count;  // the number of in-use pages

    // FIXME: a more efficient strategy for space locality.
    std::bitset<config::MAX_PAGE_NUM_PER_FILE>
        free_array; // free-or-not flag map for all pages.
    // TODO: more suitable layout
    // [[maybe_unused]] const char *left;

    // serialize the file header to a DBFileHeader-size byte stream.
    std::shared_ptr<char> serialize() const {
        // char *raw = new char[config::PAGE_SIZE];
        std::shared_ptr<char> raw =
            std::shared_ptr<char>(new char[sizeof(DBFileHeader)]{0});

        ::memcpy(raw.get(), this, sizeof(DBFileHeader));
        return raw;
    }

    // deserialize a page-size byte stream to a file header.
    // Dangerous: @raw must be larger than sizeof(DBFileHeader)
    void deserialize(const char *raw) {
        ::memcpy(this, raw, sizeof(DBFileHeader));
    }
};

// DiskManager is a global disk I/O handler for all buffer pools in a file.
// it reads/writes pages from/to a disk file and (de)serialize raw bytes
// into/from struct Page.
class DiskManager {
public:
    friend class BufferPoolManager;

    explicit DiskManager(const std::string &filename) : db_file_(filename) {
        // TODO: file format check
        db_io_.open(db_file_, std::ios::binary | std::ios::in | std::ios::out);

        // the file does not exist, create a new one
        if (!db_io_.is_open()) {
            db_io_.clear();
            // create a new file
            db_io_.open(db_file_, std::ios::binary | std::ios::trunc |
                                      std::ios::out | std::ios::in);
            if (!db_io_.is_open()) {
                throw std::runtime_error("failed to open the db file");
            }

            file_header_ = {
                .page_count = 1, .use_count = 0,
                // .free_array = 0,
            };

            auto ec = update_file_header();
            if (ec != ErrorCode::Success)
                throw std::runtime_error("failed to allocate the first page");
            return;
        }

        char *raw = new char[config::PAGE_SIZE]{0};
        common::make_scope_guard([&]() { delete[] raw; });

        db_io_.seekg(0);
        db_io_.read(raw, config::PAGE_SIZE);
        if (db_io_.bad())
            throw std::runtime_error("bad db first page");
        file_header_.deserialize(raw);
    }

    ~DiskManager() {
        // just in case
        update_file_header();
        db_io_.close();
    }

    // read record pages into std::shared_ptr<Page>
    tl::expected<std::shared_ptr<Page>, ErrorCode> read_page(page_id_t pgno) {
        int offset = pgno * config::PAGE_SIZE;
        char *data;
        // check if read beyond file length
        if (offset > std::filesystem::file_size(db_file_)) {
            return tl::unexpected(ErrorCode::DiskReadOverflow);
        }
        data = new char[config::PAGE_SIZE]();
        auto data_remove = common::make_scope_guard([&]() {
            if (data)
                delete data;
            data = nullptr;
        });
        // set read cursor to offset
        db_io_.seekp(offset);
        db_io_.read(data, config::PAGE_SIZE);
        if (db_io_.bad()) {
            return tl::unexpected(ErrorCode::DiskReadError);
        }

        // if file ends before reading PAGE_SIZE
        int read_count = db_io_.gcount();
        // read less than a page.
        if (read_count < config::PAGE_SIZE) {
            db_io_.clear();
            // std::cout << "Read less than a page" << std::endl;
        }
        std::shared_ptr<Page> page = std::make_shared<Page>(data);

        return page;
    }

    ErrorCode write_page(std::shared_ptr<Page> page) {
        auto result = page->serialize();
        if (!result)
            return result.error();

        page_id_t pgno = page->pgno();
        std::shared_ptr<char> raw = result.value();
        // set write cursor to offset
        uint32_t offset = static_cast<size_t>(pgno) * config::PAGE_SIZE;
        db_io_.seekp(offset);

        // write to the file
        db_io_.write(raw.get(), config::PAGE_SIZE);
        // check for I/O error
        if (db_io_.bad()) {
            return ErrorCode::DiskWriteError;
        }

        // flush to keep disk file in sync
        db_io_.flush();
        return ErrorCode::Success;
    }

    // get a free page, or allocate a new page if no more free pages.
    // NOTE: if a new page is allocated, only the pgno field in its header is
    // set. It's the caller's responsibility to init it and write to the disk!!!
    tl::expected<std::shared_ptr<Page>, ErrorCode> get_free_page() {
        page_id_t free_page = file_header_.page_count;

        // FIXME: O(n); use linked list to collect the free page.
        for (size_t i = 1; i < file_header_.page_count; i++) {
            if (file_header_.free_array[i]) {
                free_page = i;
                break;
            }
        }
        if (free_page < file_header_.page_count) {
            file_header_.free_array.set(free_page, false);
            update_file_header();
            std::cout << "found free page " << free_page << std::endl;
            return read_page(free_page);
        }

        // no more free pages, allocate a new one
        // zero fill
        std::filesystem::resize_file(
            db_file_, std::filesystem::file_size(db_file_) + config::PAGE_SIZE);

        // NOTE: only as a placeholder, no any data on the new page.
        std::shared_ptr<Page> page =
            std::make_shared<Page>(file_header_.page_count);

        std::cout << std::format("allocate new page {}", page->pgno())
                  << std::endl;

        file_header_.free_array.set(file_header_.page_count, false);
        file_header_.page_count++;
        file_header_.use_count++;
        update_file_header();

        return page;
    }

    // NOTE: lazy free: only append the to-be-freed page to the free list. it is
    // the caller's responsiblity to mark the page free in the page header>
    ErrorCode set_page_free(page_id_t pgno) {
        if (pgno < config::MAX_PAGE_NUM_PER_FILE) {
            file_header_.free_array.set(pgno, true);
            file_header_.use_count--;
            update_file_header();
            return ErrorCode::Success;
        }

        return ErrorCode::InvalidPageNum;
    }

#ifdef DEBUG
    // debug only
    std::fstream &io() { return db_io_; }
#endif // DEBUG

private:
    // update the header(the first page) of the disk file.
    // called every time the header changes in sync.
    ErrorCode update_file_header() {
        assert(db_io_.is_open());

        db_io_.seekg(0);
        auto raw = file_header_.serialize();
        db_io_.write(raw.get(), config::PAGE_SIZE);
        // check for I/O error
        if (db_io_.bad()) {
            return ErrorCode::DiskWriteError;
        }

        // flush to keep disk file in sync
        db_io_.flush();
        return ErrorCode::Success;
    }

    const std::string db_file_;
    std::fstream db_io_;

#ifdef DEBUG
public:
#endif // DEBUG
    DBFileHeader file_header_;
};

} // namespace storage

#endif
