// Teste físico opt-in (INC-008 parte C, ADR-0013): sessão AO VIVO no G915. ESCREVE no teclado (volátil):
// 0x8071 fn5 [01 03 05] (toma o controle), 0x8081 fn6/fn1/fn7 (pinta), 0x8071 fn5 [01 00 00] (devolve).
// Nada é gravado no teclado. Exige autorização do usuário na sessão e GHUB_HW_TESTS=1 GHUB_HW_LIGHTING_G915_LIVE=1.
// As pausas servem só para o usuário observar o teclado; não sincronizam nada.
// Recuperação se o teclado ficar com as cores do teste: desligar e religar o G915 (chave lateral).
#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"
#include "modules/lighting/application/LiveKeyboardSession.hpp"
#include "modules/lighting/domain/G915KeyCatalog.hpp"
#include "modules/lighting/infrastructure/G915HidppLighting.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::KeyColorMap;
using ghub::lighting::domain::RgbColor;
using namespace std::chrono_literals;

bool enabled(const char* name)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
}

void observe(std::string_view what, std::chrono::seconds hold)
{
    std::cout << "[observe] " << what << " — " << hold.count() << " s" << std::endl;
    std::this_thread::sleep_for(hold);
}

} // namespace

TEST(LightingG915LiveHardware, PaintsKeysAndGivesControlBackToTheFirmware)
{
    if (!enabled("GHUB_HW_TESTS") || !enabled("GHUB_HW_LIGHTING_G915_LIVE")) {
        GTEST_SKIP()
            << "teste físico com ESCRITA volátil no G915: defina GHUB_HW_TESTS=1 e GHUB_HW_LIGHTING_G915_LIVE=1";
    }
    ghub::device_access::SteadyClock clock;
    ghub::device_access::AccessRegistry registry{clock};
    ghub::device_access::ProcExternalProcessProbe probe;
    ghub::lighting::infrastructure::G915HidppLighting keyboard{ghub::platform::hidpp::HidppNodeLocator{}, clock};
    const ghub::lighting::application::StartLiveKeyboardSession useCase{
        keyboard, registry, probe,
        ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:407c").value()}; // NOLINT

    auto session = useCase.start();
    ASSERT_TRUE(session.has_value()) << "início: " << session.error().detail;
    std::cout << "[passo] controle tomado (0x8071 fn5 01 03 05)" << std::endl;

    KeyColorMap allRed;
    for (const auto& key : G915KeyCatalog::keys()) {
        allRed = allRed.with(key.id, RgbColor{0xFF, 0x00, 0x00});
    }
    const auto red = (*session)->paint(allRed);
    ASSERT_TRUE(red.has_value()) << "pintura vermelha: " << red.error().detail;
    observe("todas as teclas e o logo devem estar VERMELHOS", 10s);

    KeyColorMap wasd;
    for (const auto* name : {"Key: W", "Key: A", "Key: S", "Key: D"}) {
        wasd = wasd.with(*G915KeyCatalog::findByName(name), RgbColor{0x00, 0xFF, 0x00}); // NOLINT
    }
    const auto green = (*session)->paint(wasd);
    ASSERT_TRUE(green.has_value()) << "pintura WASD: " << green.error().detail;
    observe("W, A, S, D devem estar VERDES e o resto vermelho", 5s);

    const auto ended = (*session)->end();
    ASSERT_TRUE(ended.has_value()) << "devolução: " << ended.error().detail;
    std::cout << "[passo] controle devolvido (0x8071 fn5 01 00 00): o efeito anterior do teclado deve voltar"
              << std::endl;
}
