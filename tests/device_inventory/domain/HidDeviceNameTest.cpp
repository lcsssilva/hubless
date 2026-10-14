#include "modules/device_inventory/domain/DeviceCatalog.hpp"

#include <gtest/gtest.h>

namespace ghub::device_inventory {
namespace {

TEST(HidDeviceName, Parses_KernelHidName)
{
    EXPECT_EQ(parseHidDeviceName("0003:046D:407C.000A"), (UsbId{0x046d, 0x407c}));
    EXPECT_EQ(parseHidDeviceName("0003:046d:c090.0001"), (UsbId{0x046d, 0xc090}));
}

TEST(HidDeviceName, Rejects_MalformedNames)
{
    for (const char* name : {
             "",
             "lixo",
             "0003:046D:407C",
             "0003:046D:407C.",
             "003:046D:407C.000A",
             "0003-046D-407C.000A",
             "0003:046D:407C:000A",
             "0003:046G:407C.000A",
             "0003:046D:407C.00G0",
             "0003:046D:407C.000",
             "0003:046D:407C.000AB",
             "0003:+46D:407C.000A",
             "0003:046D: 07C.000A",
             "00003:046D:407C.000A",
             "0003:046D:407C.-00A",
             " 003:046D:407C.000A",
         }) {
        EXPECT_FALSE(parseHidDeviceName(name).has_value()) << name;
    }
}

} // namespace
} // namespace ghub::device_inventory
