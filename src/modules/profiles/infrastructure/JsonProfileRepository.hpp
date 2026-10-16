#pragma once

#include "modules/profiles/application/ProfileRepository.hpp"

#include <QByteArray>
#include <QString>

namespace ghub::profiles {

// Persistência da biblioteca em JSON versionado (ADR-0020, R7/R8; ADR-0022; ADR-0028):
// {"format":"ghub-linux-profiles","version":3,"automaticSwitching":b,["defaultProfile":s,]
// ["persistentSections":["camera","equalizer"],]"profiles":[{"name":..,"camera":[{"id":n,"name":s,"value":n}],
// "equalizer":[10 inteiros],"applications":[..]}]}. Seção ausente = chave ausente. Lê as versões 1, 2 e 3.
// Bloqueante: thread de trabalho.
class JsonProfileRepository final : public ProfileRepository {
public:
    static constexpr qint64 maximumFileBytes = 1024 * 1024;

    explicit JsonProfileRepository(QString filePath);

    // <GenericConfigLocation>/ghub-linux/profiles.json (respeita XDG_CONFIG_HOME).
    [[nodiscard]] static QString defaultFilePath();

    // Arquivo ausente -> biblioteca vazia. Arquivo maior que 1 MiB, JSON inválido, formato/versão diferentes,
    // tipo errado, inteiro fora de faixa ou violação do domínio -> MalformedData com o motivo. Arquivo
    // existente mas ilegível -> BackendUnavailable.
    [[nodiscard]] shared_kernel::Result<ProfileLibrary> load() override;

    // Valida o conteúdo de um `profiles.json` (usado também para validar uma cópia local antes de restaurá-la,
    // INC-017 R8). Mesmas regras de load(): MalformedData com o motivo.
    [[nodiscard]] static shared_kernel::Result<ProfileLibrary> parseDocument(const QByteArray& content);

    // Recusa (sem tocar o arquivo) enquanto a última leitura terminou em erro. Gravação atômica (QSaveFile),
    // arquivo 0600 e diretório criado com 0700. Falha de escrita -> BackendUnavailable.
    [[nodiscard]] shared_kernel::Result<void> save(const ProfileLibrary& library) override;

private:
    QString filePath_;
    bool lastLoadFailed_ = false;
};

} // namespace ghub::profiles
