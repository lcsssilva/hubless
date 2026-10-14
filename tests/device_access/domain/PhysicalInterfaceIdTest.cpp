#include "modules/device_access/domain/PhysicalInterfaceId.hpp"
#include "modules/device_access/domain/LeaseRequest.hpp"

#include <gtest/gtest.h>

using ghub::device_access::LeaseHolder;
using ghub::device_access::PhysicalInterfaceId;
using ghub::shared_kernel::ErrorCode;

TEST(PhysicalInterfaceId, KeepsValue_WhenNotEmpty)
{
    const auto id = PhysicalInterfaceId::create("usb:5-2.4:1.2");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->value(), "usb:5-2.4:1.2");
}

TEST(PhysicalInterfaceId, IsRejected_WhenEmpty)
{
    const auto id = PhysicalInterfaceId::create("");
    ASSERT_FALSE(id.has_value());
    EXPECT_EQ(id.error().code, ErrorCode::InvalidArgument);
}

TEST(PhysicalInterfaceId, ComparesByValue)
{
    EXPECT_EQ(PhysicalInterfaceId::create("a").value(), PhysicalInterfaceId::create("a").value());
    EXPECT_NE(PhysicalInterfaceId::create("a").value(), PhysicalInterfaceId::create("b").value());
}

TEST(LeaseHolder, IsRejected_WhenEmpty)
{
    const auto holder = LeaseHolder::create("");
    ASSERT_FALSE(holder.has_value());
    EXPECT_EQ(holder.error().code, ErrorCode::InvalidArgument);
}

TEST(LeaseHolder, KeepsName_WhenNotEmpty)
{
    EXPECT_EQ(LeaseHolder::create("pointer.ratbag").value().name(), "pointer.ratbag");
}
