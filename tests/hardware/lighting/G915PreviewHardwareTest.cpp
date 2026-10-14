// Teste físico opt-in (INC-008 R14, ADR-0013): prévia de efeito de hardware no G915 enquanto o app segura o controle.
// AVISO: ESCREVE no teclado. O fn1 vai com persistência 01, como na parte D (o D1/D2 não mudou o efeito do
// firmware). Exige autorização explícita do usuário na sessão e GHUB_HW_TESTS=1 com
// GHUB_HW_LIGHTING_G915_PREVIEW=wave|breathing-red|static-red.
// GHUB_HW_LIGHTING_HOLD_S (1..60, padrão 10) define por quantos segundos o efeito fica à vista.
// Sequência enviada: 0x8071 fn5 [01 03 05] (toma o controle), fn0 (GetInfo), fn1 [01 idx p0..p9 01] (efeito),
// pausa de observação, fn5 [01 00 00] (devolve). Só a zona das teclas; o logo fica de fora.
// Recuperação: desligar e religar o G915 (chave lateral) devolve o controle ao firmware.
#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/lighting/application/LiveKeyboardSession.hpp"
#include "modules/lighting/infrastructure/G915HidppLighting.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

using ghub::lighting::domain::EffectBrightness;
using ghub::lighting::domain::EffectPeriod;
using ghub::lighting::domain::KeyboardEffect;
using ghub::lighting::domain::KeyboardZone;
using ghub::lighting::domain::RgbColor;

std::string envValue(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
}

int holdSeconds()
{
    const auto text = envValue("GHUB_HW_LIGHTING_HOLD_S");
    const int value = text.empty() ? 10 : std::atoi(text.c_str());
    return value < 1 || value > 60 ? 10 : value;
}

} // namespace

TEST(LightingG915PreviewHardware, ShowsTheEffectWhileLiveAndRestoresOnEnd)
{
    const auto choice = envValue("GHUB_HW_LIGHTING_G915_PREVIEW");
    if (envValue("GHUB_HW_TESTS") != "1" || (choice != "wave" && choice != "breathing-red" && choice != "static-red")) {
        GTEST_SKIP() << "teste físico com ESCRITA volátil no G915: defina GHUB_HW_TESTS=1 e "
                        "GHUB_HW_LIGHTING_G915_PREVIEW=wave|breathing-red|static-red";
    }
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    const auto period = *EffectPeriod::create(5000);
    const auto brightness = *EffectBrightness::create(100);
    const RgbColor red{0xFF, 0x00, 0x00};
    const auto effect = choice == "wave"            ? KeyboardEffect::wave(period, brightness)
                        : choice == "breathing-red" ? KeyboardEffect::breathing(red, period, brightness)
                                                    : KeyboardEffect::staticColor(red);
    // NOLINTEND(bugprone-unchecked-optional-access)

    ghub::device_access::SteadyClock clock;
    ghub::device_access::AccessRegistry registry{clock};
    ghub::device_access::ProcExternalProcessProbe probe;
    ghub::lighting::infrastructure::G915HidppLighting keyboard{ghub::lighting::infrastructure::HidppNodeLocator{},
                                                               clock};
    const ghub::lighting::application::StartLiveKeyboardSession useCase{
        keyboard, registry, probe,
        ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:407c").value()}; // NOLINT

    auto session = useCase.start();
    ASSERT_TRUE(session.has_value()) << "início: " << session.error().detail;
    std::cout << "[passo] controle tomado (0x8071 fn5 01 03 05)" << std::endl;

    const auto applied = (*session)->applyEffect(KeyboardZone::Keys, effect);
    ASSERT_TRUE(applied.has_value()) << "efeito: " << applied.error().detail;
    const int hold = holdSeconds();
    std::cout << "[passo] efeito '" << choice << "' enviado às TECLAS (0x8071 fn1). Observe o teclado por " << hold
              << " s" << std::endl;
    // Pausa de observação humana, não sincronização: nada no teste depende dela.
    std::this_thread::sleep_for(std::chrono::seconds{hold});

    const auto ended = (*session)->end();
    ASSERT_TRUE(ended.has_value()) << "devolução: " << ended.error().detail;
    std::cout << "[passo] controle devolvido (0x8071 fn5 01 00 00): o efeito salvo no perfil onboard deve voltar"
              << std::endl;
}
