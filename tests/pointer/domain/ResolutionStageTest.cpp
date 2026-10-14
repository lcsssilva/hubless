#include "modules/pointer/domain/ResolutionStage.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using ghub::pointer::ResolutionStage;
using ghub::pointer::ResolutionStageSpec;
using ghub::shared_kernel::ErrorCode;

namespace {

// Lista de DPIs do G703 (evidence/2026-10-13-ratbagd-leitura.md): 88 valores.
std::vector<std::uint32_t> g703DpiList()
{
    std::vector<std::uint32_t> list;
    for (std::uint32_t dpi = 100; dpi <= 1000; dpi += 50) {
        list.push_back(dpi);
    }
    for (std::uint32_t dpi = 1100; dpi <= 2600; dpi += 100) {
        list.push_back(dpi);
    }
    for (std::uint32_t dpi = 2800; dpi <= 5000; dpi += 200) {
        list.push_back(dpi);
    }
    for (std::uint32_t dpi = 5500; dpi <= 25500; dpi += 500) {
        list.push_back(dpi);
    }
    return list;
}

ResolutionStageSpec activeStage()
{
    return {
        .index = 0,
        .dpiX = 7050,
        .dpiY = std::nullopt,
        .active = true,
        .isDefault = true,
        .disabled = false,
        .supportedDpis = g703DpiList(),
    };
}

} // namespace

TEST(ResolutionStageTest, G703TableHas88Values)
{
    EXPECT_EQ(g703DpiList().size(), 88U);
}

TEST(ResolutionStageTest, CreatesTheRealActiveStage)
{
    const auto stage = ResolutionStage::create(activeStage());

    ASSERT_TRUE(stage.has_value()) << stage.error().detail;
    EXPECT_EQ(stage->index(), 0U);
    EXPECT_EQ(stage->dpiX(), 7050U);
    EXPECT_FALSE(stage->dpiY().has_value());
    EXPECT_TRUE(stage->isActive());
    EXPECT_TRUE(stage->isDefault());
    EXPECT_FALSE(stage->isDisabled());
    EXPECT_EQ(stage->supportedDpis().size(), 88U);
}

TEST(ResolutionStageTest, RejectsZeroDpiX)
{
    auto spec = activeStage();
    spec.dpiX = 0;

    const auto stage = ResolutionStage::create(spec);

    ASSERT_FALSE(stage.has_value());
    EXPECT_EQ(stage.error().code, ErrorCode::InvalidArgument);
}

TEST(ResolutionStageTest, RejectsZeroDpiYWhenPresent)
{
    auto spec = activeStage();
    spec.dpiY = 0;

    const auto stage = ResolutionStage::create(spec);

    ASSERT_FALSE(stage.has_value());
    EXPECT_EQ(stage.error().code, ErrorCode::InvalidArgument);
}

TEST(ResolutionStageTest, KeepsSeparateDpiY)
{
    auto spec = activeStage();
    spec.dpiY = 3200;

    const auto stage = ResolutionStage::create(spec);

    ASSERT_TRUE(stage.has_value()) << stage.error().detail;
    EXPECT_EQ(stage->dpiY(), std::optional<std::uint32_t>{3200});
}

TEST(ResolutionStageTest, RejectsZeroInSupportedList)
{
    auto spec = activeStage();
    spec.supportedDpis = {0, 100};

    EXPECT_EQ(ResolutionStage::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(ResolutionStageTest, RejectsRepeatedSupportedDpi)
{
    auto spec = activeStage();
    spec.supportedDpis = {100, 100, 200};

    EXPECT_EQ(ResolutionStage::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(ResolutionStageTest, RejectsDescendingSupportedList)
{
    auto spec = activeStage();
    spec.supportedDpis = {200, 100};

    EXPECT_EQ(ResolutionStage::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(ResolutionStageTest, AcceptsEmptySupportedList)
{
    auto spec = activeStage();
    spec.supportedDpis = {};

    EXPECT_TRUE(ResolutionStage::create(spec).has_value());
}

TEST(ResolutionStageTest, ActiveDpi7050IsNotInTheRealList)
{
    const auto stage = ResolutionStage::create(activeStage());

    ASSERT_TRUE(stage.has_value());
    EXPECT_FALSE(stage->isDpiListed());
}

TEST(ResolutionStageTest, ListedDpiIsReportedAsListed)
{
    auto spec = activeStage();
    spec.dpiX = 7000;
    spec.supportedDpis = {100, 7000};

    EXPECT_TRUE(ResolutionStage::create(spec)->isDpiListed());
}

TEST(ResolutionStageTest, SeparateDpiYMustAlsoBeListed)
{
    auto spec = activeStage();
    spec.dpiX = 100;
    spec.dpiY = 150;
    spec.supportedDpis = {100, 200};

    EXPECT_FALSE(ResolutionStage::create(spec)->isDpiListed());
}

TEST(ResolutionStageTest, DisabledStageIsKeptAsReported)
{
    auto spec = activeStage();
    spec.dpiX = 100;
    spec.active = false;
    spec.isDefault = false;
    spec.disabled = true;

    const auto stage = ResolutionStage::create(spec);

    ASSERT_TRUE(stage.has_value());
    EXPECT_TRUE(stage->isDisabled());
    EXPECT_FALSE(stage->isActive());
    EXPECT_TRUE(stage->isDpiListed());
}
