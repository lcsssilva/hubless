#pragma once

#include "modules/profiles/domain/Profile.hpp"
#include "modules/profiles/domain/ProfileSection.hpp"
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
    // A versão 4 (e a 2 do arquivo de importação) aceita as seções de hardware: sidetone, headsetLights,
    // keyboardEffect e pointerDpi (INC-017 parte 2). Em modo de importação, fora delas são "ignoradas".
    bool allowHardware = true;
    // Não nulo = modo de importação: chaves desconhecidas são descartadas e listadas aqui (em vez de recusadas) e
    // "applications" é sempre descartado (nunca vem de um arquivo importado).
    std::vector<std::string>* ignored = nullptr;
};

[[nodiscard]] shared_kernel::Result<Profile> parseProfile(const QJsonValue& value, const ParseOptions& options);

// Nome da seção no JSON ("camera", "equalizer", "sidetone", "headsetLights", "keyboardEffect", "pointerDpi") e o
// caminho inverso (chave desconhecida -> nullopt). `hardwareSections` false recusa as quatro seções novas.
[[nodiscard]] QString sectionKey(ProfileSection section);
[[nodiscard]] std::optional<ProfileSection> sectionFromKey(const QString& key, bool hardwareSections);

[[nodiscard]] QJsonObject toJson(const Profile& profile, bool withApplications);

} // namespace ghub::profiles::json_format
