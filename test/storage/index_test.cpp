#include "index.h"
#include "record.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <format>
#include <gtest/gtest.h>
#include <random>
#include <utility>
#include <vector>

using namespace storage;

#define GTEST_COUT std::cerr << "[          ] [ INFO ]"

TEST(IndexTest, InsertBasic) {
    Index index = Index();
    RecordMeta int_string_record = RecordMeta(RecordMeta::Type::Common, 1);
    FieldMeta key_meta = FieldMeta::register_string_field("name", true);
    Record *new_record1 = new ClusteredRecord(
        &int_string_record, Key(&key_meta, std::string("john1")));
    Record *new_record2 = new ClusteredRecord(
        &int_string_record, Key(&key_meta, std::string("john2")));
    Record *new_record3 = new ClusteredRecord(
        &int_string_record, Key(&key_meta, std::string("john2")));

    ASSERT_EQ(Index::OpStatus::Success, index.insert_record(new_record1));
    ASSERT_EQ(Index::OpStatus::Success, index.insert_record(new_record2));
    Record *record = index.search_record(Key(&key_meta, std::string("john1")));

    ASSERT_EQ(new_record1, record);
    auto status = index.insert_record(new_record2);
    ASSERT_EQ(status, Index::OpStatus::Failure);
}

TEST(IndexTest, InsertMany) {
    Index index = Index();
    RecordMeta int_string_record = RecordMeta(RecordMeta::Type::Common, 1);
    FieldMeta key_meta = FieldMeta::register_string_field("name", true);

    std::vector<std::pair<Key, Record *>> keys;
    keys.reserve(100);

    for (int i = 0; i < 1000; i++) {
        Key key = Key(&key_meta, std::format("john{}", i));
        Record *record = new ClusteredRecord(&int_string_record, key);

        keys.push_back(std::make_pair(key, record));
        ASSERT_EQ(Index::OpStatus::Success, index.insert_record(record));
    }
    GTEST_COUT << std::format("the depth of the index: {}", index.depth())
               << std::endl;
    GTEST_COUT << std::format("the number of the index's records: {}",
                              index.number_of_records())
               << std::endl;
    ASSERT_EQ(1000, index.number_of_records());

    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(keys), std::end(keys), rng);

    for (auto &pair : keys) {
        Key key = pair.first;
        Record *expected_record = pair.second;

        Record *record = index.search_record(key);
        ASSERT_EQ(expected_record, record)
            << "search error, the key is " << key;
    }
}

TEST(IndexTest, DeleteBasic) {
    Index index = Index();
    RecordMeta int_string_record = RecordMeta(RecordMeta::Type::Common, 1);
    FieldMeta key_meta = FieldMeta::register_string_field("name", true);

    Key key1 = Key(&key_meta, std::string("john1"));
    Key key2 = Key(&key_meta, std::string("john2"));
    Key key3 = Key(&key_meta, std::string("john3"));
    Record *new_record1 = new ClusteredRecord(&int_string_record, key1);
    Record *new_record2 = new ClusteredRecord(&int_string_record, key2);
    Record *new_record3 = new ClusteredRecord(&int_string_record, key3);

    ASSERT_EQ(Index::OpStatus::Success, index.insert_record(new_record1));
    ASSERT_EQ(Index::OpStatus::Success, index.insert_record(new_record2));
    ASSERT_EQ(2, index.number_of_records());

    ASSERT_EQ(Index::OpStatus::Success, index.remove_record(key1));
    ASSERT_EQ(nullptr, index.search_record(key1));
    ASSERT_EQ(1, index.number_of_records());

    auto status = index.insert_record(new_record3);
    ASSERT_EQ(status, Index::OpStatus::Success);
    ASSERT_EQ(2, index.number_of_records());
    ASSERT_EQ(new_record3, index.search_record(key3));
}

TEST(IndexTest, DeleteMany) {
    Index index = Index();
    RecordMeta int_string_record = RecordMeta(RecordMeta::Type::Common, 1);
    FieldMeta key_meta = FieldMeta::register_string_field("name", true);

    std::vector<std::pair<Key, Record *>> keys;
    keys.reserve(100);

    for (int i = 0; i < 10000; i++) {
        Key key = Key(&key_meta, std::format("john{}", i));
        Record *record = new ClusteredRecord(&int_string_record, key);

        keys.push_back(std::make_pair(key, record));
        index.insert_record(record);
    }

    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(keys), std::end(keys), rng);

    for (auto &pair : keys) {
        Key key = pair.first;
        Record *expected_record = pair.second;

        Record *before_delete = index.search_record(key);
        ASSERT_EQ(expected_record, before_delete);

        auto status = index.remove_record(key);
        ASSERT_EQ(Index::OpStatus::Success, status);

        Record *after_delete = index.search_record(key);
        ASSERT_EQ(nullptr, after_delete);
    }

    ASSERT_EQ(0, index.number_of_records());
}