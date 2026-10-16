#include "modules/pointer/application/G703OnboardSettings.hpp"

#include <span>
#include <utility>

namespace ghub::pointer {

shared_kernel::Result<G703OnboardSnapshot> G703OnboardSettings::read() const
{
    auto snapshot = editor_.read();
    if (!snapshot.has_value()) {
        return std::unexpected(snapshot.error());
    }
    auto settings = G703ProfileSettings::read(snapshot->sector.bytes());
    if (!settings.has_value()) {
        return std::unexpected(settings.error());
    }
    return G703OnboardSnapshot{
        .sector = snapshot->number,
        .settings = std::move(*settings),
        .backupExists = snapshot->backupExists,
    };
}

shared_kernel::Result<G703ProfileSettings> G703OnboardSettings::write(const G703ProfileSettings& settings) const
{
    const auto written =
        editor_.write([&settings](std::span<const std::uint8_t> original) { return settings.applyTo(original); });
    if (!written.has_value()) {
        return std::unexpected(written.error());
    }
    return G703ProfileSettings::read(written->bytes());
}

shared_kernel::Result<G703ProfileSettings> G703OnboardSettings::restoreBackup() const
{
    // Só grava uma cópia que o domínio entende (taxa, estágios e padrão válidos).
    const auto restored = editor_.restoreBackup([](std::span<const std::uint8_t> bytes) -> shared_kernel::Result<void> {
        const auto readable = G703ProfileSettings::read(bytes);
        if (!readable.has_value()) {
            return std::unexpected(readable.error());
        }
        return {};
    });
    if (!restored.has_value()) {
        return std::unexpected(restored.error());
    }
    return G703ProfileSettings::read(restored->bytes());
}

} // namespace ghub::pointer
