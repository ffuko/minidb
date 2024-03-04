#include "cereal/archives/binary.hpp"
#include "index/record.h"
#include "serialization.h"
#include "types.h"
#include <fstream>
#include <gtest/gtest.h>

using namespace storage;

TEST(RecordTest, SerializationTest) {
    std::fstream fs;
    fs.open("record_test.db",
            std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);

    // record header
    {
        fs.seekg(0);
        auto expected =
            RecordHdr{.order = 1, .status = 2, .next_record_offset = 345};

        // operator <</>> overload
        // fs << expected;
        // RecordHdr compared;
        // fs.seekg(0);
        // fs >> compared;

        // use cereal
        {
            cereal::BinaryOutputArchive oa(fs);
            oa(expected);
        }
        RecordHdr compared;
        {
            fs.seekg(0);
            cereal::BinaryInputArchive ia(fs);
            ia(compared);
        }

        ASSERT_EQ(expected, compared);
    }
    // internal record
    {
        storage::InternalClusteredRecord expected{
            33,
            1,
        };
        expected.hdr =
            RecordHdr{.order = 1, .status = 0, .next_record_offset = 100};

        storage::InternalClusteredRecord compared;
        // cereal
        {
            // {
            //     fs.seekg(0);
            //     cereal::BinaryOutputArchive oa(fs);
            //     oa(expected);
            // }
            // {
            //     fs.seekg(0);
            //     cereal::BinaryInputArchive ia(fs);
            //     ia(compared);
            //     ASSERT_EQ(expected.hdr, compared.hdr);
            //     ASSERT_EQ(expected.key, compared.key);
            // }
            fs.seekg(0);
            serialization::serialize(fs, expected);

            fs.seekg(0);

            serialization::deserialize(fs, compared);
            ASSERT_EQ(expected.hdr, compared.hdr);
            ASSERT_EQ(expected.key, compared.key);
        }
        // FIXME: own impl failed
        // {
        //     fs.seekg(0);
        //     fs << expected;
        //     fs.flush();
        //
        //     fs.seekg(0);
        //     fs >> compared;
        //
        //     ASSERT_EQ(expected.hdr, compared.hdr);
        //     ASSERT_EQ(expected.key, compared.key);
        // }
    }
    // leaf record
    {
        storage::LeafClusteredRecord expected;
        expected.hdr = {.order = 1, .status = 0, .next_record_offset = 100};
        expected.key = 2;
        expected.value = {
            "test",
        };

        storage::LeafClusteredRecord compared;

        fs.seekg(0);
        serialization::serialize(fs, expected);

        fs.seekg(0);
        serialization::deserialize(fs, compared);
        ASSERT_EQ(expected.hdr, compared.hdr);
        ASSERT_EQ(expected.key, compared.key);
        ASSERT_EQ(expected.value, compared.value);
    }
    fs.close();
}
