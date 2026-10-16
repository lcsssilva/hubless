#include "modules/lighting/application/G915OnboardSettings.hpp"

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

namespace ghub::lighting::application {

shared_kernel::Result<G915OnboardSnapshot> G915OnboardSettings::read() const
{
    auto snapshot = editor_.read();
    if (!snapshot.has_value()) {
        return std::unexpected(snapshot.error());
    }
    auto settings = domain::G915ProfileSettings::read(snapshot->sector.bytes());
    if (!settings.has_value()) {
        return std::unexpected(settings.error());
    }
    return G915OnboardSnapshot{
        .sector = snapshot->number,
        .settings = std::move(*settings),
        .backupExists = snapshot->backupExists,
    };
}

shared_kernel::Result<domain::G915ProfileSettings>
G915OnboardSettings::write(const domain::G915ProfileSettings& settings) const
{
    // Primeiro uma leitura (só consulta) para saber qual setor está ativo e qual é a referência de "Original": a
    // cópia de segurança desse setor ou, se ainda não houver, o próprio setor.
    const auto before = editor_.read();
    if (!before.has_value()) {
        return std::unexpected(before.error());
    }
    const auto saved = backup_->load(before->number);
    if (!saved.has_value()) {
        return std::unexpected(saved.error());
    }
    const std::vector<std::uint8_t> baseline = saved->has_value() ? **saved : before->sector.bytes();
    const std::vector<std::uint8_t> seen = before->sector.bytes();

    const auto written = editor_.write(
        [&settings, &baseline, &seen](std::span<const std::uint8_t> original)
            -> shared_kernel::Result<std::vector<std::uint8_t>> {
            // O perfil ativo pode ter sido trocado (ou editado por outro programa) desde a leitura.
            if (!std::ranges::equal(original, seen)) {
                return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                           "o perfil ativo do teclado mudou desde a leitura; leia de novo");
            }
            return settings.applyTo(original, baseline);
        });
    if (!written.has_value()) {
        return std::unexpected(written.error());
    }
    return domain::G915ProfileSettings::read(written->bytes());
}

shared_kernel::Result<domain::G915ProfileSettings> G915OnboardSettings::restoreBackup() const
{
    // Só grava uma cópia que o domínio entende (tamanho e formato do perfil do G915).
    const auto restored = editor_.restoreBackup([](std::span<const std::uint8_t> bytes) -> shared_kernel::Result<void> {
        const auto readable = domain::G915ProfileSettings::read(bytes);
        if (!readable.has_value()) {
            return std::unexpected(readable.error());
        }
        return {};
    });
    if (!restored.has_value()) {
        return std::unexpected(restored.error());
    }
    return domain::G915ProfileSettings::read(restored->bytes());
}

} // namespace ghub::lighting::application
