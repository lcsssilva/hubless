#pragma once

#include "modules/profiles/domain/Profile.hpp"
#include "shared_kernel/Result.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ghub::profiles::json_format {

// Leitor/gravador de UM perfil, compartilhado pelo `profiles.json` e pelo arquivo de importação/exportação
// (INC-017 R5): a validação é a mesma e vive aqui, uma vez só.

// O JSON guarda números como double: só inteiros até 2^53-1 são exatos.
inline constexpr std::int64_t kMaxExactInteger = 9007199254740991LL;

std::unexpected<shared_kernel::DomainError> malformed(const QString& context, const QString& reason);

// Inteiro exato dentro de [minimum, maximum]; qualquer outra coisa -> nullopt.
std::optional<std::int64_t> exactInteger(const QJsonValue& value, std::int64_t minimum, std::int64_t maximum);

struct ParseOptions {
    // Texto que prefixa os motivos de erro (ex.: "profiles.json inválido").
    QString context;
    // A versão 2 em diante aceita "applications" no perfil.
    bool allowApplications = true;
    // Não nulo = modo de importação: chaves desconhecidas são descartadas e listadas aqui (em vez de recusadas) e
    // "applications" é sempre descartado (nunca vem de um arquivo importado).
    std::vector<std::string>* ignored = nullptr;
};

[[nodiscard]] shared_kernel::Result<Profile> parseProfile(const QJsonValue& value, const ParseOptions& options);

[[nodiscard]] QJsonObject toJson(const Profile& profile, bool withApplications);

} // namespace ghub::profiles::json_format
