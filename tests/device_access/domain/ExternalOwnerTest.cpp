#include "modules/device_access/domain/ExternalOwner.hpp"

#include <gtest/gtest.h>

namespace ghub::device_access {
namespace {

TEST(ExternalOwner, Identifies_KnownProcessNames)
{
    EXPECT_EQ(externalOwnerForProcessName("openrgb"), ExternalOwner::OpenRgb);
    EXPECT_EQ(externalOwnerForProcessName("solaar"), ExternalOwner::Solaar);
    EXPECT_EQ(externalOwnerForProcessName("piper"), ExternalOwner::Piper);
    EXPECT_EQ(externalOwnerForProcessName("ratbagd"), ExternalOwner::Ratbagd);
    EXPECT_EQ(externalOwnerForProcessName("headsetcontrol"), ExternalOwner::HeadsetControl);
}

TEST(ExternalOwner, Ignores_OtherOrPartialNames)
{
    EXPECT_FALSE(externalOwnerForProcessName("openrgb-helper"));
    EXPECT_FALSE(externalOwnerForProcessName("OpenRGB"));
    EXPECT_FALSE(externalOwnerForProcessName("openrg"));
    EXPECT_FALSE(externalOwnerForProcessName(""));
    EXPECT_FALSE(externalOwnerForProcessName("bash"));
}

TEST(ExternalOwner, ProcessName_RoundTrips)
{
    for (const auto owner : {
             ExternalOwner::OpenRgb,
             ExternalOwner::Solaar,
             ExternalOwner::Piper,
             ExternalOwner::Ratbagd,
             ExternalOwner::HeadsetControl,
         }) {
        EXPECT_FALSE(processName(owner).empty());
        EXPECT_EQ(externalOwnerForProcessName(processName(owner)), owner);
    }
}

} // namespace
} // namespace ghub::device_access
