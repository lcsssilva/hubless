#pragma once

#include "modules/profiles/application/ProfileTransfer.hpp"

#include <QtGlobal>

namespace ghub::profiles {

// Exportação/importação de um perfil em JSON (INC-017 R4/R5):
// {"format":"ghub-linux-profile","version":1,"profile":{"name":..,"camera":[..],"equalizer":[..]}}.
// A importação é entrada não confiável: arquivo de no máximo 64 KiB, versão conhecida, mesmo leitor de perfil do
// `profiles.json` (objetos de valor), campos desconhecidos descartados e listados, aplicativos nunca importados.
class JsonProfileTransfer final : public ProfileTransfer {
public:
    static constexpr qint64 maximumFileBytes = 64 * 1024;

    // Gravação atômica, arquivo 0600; não cria diretórios (o caminho foi escolhido pelo usuário).
    [[nodiscard]] shared_kernel::Result<void> exportProfile(const Profile& profile, const std::string& path) override;

    [[nodiscard]] shared_kernel::Result<ImportedProfile> importProfile(const std::string& path) override;
};

} // namespace ghub::profiles
