#include "modules/lighting/domain/ScreenRegions.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace {

using ghub::lighting::domain::averageRegions;
using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::PixelFormat;
using ghub::lighting::domain::RegionGrid;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::domain::ScreenFrameView;
using ghub::lighting::domain::screenRegionsFrame;
using ghub::shared_kernel::ErrorCode;

// Imagem sintética: `columns` faixas verticais de cores conhecidas; stride com preenchimento (`padding` bytes).
struct Image {
    std::vector<std::uint8_t> bytes;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
    PixelFormat format;

    [[nodiscard]] ScreenFrameView view() const { return {bytes, width, height, stride, format}; }
};

void put(Image& image, std::uint32_t x, std::uint32_t y, RgbColor c)
{
    auto* p = image.bytes.data() + (static_cast<std::size_t>(y) * image.stride) + (static_cast<std::size_t>(x) * 4);
    if (image.format == PixelFormat::Bgrx) {
        p[0] = c.blue();
        p[1] = c.green();
        p[2] = c.red();
    } else {
        p[0] = c.red();
        p[1] = c.green();
        p[2] = c.blue();
    }
    p[3] = 0xAB; // byte X/alfa: nunca pode influenciar o resultado
}

Image make(std::uint32_t width, std::uint32_t height, std::uint32_t padding, PixelFormat format)
{
    Image image{{}, width, height, (width * 4) + padding, format};
    image.bytes.assign(static_cast<std::size_t>(image.stride) * height, 0x7F); // preenchimento não-zero
    return image;
}

void fill(Image& image, std::uint32_t x0, std::uint32_t x1, std::uint32_t y0, std::uint32_t y1, RgbColor c)
{
    for (auto y = y0; y < y1; ++y) {
        for (auto x = x0; x < x1; ++x) {
            put(image, x, y, c);
        }
    }
}

constexpr RgbColor kRed{255, 0, 0};
constexpr RgbColor kGreen{0, 255, 0};
constexpr RgbColor kBlue{0, 0, 255};
constexpr RgbColor kBlack{0, 0, 0};

TEST(AverageRegionsTest, UniformColumnsGiveTheirColorInBgrxAndRgbx)
{
    for (const auto format : {PixelFormat::Bgrx, PixelFormat::Rgbx}) {
        auto image = make(30, 4, 16, format);
        fill(image, 0, 10, 0, 4, kRed);
        fill(image, 10, 20, 0, 4, kGreen);
        fill(image, 20, 30, 0, 4, kBlue);

        const auto regions = averageRegions(image.view(), {.columns = 3, .rows = 1});

        ASSERT_TRUE(regions.has_value());
        ASSERT_EQ(regions->size(), 3U);
        EXPECT_EQ((*regions)[0], kRed);
        EXPECT_EQ((*regions)[1], kGreen);
        EXPECT_EQ((*regions)[2], kBlue);
    }
}

TEST(AverageRegionsTest, RowsAreReportedRowMajor)
{
    auto image = make(4, 4, 0, PixelFormat::Bgrx);
    fill(image, 0, 2, 0, 2, kRed);
    fill(image, 2, 4, 0, 2, kGreen);
    fill(image, 0, 2, 2, 4, kBlue);
    fill(image, 2, 4, 2, 4, kBlack);

    const auto regions = averageRegions(image.view(), {.columns = 2, .rows = 2});

    ASSERT_TRUE(regions.has_value());
    EXPECT_EQ((*regions)[0], kRed);
    EXPECT_EQ((*regions)[1], kGreen);
    EXPECT_EQ((*regions)[2], kBlue);
    EXPECT_EQ((*regions)[3], kBlack);
}

TEST(AverageRegionsTest, MixedRegionAveragesEachChannelWithRounding)
{
    auto image = make(2, 1, 0, PixelFormat::Bgrx);
    put(image, 0, 0, {255, 0, 1});
    put(image, 1, 0, {0, 255, 2});

    const auto regions = averageRegions(image.view(), {.columns = 1, .rows = 1});

    ASSERT_TRUE(regions.has_value());
    EXPECT_EQ((*regions)[0], (RgbColor{128, 128, 2})); // 127.5 -> 128; 1.5 -> 2
}

