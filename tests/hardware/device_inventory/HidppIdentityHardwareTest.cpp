// Teste físico opt-in (INC-015 parte 1, ADR-0024): LEITURA da identidade HID++ do G915, G703 e G733 pelo adaptador de
// produção. Só envia o que a InventoryIdentityPolicy permite (Root fn0, 0x0001 fn0/1, 0x0003 fn0/1, 0x0005 fn0/1/2).
// Executar com GHUB_HW_TESTS=1 e nenhuma instância do app aberta; comparar a saída com a captura C-00.
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/device_inventory/domain/DeviceType.hpp"
#include "modules/device_inventory/infrastructure/HidppIdentitySource.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

bool enabled()
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv("GHUB_HW_TESTS");
    return value != nullptr && std::string_view{value} == "1";
}

void readAndPrint(ghub::device_inventory::IdentityTarget target, std::string_view label)
{
    if (!enabled()) {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1";
    }
    ghub::device_access::SteadyClock clock;
    ghub::device_inventory::HidppIdentitySource source{ghub::platform::hidpp::HidppNodeLocator{}, clock};
    const auto identity = source.read(target);
    ASSERT_TRUE(identity.has_value()) << label << ": " << identity.error().detail;
    std::cout << "[" << label << "] nome: " << identity->name().value_or("(não informado)") << '\n';
    std::cout << "[" << label << "] tipo: "
              << (identity->deviceType() ? ghub::device_inventory::deviceTypeText(*identity->deviceType())
                                         : std::string{"(não informado)"})
              << '\n';
    for (const auto& firmware : identity->firmwares()) {
        std::cout << "[" << label << "] firmware: " << firmware.describe() << '\n';
    }
    std::cout << "[" << label << "] features (" << identity->featureIds().size() << "):";
    for (const auto id : identity->featureIds()) {
        std::array<char, 8> text{};
        std::snprintf(text.data(), text.size(), " %04x", id);
        std::cout << text.data();
    }
    std::cout << '\n';
    EXPECT_TRUE(identity->name().has_value());
    EXPECT_FALSE(identity->firmwares().empty());
    EXPECT_FALSE(identity->featureIds().empty());
}

} // namespace

TEST(HidppIdentityHardware, ReadsG915) { readAndPrint(ghub::device_inventory::IdentityTarget::G915, "G915"); }
TEST(HidppIdentityHardware, ReadsG703) { readAndPrint(ghub::device_inventory::IdentityTarget::G703, "G703"); }
TEST(HidppIdentityHardware, ReadsG733) { readAndPrint(ghub::device_inventory::IdentityTarget::G733, "G733"); }
