#pragma once

#include "modules/profiles/domain/Profile.hpp"
#include "shared_kernel/Result.hpp"

#include <string>
#include <vector>

namespace ghub::profiles {

// Perfil lido de um arquivo de importação; `ignoredFields` lista os campos desconhecidos que foram descartados
// (INC-017 R5). Os aplicativos associados nunca vêm de um arquivo importado.
struct ImportedProfile {
    Profile profile;
    std::vector<std::string> ignoredFields;
};

// Exportação e importação de UM perfil em arquivo escolhido pelo usuário. Bloqueante: thread de trabalho.
class ProfileTransfer {
public:
    virtual ~ProfileTransfer() = default;

    // Falha de escrita -> BackendUnavailable.
    [[nodiscard]] virtual shared_kernel::Result<void> exportProfile(const Profile& profile,
                                                                    const std::string& path) = 0;

    // Arquivo ilegível -> BackendUnavailable; grande demais, JSON inválido, versão desconhecida ou valor inválido
    // -> MalformedData. Nada é aplicado nem gravado na biblioteca.
    [[nodiscard]] virtual shared_kernel::Result<ImportedProfile> importProfile(const std::string& path) = 0;
};

} // namespace ghub::profiles
