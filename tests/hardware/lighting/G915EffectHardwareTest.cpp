// Teste físico opt-in (INC-008 parte D, ADR-0013): grava um efeito de hardware no G915 pela 0x8071 fn1.
// AVISO: ESCREVE NA MEMÓRIA DO TECLADO (PERSISTENTE). O efeito anterior não é legível. Exige autorização explícita
// do usuário na sessão (protocol-safety-auditor) e GHUB_HW_TESTS=1 com GHUB_HW_LIGHTING_G915_EFFECT=wave|static-red.
// Sequência enviada: 0x8071 fn0 (GetInfo), fn5 [01 03 05] (toma o controle), fn1 [cluster 1, índice, p0..p9, 01]
// (grava), fn5 [01 00 00] (devolve). Só a zona das teclas; o logo fica de fora.
// Resultado (D1/D2, 2026-10-14): nada visível muda; o efeito do firmware vem do perfil onboard (0x8100), não desta
// gravação. O efeito salvo nesta unidade é Ripple (evidence/2026-10-14-g915-perfis-onboard-0x8100-inc008.md).
#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/lighting/application/SaveKeyboardEffect.hpp"
#include "modules/lighting/infrastructure/G915HidppLighting.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

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

} // namespace

TEST(LightingG915EffectHardware, SavesTheChosenEffectOnTheKeys)
{
    const auto choice = envValue("GHUB_HW_LIGHTING_G915_EFFECT");
    if (envValue("GHUB_HW_TESTS") != "1" || (choice != "wave" && choice != "static-red")) {
        GTEST_SKIP() << "teste físico com ESCRITA PERSISTENTE no G915: defina GHUB_HW_TESTS=1 e "
                        "GHUB_HW_LIGHTING_G915_EFFECT=wave|static-red";
    }
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    const auto effect = choice == "wave"
                            ? KeyboardEffect::wave(*EffectPeriod::create(5000), *EffectBrightness::create(100))
                            : KeyboardEffect::staticColor(RgbColor{0xFF, 0x00, 0x00});
    // NOLINTEND(bugprone-unchecked-optional-access)

    ghub::device_access::SteadyClock clock;
    ghub::device_access::AccessRegistry registry{clock};
    ghub::device_access::ProcExternalProcessProbe probe;
    ghub::lighting::infrastructure::G915HidppLighting keyboard{ghub::platform::hidpp::HidppNodeLocator{}, clock};
    const ghub::lighting::application::SaveKeyboardEffect useCase{
        keyboard, registry, probe,
        ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:407c").value()}; // NOLINT

    std::cout << "[passo] gravando o efeito '" << choice << "' nas TECLAS (0x8071 fn1, persistente)" << std::endl;
    const auto saved = useCase.save(KeyboardZone::Keys, effect);
    ASSERT_TRUE(saved.has_value()) << "gravação: " << saved.error().detail;
    std::cout << "[passo] controle devolvido (0x8071 fn5 01 00 00). Observe o teclado: "
              << (choice == "wave" ? "onda de cores horizontal, período 5 s, brilho 100%"
                                   : "todas as teclas em vermelho fixo")
              << ". Desligue e religue o G915 (chave lateral) e confirme que o efeito permanece." << std::endl;
}
