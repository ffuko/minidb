#include "error.h"
#include "index/index.h"
#include "types.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace storage;
#define print_line                                                             \
    std::cout << "-----------------------------------------------------------" \
              << std::endl

TEST(IndexTest, BasicTest) {
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

    print_line;

    std::function<void(storage::LeafClusteredRecord &)> print =
        [](storage::LeafClusteredRecord &record) {
            std::cerr << record.key << ": " << record.value << std::endl;
        };

    index->traverse(print);

    print_line;

    index->traverse_r(print);

    print_line;

    for (auto &row : input) {
        auto ec = index->remove_record(row.first);
        ASSERT_EQ(ErrorCode::Success, ec)
            << "error is " << ErrorHandler().print_error(ec);

        auto result = index->search_record(row.first);
        ASSERT_EQ(false, result.has_value());
    }
    std::filesystem::remove("test.db");
}

TEST(IndexTest, ManyInsert) {
    KeyMeta key_meta = {"id", storage::key_t(KeyType::Int)};
    FieldMeta field_meta = {"score", storage::key_t(KeyType::Int)};
    std::vector<FieldMeta> fields_meta = {field_meta};

    auto index =
        Index::make_index(0, "test.db", key_meta, fields_meta, std::cerr);

    std::vector<std::pair<Key, Column>> input;
    for (int i = 0; i < 10000; i++) {
        input.push_back({i, {90}});
    }

    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(input), std::end(input), rng);

    for (auto &row : input) {
        auto ec = index->insert_record(row.first, row.second);
        ASSERT_EQ(ErrorCode::Success, ec)
            << "error is " << ErrorHandler().print_error(ec);
    }

    std::function<void(storage::LeafClusteredRecord &)> print =
        [](storage::LeafClusteredRecord &record) {
            std::cerr << record.key << ": " << record.value << std::endl;
        };

    // FIXME:
    // index->traverse(print);

    for (auto &row : input) {
        auto result = index->search_record(row.first);
        ASSERT_EQ(true, result.has_value())
            << "cannot find record because of "
            << ErrorHandler::print_error(result.error());
        ASSERT_EQ(row.second, result.value().value);
    }
}
