#include "error.h"
#include "index/index.h"
#include "types.h"
#include <gtest/gtest.h>
#include <vector>

using namespace storage;
TEST(IndexTest, BasicInsertTest) {
    KeyMeta key_meta = {"id", storage::key_t(KeyType::Int)};
    FieldMeta field_meta = {"score", storage::key_t(KeyType::Int)};
    std::vector<FieldMeta> fields_meta = {field_meta};

    auto index =
        Index::make_index(0, "test.db", key_meta, fields_meta, std::cerr);

    std::vector<std::pair<Key, Column>> input = {
        {1, {80}}, {5, {80}}, {2, {80}}, {8, {80}}, {3, {80}}};
    for (auto &row : input) {
        ASSERT_EQ(ErrorCode::Success,
                  index->insert_record(row.first, row.second));
    }

    for (auto &row : input) {
        auto result = index->search_record(row.first);
        ASSERT_EQ(true, result.has_value())
            << "cannot find record because of "
            << ErrorHandler::print_error(result.error());
        ASSERT_EQ(row.second, result.value().value);
    }
}
