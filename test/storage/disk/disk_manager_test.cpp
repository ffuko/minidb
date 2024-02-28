#include "config.h"
#include "disk/disk_manager.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#define GTEST_COUT std::cerr << "[          ] [ INFO ]"
TEST(DBFileHeaderTest, SerializationTest) {
    storage::DBFileHeader hdr = {
        .page_count = 1,
        .use_count = 2,
        .free_array = 0,
    };
    auto raw = hdr.serialize();
    storage::DBFileHeader another_hdr = {0};
    another_hdr.deserialize(raw.get());
    ASSERT_EQ(hdr.page_count, another_hdr.page_count);
    ASSERT_EQ(hdr.page_count, another_hdr.page_count);
}

TEST(DiskManagerTest, CreateFileHeaderTest) {
    testing::internal::CaptureStdout();
    std::cout << "sizeof DBFileHeader: " << sizeof(storage::DBFileHeader)
              << std::endl;

    {
        storage::DiskManager disk("test_fh.db");
        std::fstream &io = disk.io();
        char *first_page = new char[config::PAGE_SIZE]{};

        io.seekg(0);
        io.read(first_page, config::PAGE_SIZE);
        storage::DBFileHeader hdr;
        hdr.deserialize(first_page);
        ASSERT_EQ(hdr.use_count, 0);
        ASSERT_EQ(hdr.page_count, 1);
        ASSERT_EQ(hdr.free_array, 0);
    }
    std::filesystem::remove("test_fh.db");
}

TEST(DiskManagerTest, BasicIOTest) {
    testing::internal::CaptureStderr();

    // std::fstream log;
    // log.open("disk_manager.log", std::ios::out | std::ios::trunc);

    // test new disk file
    {
        storage::DiskManager disk("test1.db");
        auto result = disk.get_free_page();
        ASSERT_EQ(true, result.has_value());

        std::shared_ptr<storage::Page> page = result.value();
        ASSERT_EQ(1, page->pgno())
            << std::format("file header: page_count {}, use_count {}, "
                           "free_array free_array ",
                           disk.file_header_.page_count,
                           disk.file_header_.use_count)
            << disk.file_header_.free_array << std::endl;

        {
            result = disk.get_free_page();
            ASSERT_EQ(true, result.has_value());
            page = result.value();
            ASSERT_EQ(2, page->pgno());
            page->hdr.pgno = 2;
            page->hdr.number_of_records = 1;
            page->payload =
                new char[config::LEAF_CLUSTER_RECORD_LEN]{'1', '2', 0};
            disk.write_page(page);
        }
        {
            result = disk.read_page(2);
            ASSERT_EQ(true, result.has_value());
            page = result.value();
            ASSERT_EQ(2, page->pgno());
            ASSERT_EQ(1, page->hdr.number_of_records);
            ASSERT_EQ('1', page->payload[0]);
            ASSERT_EQ('2', page->payload[1]);
            disk.write_page(page);
        }
        {
            disk.get_free_page();
            disk.set_page_free(2);
            auto result = disk.get_free_page();
            ASSERT_EQ(true, result.has_value());
            ASSERT_EQ(2, page->pgno())
                << std::format("file header: page_count {}, use_count {}, "
                               "free_array free_array ",
                               disk.file_header_.page_count,
                               disk.file_header_.use_count)
                << disk.file_header_.free_array << std::endl;
            disk.write_page(result.value());
        }
    }
    // test reopen
    {
        storage::DiskManager disk("test1.db");
        auto result = disk.get_free_page();
        ASSERT_EQ(true, result.has_value());

        std::shared_ptr<storage::Page> page = result.value();
        ASSERT_EQ(4, page->pgno())
            << std::format("file header: page_count {}, use_count {}, "
                           "free_array free_array ",
                           disk.file_header_.page_count,
                           disk.file_header_.use_count)
            << disk.file_header_.free_array << std::endl;
    }

    std::filesystem::remove("test1.db");
}
