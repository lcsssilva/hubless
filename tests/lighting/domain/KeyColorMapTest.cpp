#include "modules/lighting/domain/KeyColorMap.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace ghub::lighting::domain;
using ghub::shared_kernel::ErrorCode;

namespace {

KeyId key(const char* name)
{
    const auto id = G915KeyCatalog::findByName(name);
    return id.value(); // NOLINT(bugprone-unchecked-optional-access) -- vazio lança e o teste falha
}

constexpr RgbColor kRed{255, 0, 0};
constexpr RgbColor kBlue{0, 0, 255};

} // namespace

TEST(KeyColorMapTest, StartsEmpty)
{
    const KeyColorMap map;

    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0U);
    EXPECT_FALSE(map.colorOf(key("Key: A")).has_value());
}

TEST(KeyColorMapTest, CreatesFromDistinctKeys)
{
    const auto map = KeyColorMap::create({{key("Key: A"), kRed}, {key("Key: B"), kBlue}});

    ASSERT_TRUE(map.has_value());
    EXPECT_EQ(map->size(), 2U);
    EXPECT_EQ(map->colorOf(key("Key: A")), kRed);
    EXPECT_EQ(map->colorOf(key("Key: B")), kBlue);
    EXPECT_FALSE(map->colorOf(key("Key: C")).has_value());
}

TEST(KeyColorMapTest, CreateRejectsADuplicatedKeyEvenWithTheSameColor)
{
    const auto map = KeyColorMap::create({{key("Key: A"), kRed}, {key("Key: A"), kRed}});

    ASSERT_FALSE(map.has_value());
    EXPECT_EQ(map.error().code, ErrorCode::InvalidArgument);
}

TEST(KeyColorMapTest, WithAddsAKeyWithoutChangingTheOriginal)
{
    const KeyColorMap original;

    const auto changed = original.with(key("Key: A"), kRed);

    EXPECT_TRUE(original.empty());
    EXPECT_EQ(changed.colorOf(key("Key: A")), kRed);
}

TEST(KeyColorMapTest, WithReplacesTheColorOfAnExistingKey)
{
    const auto map = KeyColorMap{}.with(key("Key: A"), kRed).with(key("Key: A"), kBlue);

    EXPECT_EQ(map.size(), 1U);
    EXPECT_EQ(map.colorOf(key("Key: A")), kBlue);
}

TEST(KeyColorMapTest, IteratesInCatalogOrderRegardlessOfInsertionOrder)
{
    const auto map = KeyColorMap{}.with(key("Key: Z"), kBlue).with(key("Key: A"), kRed).with(key("Key: M"), kRed);

    std::vector<KeyId> order;
    for (const auto& [id, color] : map) {
        order.push_back(id);
    }

    EXPECT_EQ(order, (std::vector<KeyId>{key("Key: A"), key("Key: M"), key("Key: Z")}));
}

TEST(KeyColorMapTest, ComparesByContentNotByInsertionOrder)
{
    const auto first = KeyColorMap{}.with(key("Key: A"), kRed).with(key("Key: B"), kBlue);
    const auto second = KeyColorMap{}.with(key("Key: B"), kBlue).with(key("Key: A"), kRed);

    EXPECT_EQ(first, second);
    EXPECT_NE(first, KeyColorMap{}.with(key("Key: A"), kRed));
}
