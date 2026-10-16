#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace ghub::app {

class AppSettings;

// Cópia local das configurações (INC-017 R8, ADR-0028): "Fazer cópia" grava `profiles.json` e as configurações do
// app em `<dir>/config-<aaaammdd-hhmmss>.json` (0600, diretório 0700); "Restaurar" lê uma cópia da lista, valida
// tudo (perfis pelo leitor do módulo profiles, configurações pelo AppSettings) e só então regrava os dois
// arquivos. Nada vai para a rede e nada é gravado na construção nem na listagem. Só acessa o diretório de cópias:
// o nome da cópia a restaurar nunca é um caminho. O arquivo de perfis ausente na cópia (`null`) não é tocado.
class ConfigBackup final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList backups READ backups NOTIFY backupsChanged)
    Q_PROPERTY(QString status READ status NOTIFY backupsChanged)

public:
    static constexpr qint64 maximumFileBytes = 2 * 1024 * 1024;

    // Valida o conteúdo de um profiles.json: vazio = válido, senão o motivo.
    using ProfilesValidator = std::function<QString(const QByteArray&)>;

    // `settings` precisa sobreviver a este objeto.
    ConfigBackup(QString backupDirectory, QString profilesPath, AppSettings& settings, ProfilesValidator validate,
                 QObject* parent = nullptr);

    // <GenericDataLocation>/ghub-linux/backup (respeita XDG_DATA_HOME).
    [[nodiscard]] static QString defaultDirectory();

    [[nodiscard]] QStringList backups() const { return backups_; }
    [[nodiscard]] QString status() const { return status_; }

    // Relê a lista (somente leitura), mais recente primeiro.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool makeBackup();
    // `name` é só o nome do arquivo da lista. Em falha nada é alterado.
    Q_INVOKABLE bool restore(const QString& name);

signals:
    void backupsChanged();
    // Perfis/configurações foram regravados: a janela deve reler os perfis.
    void restored();

private:
    bool fail(const QString& reason);

    QString directory_;
    QString profilesPath_;
    AppSettings* settings_; // não nulo
    ProfilesValidator validate_;
    QStringList backups_;
    QString status_;
};

} // namespace ghub::app
