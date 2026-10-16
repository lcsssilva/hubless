#pragma once

#include "modules/headset/application/HeadsetLightingWriter.hpp"
#include "modules/headset/application/HeadsetSettingsWriter.hpp"
#include "modules/headset/infrastructure/HidppHeadsetStatusSource.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "shared_kernel/Clock.hpp"

namespace ghub::headset::infrastructure {

// Adaptador dos ports de escrita do G733 por HID++ 2.0 próprio (ADR-0026, INC-016 A3/A4, INC-018): sidetone
// (0x8300 fn1, 1 byte) e luzes (0x8070 fn3, uma escrita por zona), dentro de G733StatusPolicy. A cada chamada
// localiza o nó (índice 0xFF), abre o hidraw, escreve e fecha; prazo de 2 s por pedido, sem reenvio. O chamador
// (caso de uso) segura o DeviceLease "hidpp:046d:0afe". `clock` precisa sobreviver ao adaptador.
class HidppHeadsetSettingsWriter final : public HeadsetSettingsWriter, public HeadsetLightingWriter {
public:
    HidppHeadsetSettingsWriter(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                               HeadsetIoOpener opener = headsetHidrawOpener());

    [[nodiscard]] shared_kernel::Result<void> applySidetone(SidetoneLevel level) override;
    // Ligar = ciclo de 5000 ms, brilho 100, nas zonas 0 e 1; desligar = efeito 0 nas duas.
    [[nodiscard]] shared_kernel::Result<void> applyLights(HeadsetLights lights) override;
    [[nodiscard]] shared_kernel::Result<void> applyLighting(HeadsetLighting lighting) override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    HeadsetIoOpener opener_;
};

} // namespace ghub::headset::infrastructure
