#include "modules/pointer/api/G703DpiSettings.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace ghub::pointer {

namespace {

using shared_kernel::ErrorCode;

G703DpiValue toValue(const G703ProfileSettings& settings)
{
    G703DpiValue value{.reportRateHz = static_cast<int>(settings.reportRateHz()),
                       .dpiStages = {},
                       .defaultStage = static_cast<int>(settings.defaultStage()),
                       .dpiShiftStage = std::nullopt};
    for (const std::uint16_t stage : settings.dpiStages()) {
        value.dpiStages.push_back(stage);
    }
    if (settings.dpiShiftStage().has_value()) {
        value.dpiShiftStage = static_cast<int>(*settings.dpiShiftStage());
    }
    return value;
}

} // namespace

shared_kernel::Result<G703DpiValue> G703DpiSettings::readCurrent() const
{
    auto snapshot = onboard_->read();
    if (!snapshot) {
        return std::unexpected(snapshot.error());
    }
    return toValue(snapshot->settings);
}

shared_kernel::Result<G703DpiApplied> G703DpiSettings::apply(const G703DpiValue& wanted) const
{
    // Valores que não cabem nos tipos do domínio são recusados aqui: truncar viraria outro valor.
    if (wanted.reportRateHz < 0 || wanted.defaultStage < 0 ||
        (wanted.dpiShiftStage.has_value() && *wanted.dpiShiftStage < 0)) {
        return shared_kernel::fail(ErrorCode::InvalidArgument, "valor negativo na seção de DPI");
    }
    std::vector<std::uint16_t> stages;
    for (const int stage : wanted.dpiStages) {
        if (stage < 0 || stage > std::numeric_limits<std::uint16_t>::max()) {
            return shared_kernel::fail(ErrorCode::InvalidArgument, "DPI fora da faixa");
        }
        stages.push_back(static_cast<std::uint16_t>(stage));
    }
    // Os LEDs (e o shift, se o perfil não tem um) vêm do que o mouse tem agora: o perfil só traz DPI e taxa.
    auto current = onboard_->read();
    if (!current) {
        return std::unexpected(current.error());
    }
    const std::optional<std::uint32_t> shift =
        wanted.dpiShiftStage.has_value() ? std::optional<std::uint32_t>{static_cast<std::uint32_t>(*wanted.dpiShiftStage)}
                                         : current->settings.dpiShiftStage();
    auto settings = G703ProfileSettings::make(static_cast<std::uint32_t>(wanted.reportRateHz), std::move(stages),
                                              static_cast<std::uint32_t>(wanted.defaultStage), shift,
                                              current->settings.primaryLed(), current->settings.logoLed());
    if (!settings) {
        return std::unexpected(settings.error());
    }
    const bool unchanged = *settings == current->settings;
    auto written = onboard_->write(*settings); // sem mudança, o caminho seguro não envia nada
    if (!written) {
        return std::unexpected(written.error());
    }
    return G703DpiApplied{.value = toValue(*written), .wrote = !unchanged};
}

} // namespace ghub::pointer
