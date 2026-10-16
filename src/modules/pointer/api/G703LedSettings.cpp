#include "modules/pointer/api/G703LedSettings.hpp"

#include <cstdint>
#include <utility>

namespace ghub::pointer {

namespace {

bool channelInRange(int value)
{
    return value >= 0 && value <= 255;
}

} // namespace

shared_kernel::Result<G703LedApplied> G703LedSettings::applyFixed(const G703LedColor& color) const
{
    if (!channelInRange(color.red) || !channelInRange(color.green) || !channelInRange(color.blue)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "cor do LED do G703 fora de 0-255");
    }
    auto current = onboard_->read();
    if (!current) {
        return std::unexpected(current.error());
    }
    const auto led = LedSetting::fixed(static_cast<std::uint8_t>(color.red), static_cast<std::uint8_t>(color.green),
                                       static_cast<std::uint8_t>(color.blue));
    const auto& old = current->settings;
    auto settings = G703ProfileSettings::make(old.reportRateHz(), old.dpiStages(), old.defaultStage(),
                                              old.dpiShiftStage(), led, led);
    if (!settings) {
        return std::unexpected(settings.error());
    }
    const bool unchanged = *settings == old;
    auto written = onboard_->write(*settings); // sem mudança, o caminho seguro não envia nada
    if (!written) {
        return std::unexpected(written.error());
    }
    return G703LedApplied{.wrote = !unchanged};
}

} // namespace ghub::pointer
