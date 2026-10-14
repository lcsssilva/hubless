#pragma once

#include "modules/audio_processing/api/EqualizerCurveAccess.hpp"
#include "modules/profiles/application/EqualizerSettingsPort.hpp"

namespace ghub::profiles {

// Liga o port de EQ do módulo profiles à API pública do módulo audio_processing (ADR-0020).
// Só na thread dona do EQ. O acesso precisa sobreviver a este objeto.
class EqualizerSettingsAdapter final : public EqualizerSettingsPort {
public:
    explicit EqualizerSettingsAdapter(audio_processing::EqualizerCurveAccess& access) noexcept : access_(&access) {}

    [[nodiscard]] std::array<int, 10> currentGains() const override { return access_->currentGains(); }
    [[nodiscard]] shared_kernel::Result<void> requestGains(const std::array<int, 10>& gains) override
    {
        return access_->requestGains(gains);
    }

private:
    audio_processing::EqualizerCurveAccess* access_; // não nulo
};

} // namespace ghub::profiles
