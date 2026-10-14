#include "modules/lighting/infrastructure/HidppCapabilities.hpp"
#include "HidppFixture.hpp"

using namespace ghub::lighting::testing; // NOLINT(google-build-using-namespace)
using ghub::lighting::infrastructure::HidppCapabilities;
using ghub::lighting::infrastructure::HidppErrorKind;
using ghub::lighting::infrastructure::HidppFeatureEntry;

class HidppCapabilitiesTest : public HidppFixture {};

TEST_F(HidppCapabilitiesTest, EnumeratesFeaturesThroughFeatureSet)
{
    HidppCapabilities capabilities{*channel};

    const auto result = capabilities.enumerateFeatures(device);

    ASSERT_TRUE(result.has_value());
    const std::vector<HidppFeatureEntry> expected{
        {1, 0x1004, 0x60}, {2, 0x8081, 0x60}, {3, 0x0001, 0x60}, {4, 0x8071, 0x60}, {5, 0x8070, 0x60}};
    EXPECT_EQ(*result, expected);
}

TEST_F(HidppCapabilitiesTest, EnumerationFailsWithoutFeatureSet)
{
    features = {0x1004, 0x8071};
    HidppCapabilities capabilities{*channel};

    const auto result = capabilities.enumerateFeatures(device);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::FeatureNotPresent);
}

TEST_F(HidppCapabilitiesTest, RawInfoSendsOnlyGetInfoOfThePresentFeature)
{
    HidppCapabilities capabilities{*channel};

    const auto result = capabilities.rawInfo(device, 0x8071);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());
    ASSERT_EQ(io->written().size(), 2U);
    EXPECT_EQ(io->written()[1], longReport(device, indexOf(0x8071), 0x0B));
}

TEST_F(HidppCapabilitiesTest, RawInfoOfAbsentFeatureIsEmpty)
{
    features = {0x0001, 0x8071};
    HidppCapabilities capabilities{*channel};

    const auto result = capabilities.rawInfo(device, 0x8081);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
    EXPECT_EQ(io->written().size(), 1U);
}

TEST_F(HidppCapabilitiesTest, RawInfoRefusesFeaturesOutsideTheLightingSet)
{
    HidppCapabilities capabilities{*channel};

    const auto result = capabilities.rawInfo(device, 0x0001);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::InvalidArgument);
    EXPECT_TRUE(io->written().empty());
}
