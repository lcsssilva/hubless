#include "platform/hidpp/HidppCapabilities.hpp"
#include "HidppFixture.hpp"

using namespace ghub::lighting::testing; // NOLINT(google-build-using-namespace)
using hidpp::HidppCapabilities;
using hidpp::HidppErrorKind;
using hidpp::HidppFeatureEntry;

class HidppCapabilitiesTest : public HidppFixture {};

TEST_F(HidppCapabilitiesTest, EnumeratesFeaturesThroughFeatureSet)
{
    hidpp::HidppCapabilities capabilities{*channel};

    const auto result = capabilities.enumerateFeatures(device);

    ASSERT_TRUE(result.has_value());
    const std::vector<hidpp::HidppFeatureEntry> expected{
        {1, 0x1004, 0x60}, {2, 0x8081, 0x60}, {3, 0x0001, 0x60}, {4, 0x8071, 0x60}, {5, 0x8070, 0x60}};
    EXPECT_EQ(*result, expected);
}

TEST_F(HidppCapabilitiesTest, EnumerationFailsWithoutFeatureSet)
{
    features = {0x1004, 0x8071};
    hidpp::HidppCapabilities capabilities{*channel};

    const auto result = capabilities.enumerateFeatures(device);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, hidpp::HidppErrorKind::FeatureNotPresent);
}

TEST_F(HidppCapabilitiesTest, RawInfoSendsOnlyGetInfoOfThePresentFeature)
{
    hidpp::HidppCapabilities capabilities{*channel};

    const auto result = capabilities.rawInfo(device, 0x8071);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());
    ASSERT_EQ(io->written().size(), 2U);
    EXPECT_EQ(io->written()[1], longReport(device, indexOf(0x8071), 0x0B));
}

TEST_F(HidppCapabilitiesTest, RawInfoOfAbsentFeatureIsEmpty)
{
    features = {0x0001, 0x8071};
    hidpp::HidppCapabilities capabilities{*channel};

    const auto result = capabilities.rawInfo(device, 0x8081);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
    EXPECT_EQ(io->written().size(), 1U);
}

TEST_F(HidppCapabilitiesTest, RawInfoRefusesFeaturesOutsideTheLightingSet)
{
    hidpp::HidppCapabilities capabilities{*channel};

    const auto result = capabilities.rawInfo(device, 0x0001);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, hidpp::HidppErrorKind::InvalidArgument);
    EXPECT_TRUE(io->written().empty());
}
