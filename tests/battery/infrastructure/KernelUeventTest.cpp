#include "modules/battery/infrastructure/KernelUevent.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using ghub::battery::isPowerSupplyUevent;

namespace {

using namespace std::string_literals;
using namespace std::string_view_literals;

// Uevent como montado por kobject_uevent.c + power_supply_uevent (kernel v7.2.9).
constexpr std::string_view hidppChange =
    "change@/devices/pci0000:00/0000:00:08.1/0000:11:00.4/usb5/5-2/5-2.4/5-2.4:1.2/0003:046D:C547.0008/"
    "0003:046D:407C.000A/power_supply/hidpp_battery_0\0"
    "ACTION=change\0"
    "DEVPATH=/devices/pci0000:00/0000:00:08.1/0000:11:00.4/usb5/5-2/5-2.4/5-2.4:1.2/0003:046D:C547.0008/"
    "0003:046D:407C.000A/power_supply/hidpp_battery_0\0"
    "SUBSYSTEM=power_supply\0"
    "POWER_SUPPLY_NAME=hidpp_battery_0\0"
    "POWER_SUPPLY_STATUS=Discharging\0"
    "POWER_SUPPLY_CAPACITY=44\0"
    "SEQNUM=4242\0"sv;

} // namespace

TEST(KernelUevent, AcceptsPowerSupplyChange)
{
    EXPECT_TRUE(isPowerSupplyUevent(hidppChange));
}

TEST(KernelUevent, RejectsOtherSubsystems)
{
    const auto usbAdd = "add@/devices/pci0000:00/usb5/5-2\0ACTION=add\0DEVPATH=/devices/pci0000:00/usb5/5-2\0"
                        "SUBSYSTEM=usb\0SEQNUM=1\0"s;

    EXPECT_FALSE(isPowerSupplyUevent(usbAdd));
}

TEST(KernelUevent, AcceptsOnlyAddRemoveChangeActions)
{
    const auto withAction = [](const std::string& action) {
        return action + "@/devices/x/power_supply/hidpp_battery_0\0ACTION="s + action +
               "\0DEVPATH=/devices/x/power_supply/hidpp_battery_0\0SUBSYSTEM=power_supply\0"s;
    };

    EXPECT_TRUE(isPowerSupplyUevent(withAction("add")));
    EXPECT_TRUE(isPowerSupplyUevent(withAction("remove")));
    EXPECT_TRUE(isPowerSupplyUevent(withAction("change")));
    EXPECT_FALSE(isPowerSupplyUevent(withAction("bind")));
    EXPECT_FALSE(isPowerSupplyUevent(withAction("online")));
}

TEST(KernelUevent, RejectsDatagramWithoutKernelHeader)
{
    // Sem "ACTION@DEVPATH" (ex.: mensagem do udev, que começa com "libudev\0") ou vazio.
    const auto noHeader = "ACTION=change\0SUBSYSTEM=power_supply\0"s;
    const auto libudev = "libudev\0ACTION=change\0SUBSYSTEM=power_supply\0"s;
    const auto headerWithoutAt = "change\0ACTION=change\0SUBSYSTEM=power_supply\0"s;

    EXPECT_FALSE(isPowerSupplyUevent(noHeader));
    EXPECT_FALSE(isPowerSupplyUevent(libudev));
    EXPECT_FALSE(isPowerSupplyUevent(headerWithoutAt));
    EXPECT_FALSE(isPowerSupplyUevent({}));
}

TEST(KernelUevent, MatchesWholeKeys_Only)
{
    // Campos que só começam com/contêm "SUBSYSTEM" não são a chave SUBSYSTEM.
    const auto withField = [](const std::string& lookalike) {
        return "change@/devices/x\0ACTION=change\0DEVPATH=/devices/x\0"s + lookalike + "\0SUBSYSTEM=usb\0"s;
    };

    EXPECT_FALSE(isPowerSupplyUevent(withField("XSUBSYSTEM=power_supply")));
    EXPECT_FALSE(isPowerSupplyUevent(withField("SUBSYSTEM_power_supply")));
}

namespace {

sockaddr_nl netlinkSender(std::uint32_t portId)
{
    sockaddr_nl sender{};
    sender.nl_family = AF_NETLINK;
    sender.nl_pid = portId;
    sender.nl_groups = 1;
    return sender;
}

} // namespace

TEST(KernelUevent, TrustsNetlinkSenderWithPortIdZero)
{
    EXPECT_TRUE(ghub::battery::isFromKernel(netlinkSender(0), sizeof(sockaddr_nl)));
}

TEST(KernelUevent, DistrustsOtherSenders)
{
    auto unixFamily = netlinkSender(0);
    unixFamily.nl_family = AF_UNIX;

    EXPECT_FALSE(ghub::battery::isFromKernel(netlinkSender(1234), sizeof(sockaddr_nl)));
    EXPECT_FALSE(ghub::battery::isFromKernel(unixFamily, sizeof(sockaddr_nl)));
    EXPECT_FALSE(ghub::battery::isFromKernel(netlinkSender(0), 0));
    EXPECT_FALSE(ghub::battery::isFromKernel(netlinkSender(0), sizeof(sockaddr_nl) - 1));
}