TEST(AverageRegionsTest, WidthNotDivisibleByColumnsCoversEveryPixelOnce)
{
    auto image = make(10, 1, 0, PixelFormat::Rgbx);
    fill(image, 0, 3, 0, 1, kRed);   // região 0: x 0..2 (10*1/3 = 3)
    fill(image, 3, 6, 0, 1, kGreen); // região 1: x 3..5
    fill(image, 6, 10, 0, 1, kBlue); // região 2: x 6..9

    const auto regions = averageRegions(image.view(), {.columns = 3, .rows = 1});

    ASSERT_TRUE(regions.has_value());
    EXPECT_EQ((*regions)[0], kRed);
    EXPECT_EQ((*regions)[1], kGreen);
    EXPECT_EQ((*regions)[2], kBlue);
}

TEST(AverageRegionsTest, SampleStepStillReadsInsideTheRegionAndAgreesOnUniformColor)
{
    auto image = make(40, 8, 8, PixelFormat::Bgrx);
    fill(image, 0, 20, 0, 8, kRed);
    fill(image, 20, 40, 0, 8, kGreen);

    const auto regions = averageRegions(image.view(), {.columns = 2, .rows = 1}, 7);

    ASSERT_TRUE(regions.has_value());
    EXPECT_EQ((*regions)[0], kRed);
    EXPECT_EQ((*regions)[1], kGreen);
}

TEST(AverageRegionsTest, InvalidInputsAreRejectedWithoutReadingOutOfBounds)
{
    const auto image = make(8, 4, 0, PixelFormat::Bgrx);

    EXPECT_EQ(averageRegions({image.bytes, 0, 4, 32, PixelFormat::Bgrx}, {1, 1}).error().code,
              ErrorCode::InvalidArgument);
    EXPECT_EQ(averageRegions({image.bytes, 8, 0, 32, PixelFormat::Bgrx}, {1, 1}).error().code,
              ErrorCode::InvalidArgument);
    EXPECT_EQ(averageRegions(image.view(), {0, 1}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(averageRegions(image.view(), {1, 0}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(averageRegions(image.view(), {9, 1}).error().code, ErrorCode::InvalidArgument); // mais colunas que pixels
    EXPECT_EQ(averageRegions({image.bytes, 8, 4, 31, PixelFormat::Bgrx}, {1, 1}).error().code,
              ErrorCode::InvalidArgument); // stride menor que a linha
    const std::vector<std::uint8_t> shortBuffer(8 * 4 * 4 - 1, 0);
    EXPECT_EQ(averageRegions({shortBuffer, 8, 4, 32, PixelFormat::Bgrx}, {1, 1}).error().code,
              ErrorCode::InvalidArgument); // buffer menor que o quadro
}

TEST(AverageRegionsTest, LastRowNeedsNoTrailingPadding)
{
    auto image = make(2, 2, 8, PixelFormat::Bgrx);
    fill(image, 0, 2, 0, 2, kGreen);
    image.bytes.resize(static_cast<std::size_t>(image.stride) + (2 * 4)); // sem o preenchimento da última linha

    const auto regions = averageRegions(image.view(), {.columns = 1, .rows = 1});

    ASSERT_TRUE(regions.has_value());
    EXPECT_EQ((*regions)[0], kGreen);
}

TEST(ScreenRegionsFrameTest, EachKeyTakesTheColorOfItsBandAndEveryKeyIsPainted)
{
    std::vector<RgbColor> regions;
    for (std::uint8_t i = 0; i < 10; ++i) {
        regions.emplace_back(i, static_cast<std::uint8_t>(i * 10), 255);
    }

    const auto frame = screenRegionsFrame(regions);

    const auto total = G915KeyCatalog::keys().size();
    EXPECT_EQ(frame.size(), total);
    for (const auto& key : G915KeyCatalog::keys()) {
        EXPECT_EQ(frame.colorOf(key.id), regions[key.id.position() * 10 / total]);
    }
}

TEST(ScreenRegionsFrameTest, WrongRegionCountGivesABlackKeyboard)
{
    const std::vector<RgbColor> regions(9, kRed);

    const auto frame = screenRegionsFrame(regions);

    EXPECT_EQ(frame.size(), G915KeyCatalog::keys().size());
    for (const auto& [key, color] : frame) {
        EXPECT_EQ(color, kBlack);
    }
}

} // namespace
