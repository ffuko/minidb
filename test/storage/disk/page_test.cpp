#include "config.h"
#include "disk/page.h"
#include "error.h"
#include <gtest/gtest.h>

TEST(PageTest, SerializationTest) {
    storage::Page invalid_page(1, true);
    invalid_page.hdr.pgno = 1;
    invalid_page.hdr.number_of_records = 20;
    invalid_page.payload = nullptr;

    auto result = invalid_page.serialize();
    ASSERT_EQ(false, result.has_value());
    ASSERT_EQ(ErrorCode::InvalidPagePayload, result.error());

    storage::Page simple_leaf_page(1, true);
    simple_leaf_page.hdr.pgno = 2;
    simple_leaf_page.hdr.number_of_records = 1;
    simple_leaf_page.payload =
        new char[config::LEAF_CLUSTER_RECORD_LEN]{'1', '2', 0};
    auto serialized = simple_leaf_page.serialize();
    ASSERT_EQ(true, serialized.has_value());

    storage::Page deserizalized(serialized.value().get());
    ASSERT_EQ(simple_leaf_page.hdr.pgno, deserizalized.hdr.pgno);
    ASSERT_EQ(simple_leaf_page.hdr.number_of_records, deserizalized.hdr.number_of_records);
    ASSERT_EQ(simple_leaf_page.payload[0], deserizalized.payload[0]);
    ASSERT_EQ(simple_leaf_page.payload[1], deserizalized.payload[1]);
}
