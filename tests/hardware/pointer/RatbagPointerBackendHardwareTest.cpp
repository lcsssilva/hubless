// Teste físico opt-in (TEST_STRATEGY.md §1, ADR-0011 §7): LEITURA do G703 real pelo ratbagd no barramento de
// SISTEMA. Ativa o ratbagd (root), que pode enviar `set_onboard_mode` ao mouse (autorizado pelo usuário em
// 2026-10-13, ADR-0011). Executar com GHUB_HW_TESTS=1 e GHUB_HW_RATBAG=1; registrar no VERIFICATION_LOG.
#include "modules/pointer/infrastructure/RatbagPointerBackend.hpp"

#include <QCoreApplication>
#include <QDBusConnection>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using ghub::pointer::DiscoveredPointer;
using ghub::pointer::RatbagPointerBackend;

namespace {

bool enabled(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
}

} // namespace

TEST(PointerHardware, ReadsG703FromTheSystemRatbagd)
{
    if (!enabled("GHUB_HW_TESTS") || !enabled("GHUB_HW_RATBAG")) {
        GTEST_SKIP() << "teste físico: defina GHUB_HW_TESTS=1 e GHUB_HW_RATBAG=1 (ativa o ratbagd, root)";
    }
    RatbagPointerBackend backend{QDBusConnection::systemBus(), QStringLiteral("org.freedesktop.ratbag1"),
                                 std::chrono::seconds{30}};

    const auto devices = backend.readDevices();

    ASSERT_TRUE(devices.has_value()) << devices.error().detail;
    const auto g703 = std::ranges::find_if(*devices, [](const DiscoveredPointer& device) {
        return device.usbId.has_value() && device.usbId->vendorId == 0x046D && device.usbId->productId == 0x4086;
    });
    for (const auto& device : *devices) {
        std::cout << "dispositivo '" << device.name << "' usb=" << (device.usbId.has_value() ? "sim" : "não")
                  << " configuração=" << (device.configuration.has_value() ? "ok" : device.configuration.error().detail)
                  << "\n";
    }
    ASSERT_NE(g703, devices->end()) << "G703 (046d:4086) não listado pelo ratbagd";
    ASSERT_TRUE(g703->configuration.has_value()) << g703->configuration.error().detail;
    const auto& configuration = *g703->configuration;
    std::cout << "G703 '" << configuration.name() << "' perfis=" << configuration.profiles().size() << "\n";
    EXPECT_EQ(configuration.profiles().size(), 5U);

    bool sawDataProfile = false;
    for (const auto& profile : configuration.profiles()) {
        std::cout << "perfil " << profile.index() << " ativo=" << profile.isActive()
                  << " habilitado=" << profile.isEnabled() << " taxa=" << profile.reportRate()
                  << " estágios=" << profile.stages().size() << " botões=" << profile.buttons().size()
                  << " leds=" << profile.leds().size() << "\n";
        for (const auto& stage : profile.stages()) {
            std::cout << "  estágio " << stage.index() << " dpi=" << stage.dpiX() << " ativo=" << stage.isActive()
                      << " padrão=" << stage.isDefault() << " desabilitado=" << stage.isDisabled()
                      << " listado=" << stage.isDpiListed() << "\n";
        }
        if (profile.buttons().empty() && profile.leds().empty() && profile.stages().empty()) {
            continue; // perfil sem dados (desabilitado)
        }
        sawDataProfile = true;
        EXPECT_EQ(profile.buttons().size(), 6U);
        EXPECT_EQ(profile.leds().size(), 2U);
        EXPECT_EQ(profile.supportedReportRates(), (std::vector<std::uint32_t>{125, 250, 500, 1000}));
    }
    EXPECT_TRUE(sawDataProfile);
    for (const auto& warning : configuration.warnings()) {
        std::cout << "aviso " << static_cast<int>(warning.kind) << "\n";
    }
}

int main(int argc, char** argv)
{
    const QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
