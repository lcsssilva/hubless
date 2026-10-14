#include "modules/lighting/infrastructure/G915PerKeyFrames.hpp"

#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace {

using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::KeyColorMap;
using ghub::lighting::domain::KeyId;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::infrastructure::buildG915PerKeyFrames;
using ghub::lighting::infrastructure::g915FrameKeyCode;
using ghub::lighting::infrastructure::PerKeyFrame;
using Bytes = std::vector<std::uint8_t>;

KeyId key(std::string_view name)
{
    const auto found = G915KeyCatalog::findByName(name);
    EXPECT_TRUE(found.has_value()) << name;
    return *found; // NOLINT(bugprone-unchecked-optional-access)
}

std::uint8_t codeOf(std::string_view name)
{
    const auto& definition = G915KeyCatalog::definition(key(name));
    return g915FrameKeyCode(definition.zone, definition.index);
}

// Teclas comuns (zona 0x01) a partir de "Key: A": código = índice - 3 (A = 0x04 -> 0x01).
std::vector<KeyId> letters(std::size_t count)
{
    std::vector<KeyId> keys;
    for (std::size_t i = 0; i < count; ++i) {
        const char name[] = {'K', 'e', 'y', ':', ' ', static_cast<char>('A' + i), '\0'};
        keys.push_back(key(name));
    }
    return keys;
}

TEST(G915PerKeyFramesTest, TranslatesEachZoneAsTheOpenRgbDoes)
{
    EXPECT_EQ(codeOf("Key: A"), 0x01);            // teclado 0x04 - 0x03
    EXPECT_EQ(codeOf("Key: Left Control"), 0x68); // modificadores 0xE0 - 0x78
    EXPECT_EQ(codeOf("Key: G1"), 0xB4);           // teclas G 0x01 + 0xB3
    EXPECT_EQ(codeOf("Logo"), 0xD2);              // logo 0x01 + 0xD1
    EXPECT_EQ(codeOf("Key: Media Mute"), 0x9C);   // mídia: índice sem mudança
    EXPECT_EQ(codeOf("Key: Brightness"), 0x99);   // indicadores: índice sem mudança
}

TEST(G915PerKeyFramesTest, EmptyMapProducesNoFrame)
{
    EXPECT_TRUE(buildG915PerKeyFrames(KeyColorMap{}).empty());
}

TEST(G915PerKeyFramesTest, SingleKeyUsesALittleFrameTerminatedBy0xFF)
{
    const auto frames = buildG915PerKeyFrames(KeyColorMap{}.with(key("Key: A"), RgbColor{0x10, 0x20, 0x30}));

    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames[0], (PerKeyFrame{.function = 1, .params = Bytes{0x01, 0x10, 0x20, 0x30, 0xFF}}));
}

TEST(G915PerKeyFramesTest, FourDistinctColorsFillALittleFrameWithoutTerminator)
{
    KeyColorMap colors;
    const auto keys = letters(4);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        colors = colors.with(keys[i], RgbColor{static_cast<std::uint8_t>(i + 1), 0, 0});
    }

    const auto frames = buildG915PerKeyFrames(colors);

    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames[0], (PerKeyFrame{.function = 1,
                                      .params = Bytes{0x01, 1, 0, 0, 0x02, 2, 0, 0, 0x03, 3, 0, 0, 0x04, 4, 0, 0}}));
}

TEST(G915PerKeyFramesTest, FiveDistinctColorsSpillIntoASecondLittleFrame)
{
    KeyColorMap colors;
    const auto keys = letters(5);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        colors = colors.with(keys[i], RgbColor{static_cast<std::uint8_t>(i + 1), 0, 0});
    }

    const auto frames = buildG915PerKeyFrames(colors);

    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames[0].params.size(), 16U);
    EXPECT_EQ(frames[1], (PerKeyFrame{.function = 1, .params = Bytes{0x05, 5, 0, 0, 0xFF}}));
}

TEST(G915PerKeyFramesTest, ExactlyFourKeysOfOneColorStillUseALittleFrame)
{
    KeyColorMap colors;
    for (const auto id : letters(4)) {
        colors = colors.with(id, RgbColor{0xAA, 0xBB, 0xCC});
    }

    const auto frames = buildG915PerKeyFrames(colors);

    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames[0].function, 1);
    EXPECT_EQ(frames[0].params.size(), 16U);
}

TEST(G915PerKeyFramesTest, MoreThanFourKeysOfOneColorUseABigFrame)
{
    KeyColorMap colors;
    for (const auto id : letters(5)) {
        colors = colors.with(id, RgbColor{0xAA, 0xBB, 0xCC});
    }

    const auto frames = buildG915PerKeyFrames(colors);

    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames[0],
              (PerKeyFrame{.function = 6, .params = Bytes{0xAA, 0xBB, 0xCC, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF}}));
}

TEST(G915PerKeyFramesTest, FourteenKeysOfOneColorSplitIntoAFullBigFrameAndALittleOne)
{
    KeyColorMap colors;
    for (const auto id : letters(14)) {
        colors = colors.with(id, RgbColor{0x01, 0x02, 0x03});
    }

    const auto frames = buildG915PerKeyFrames(colors);

    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames[0], (PerKeyFrame{.function = 6,
                                      .params = Bytes{0x01, 0x02, 0x03, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                                      0x09, 0x0A, 0x0B, 0x0C, 0x0D}}));
    EXPECT_EQ(frames[1], (PerKeyFrame{.function = 1, .params = Bytes{0x0E, 0x01, 0x02, 0x03, 0xFF}}));
}

TEST(G915PerKeyFramesTest, BigFramesComeBeforeLittleFramesAndColorsFollowTheOpenRgbOrder)
{
    // RGBColor do OpenRGB = R | G << 8 | B << 16; o std::map ordena por esse valor. Azul (0x010000) vem depois
    // de vermelho (0x000001) mesmo aparecendo antes no catálogo.
    KeyColorMap colors;
    const auto keys = letters(7);
    colors = colors.with(keys[0], RgbColor{0, 0, 1}); // azul, 1 tecla -> quadro pequeno
    for (std::size_t i = 1; i < 6; ++i) {
        colors = colors.with(keys[i], RgbColor{1, 0, 0}); // vermelho, 5 teclas -> quadro grande
    }
    colors = colors.with(keys[6], RgbColor{2, 0, 0});

    const auto frames = buildG915PerKeyFrames(colors);

    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames[0].function, 6);
    EXPECT_EQ(frames[1], (PerKeyFrame{.function = 1, .params = Bytes{0x07, 2, 0, 0, 0x01, 0, 0, 1, 0xFF}}));
}

} // namespace
