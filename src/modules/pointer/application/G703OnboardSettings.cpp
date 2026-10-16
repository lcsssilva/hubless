#include "modules/pointer/application/G703OnboardSettings.hpp"

#include <span>
#include <utility>
#include <vector>

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
    auto buttons = ButtonAssignments::read(snapshot->sector.bytes());
    if (!buttons.has_value()) {
        return std::unexpected(buttons.error());
    }
    return G703OnboardSnapshot{
        .sector = snapshot->number,
        .settings = std::move(*settings),
        .backupExists = snapshot->backupExists,
        .buttons = std::move(*buttons),
    };
}

shared_kernel::Result<G703ProfileSettings> G703OnboardSettings::write(const G703ProfileSettings& settings) const
{
    return write(settings, ButtonAssignments::keepAll());
}

shared_kernel::Result<G703ProfileSettings> G703OnboardSettings::write(const G703ProfileSettings& settings,
                                                                      const ButtonAssignments& buttons) const
{
    // "Padrão" precisa da cópia original do setor ativo (leitura apenas). Sem cópia, o setor atual ainda é o original.
    std::vector<std::uint8_t> reference;
    if (buttons.needsReference()) {
        const auto snapshot = editor_.read();
        if (!snapshot.has_value()) {
            return std::unexpected(snapshot.error());
        }
        if (snapshot->backupExists) {
            const auto stored = backup_->load(snapshot->number);
            if (!stored.has_value()) {
                return std::unexpected(stored.error());
            }
            if (!stored->has_value()) {
                return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                           "cópia de segurança sumiu durante a operação");
            }
            reference = **stored;
        }
    }
    const auto written =
        editor_.write([&settings, &buttons, &reference](
                          std::span<const std::uint8_t> original) -> shared_kernel::Result<std::vector<std::uint8_t>> {
            auto edited = settings.applyTo(original);
            if (!edited.has_value()) {
                return edited;
            }
            return buttons.applyTo(*edited, reference.empty() ? original : std::span<const std::uint8_t>{reference});
        });
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
