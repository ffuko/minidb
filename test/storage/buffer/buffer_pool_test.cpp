#include "buffer/buffer_pool.h"
#include "disk/disk_manager.h"
#include "error.h"
#include "types.h"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <memory>

TEST(BufferPoolTest, BasicTest) {
    ErrorHandler handler;

    auto disk = std::make_shared<storage::DiskManager>("test.db");
    storage::BufferPoolManager pool(10, disk);
    // basic alloc
    storage::page_id_t pgno;
    {
        auto result = pool.allocate_frame();
        ASSERT_EQ(true, result.has_value());
        auto frame = result.value();
        ASSERT_EQ(0, frame->id());
    }
    {
        auto result = pool.allocate_frame();
        ASSERT_EQ(true, result.has_value());
        auto frame = result.value();
        ASSERT_EQ(1, frame->id());
        pgno = frame->page()->pgno();
    }
    {
        auto result = pool.allocate_frame();
        ASSERT_EQ(true, result.has_value());
        auto frame = result.value();
        ASSERT_EQ(2, frame->id());
    }

    // basic get/flush/remove
    {
        // write and flush
        {
            auto result = pool.get_frame(pgno);
            ASSERT_EQ(true, result.has_value());
            auto frame = result.value();
            ASSERT_EQ(1, frame->id());
            ASSERT_EQ(pgno, frame->page()->pgno());

            auto page = frame->page();
            // NOTE: new page's payload field is nullptr
            frame->page()->hdr.number_of_records = 1;
            frame->page()->payload =
                new char[config::LEAF_CLUSTER_RECORD_LEN]{'1', '2', 0};
            frame->mark_dirty();
            auto ec = pool.flush_frame(frame);
            ASSERT_EQ(ErrorCode::Success, ec);
        }
        // remove
        {
            auto ec = pool.remove_frame(pgno);
            ASSERT_EQ(ec, ErrorCode::Success) << std::format(
                "failed to remove page {}: {}", pgno, handler.print_error(ec));
        }
        // get again
        {
            auto result = pool.get_frame(pgno);
            ASSERT_EQ(true, result.has_value());
            auto frame = result.value();
            ASSERT_EQ(pgno, frame->page()->pgno());
            ASSERT_EQ(3, frame->id());
            ASSERT_EQ('1', frame->page()->payload[0]);
            ASSERT_EQ('2', frame->page()->payload[1]);
        }
    }
    std::filesystem::remove("test.db");
}
