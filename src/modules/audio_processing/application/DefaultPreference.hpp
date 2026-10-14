#pragma once

#include <optional>
#include <string>
#include <utility>

#include "shared_kernel/Result.hpp"

namespace ghub::audio_processing {

// Regra comum do EQ de saída e da redução de ruído do microfone (ADR-0016/0018): ao desligar, a preferência de
// dispositivo padrão guardada na ativação só é restaurada se a atual ainda for a nossa (`ourName`); se o usuário
// trocou, a escolha dele é respeitada e nada é gravado. `saved` ausente apaga a chave.
//
// readCurrent: () -> Result<std::optional<std::string>>; writePreference: (const std::optional<std::string>&)
// -> Result<void>. Erro ao ler ou gravar é devolvido como veio; nada é gravado se a leitura falhar.
template <typename ReadCurrent, typename WritePreference>
[[nodiscard]] shared_kernel::Result<void>
restoreDefaultIfStillOurs(ReadCurrent&& readCurrent, WritePreference&& writePreference, const std::string& ourName,
                          const std::optional<std::string>& saved)
{
    const auto current = std::forward<ReadCurrent>(readCurrent)();
    if (!current) {
        return std::unexpected(current.error());
    }
    if (*current != ourName) {
        return {};
    }
    return std::forward<WritePreference>(writePreference)(saved);
}

} // namespace ghub::audio_processing
