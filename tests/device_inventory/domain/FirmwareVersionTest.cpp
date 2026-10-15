#include "modules/device_inventory/domain/FirmwareVersion.hpp"

#include <gtest/gtest.h>

namespace ghub::device_inventory {
namespace {

// Bytes da captura C-00 (INC-015): 0x0003 fn1, unit ID redigido e fora do tipo.
constexpr std::array<std::uint8_t, 3> kMpk{'M', 'P', 'K'};
constexpr std::array<std::uint8_t, 3> kMpm{'M', 'P', 'M'};
constexpr std::array<std::uint8_t, 3> kU2{'U', '2', ' '};

TEST(FirmwareVersion, FormatsG915ApplicationFromC00)
{
    const FirmwareVersion firmware{0, kMpk, 0x09, 0x04, 0x0042, true};

    EXPECT_EQ(firmware.versionText(), "MPK 09.04 b0042");
    EXPECT_EQ(firmware.kindText(), "aplicação");
    EXPECT_TRUE(firmware.active());
    EXPECT_EQ(firmware.describe(), "MPK 09.04 b0042 — aplicação (ativo)");
}

TEST(FirmwareVersion, FormatsG703ApplicationFromC00)
{
    const FirmwareVersion firmware{0, kMpm, 0x22, 0x02, 0x0015, true};

    EXPECT_EQ(firmware.versionText(), "MPM 22.02 b0015");
}

TEST(FirmwareVersion, DropsTrailingSpacesOfThePrefix)
{
    const FirmwareVersion firmware{0, kU2, 0x00, 0x06, 0x0000, true};

    EXPECT_EQ(firmware.versionText(), "U2 00.06 b0000");
}

TEST(FirmwareVersion, InactiveEntryHasNoActiveMarker)
{
    const FirmwareVersion firmware{1, kMpk, 0x01, 0x02, 0x0003, false};

    EXPECT_FALSE(firmware.active());
    EXPECT_EQ(firmware.describe(), "MPK 01.02 b0003 — bootloader");
}

TEST(FirmwareVersion, NamesTheKnownKindsAndNumbersTheOthers)
{
    EXPECT_EQ((FirmwareVersion{0, kMpk, 0, 0, 0, false}.kindText()), "aplicação");
    EXPECT_EQ((FirmwareVersion{1, kMpk, 0, 0, 0, false}.kindText()), "bootloader");
    EXPECT_EQ((FirmwareVersion{2, kMpk, 0, 0, 0, false}.kindText()), "hardware");
    EXPECT_EQ((FirmwareVersion{3, kMpk, 0, 0, 0, false}.kindText()), "outro (3)");
    EXPECT_EQ((FirmwareVersion{255, kMpk, 0, 0, 0, false}.kindText()), "outro (255)");
}

TEST(FirmwareVersion, ShowsNonBcdNibblesInHexadecimalWithoutFailing)
{
    const FirmwareVersion firmware{0, kMpk, 0x0A, 0xB3, 0xC0DE, false};

    EXPECT_EQ(firmware.versionText(), "MPK 0A.B3 bC0DE");
}

TEST(FirmwareVersion, ReplacesNonPrintablePrefixBytes)
{
    const FirmwareVersion firmware{0, {'A', 0x01, 0xFF}, 0x01, 0x02, 0x0003, false};

    EXPECT_EQ(firmware.versionText(), "A?? 01.02 b0003");
}

TEST(FirmwareVersion, OmitsTheSeparatorWhenThePrefixIsBlank)
{
    const FirmwareVersion firmware{0, {' ', ' ', ' '}, 0x01, 0x02, 0x0003, false};

    EXPECT_EQ(firmware.versionText(), "01.02 b0003");
}

} // namespace
} // namespace ghub::device_inventory
