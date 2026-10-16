#pragma once

#include "modules/lighting/application/RunSoftwareAnimation.hpp"
#include "modules/lighting/application/ScreenColorSource.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::lighting::application {

// INC-021 R4 (ADR-0030): amostragem da tela para o teclado. Liga a captura apenas entre start() e stop()/destruição e
// fornece a fonte de quadros que pinta o teclado com as médias das 10 regiões (screenRegionsFrame). Construir não abre
// nada. A fonte de cores precisa sobreviver a este objeto e à fonte de quadros devolvida.
class ScreenSampler {
public:
    explicit ScreenSampler(ScreenColorSource& screen) noexcept;
    ~ScreenSampler();
    ScreenSampler(const ScreenSampler&) = delete;
    ScreenSampler& operator=(const ScreenSampler&) = delete;
    ScreenSampler(ScreenSampler&&) = delete;
    ScreenSampler& operator=(ScreenSampler&&) = delete;

    [[nodiscard]] shared_kernel::Result<void> start();
    void stop();

    // Quadro = médias atuais; ignora o tempo. Sem médias (antes do primeiro quadro ou depois de stop()) = teclado
    // preto.
    [[nodiscard]] RunSoftwareAnimation::FrameSource frameSource() const;

private:
    ScreenColorSource* screen_; // não nulo
};

} // namespace ghub::lighting::application
