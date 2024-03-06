#include "buffer/lru_cache.h"
#include <gtest/gtest.h>

TEST(LruTest, BasicTest) {
    using Cache = storage::LRUCacheWithPin<int, int>;

    Cache cache(10);
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(ErrorCode::Success, cache.put(i, i));
    }

    for (int i = 10; i < 20; i++) {
        int j;
        ASSERT_EQ(ErrorCode::Success, cache.get(i, j));
        ASSERT_EQ(i, j);
    }

    int i;
    cache.get(10, i);

    for (int i = 0; i < 9; i++) {
        int j;
        auto result = cache.victim();
        ASSERT_EQ(true, result.has_value());

        ASSERT_EQ(i + 10 + 1, result.value());
    }

    auto result = cache.victim();
    ASSERT_EQ(true, result.has_value());
    ASSERT_EQ(10, result.value());
}
