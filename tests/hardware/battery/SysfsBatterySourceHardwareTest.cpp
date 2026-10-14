// Teste físico opt-in (TEST_STRATEGY.md §1): somente leitura de /sys/class/power_supply real.
// Executar com GHUB_HW_TESTS=1 e G915 + G703 ligados; registrar resultado no VERIFICATION_LOG.
#include "modules/battery/application/GetBatteryStatus.hpp"
#include "modules/battery/infrastructure/SysfsBatterySource.hpp"
#include "modules/device_inventory/api/DeviceInventory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>

using ghub::battery::BatteryReading;
using ghub::battery::ChargeState;
using ghub::battery::GetBatteryStatus;
using ghub::battery::SysfsBatterySource;

TEST(BatteryHardware, ReadsG915AndG703FromKernel)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* enabled = std::getenv("GHUB_HW_TESTS");
    if (enabled == nullptr || std::string_view{enabled} != "1") {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1";
    }
    SysfsBatterySource source;

    const auto statuses = GetBatteryStatus{{&source}}.execute();

    // G733 entra na lista (INC-004), mas só vem de HeadsetControl, não do sysfs: aqui valem G915 e G703.
    ASSERT_EQ(statuses.size(), 3U);
    for (const auto& status : std::views::take(statuses, 2)) {
        ASSERT_TRUE(std::holds_alternative<BatteryReading>(status.state))
            << "dispositivo " << ghub::device_inventory::modelName(status.model) << " sem leitura";
        const auto& reading = std::get<BatteryReading>(status.state);
        const auto current = reading.currentLevel();
        // Dispositivo em repouso/sem link: o driver reporta Unknown e mantém capacity antigo (ADR-0004).
        EXPECT_EQ(current.has_value(), reading.chargeState() != ChargeState::Unknown);
        std::cout << "dispositivo " << ghub::device_inventory::modelName(status.model) << ": "
                  << (current.has_value() ? std::to_string(current->percent()) + "%" : std::string{"sem nível"})
                  << " estado " << static_cast<int>(reading.chargeState()) << '\n';
    }
}
