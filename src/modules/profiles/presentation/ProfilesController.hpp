#pragma once

#include "modules/profiles/application/ActiveApplicationSource.hpp"
#include "modules/profiles/application/ApplyProfile.hpp"
#include "modules/profiles/application/AutomaticProfileSwitcher.hpp"
#include "modules/profiles/application/EqualizerSettingsPort.hpp"
#include "modules/profiles/application/ProfileBook.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

namespace ghub::profiles {

// Perfis na janela (INC-012, R9/R10): ler, salvar, aplicar e remover só por ação do usuário. O I/O (arquivo e
// câmera) roda em thread de trabalho (QtConcurrent); o EQ só é tocado na thread dona. Uma operação por vez: um
// pedido feito durante uma operação é recusado (não é enfileirado) e não há retentativa. Nada é executado na
// construção; `load()` só lê o arquivo. O ProfileBook só é tocado pelo worker enquanto há operação; por isso os
// nomes são uma cópia atualizada ao fim de cada operação. O destrutor espera a operação em andamento. Os
// objetos recebidos precisam sobreviver a este. Use sempre da thread dona.
//
// Troca automática (INC-012 parte 2, ADR-0022): com a opção ligada (persistida), a fonte do aplicativo em foco é
// iniciada na thread dona depois de `load()` ou de ligar a opção, e parada ao desligar e no destrutor. Os avisos
// da fonte chegam de qualquer thread e são reenfileirados. O primeiro aviso depois de cada início ou recarga da
// fonte só atualiza `lastApplication` (o script do KWin informa a janela ativa ao carregar; abrir o app nunca
// aplica perfil). Depois, o aplicativo precisa ficar em foco por `settleDelay` para o perfil associado ser
// aplicado; durante uma operação, só o aplicativo mais recente é reavaliado ao terminar. Sem retentativa.
class ProfilesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY profilesChanged)
    Q_PROPERTY(QStringList profileNames READ profileNames NOTIFY profilesChanged)
    Q_PROPERTY(QString status READ status NOTIFY profilesChanged)
    Q_PROPERTY(QStringList report READ report NOTIFY profilesChanged)
    Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY profilesChanged)
    Q_PROPERTY(bool automaticSwitching READ automaticSwitching NOTIFY profilesChanged)
    // "desligada", "ativa" ou "indisponível: <motivo>".
    Q_PROPERTY(QString switchingStatus READ switchingStatus NOTIFY profilesChanged)
    // Último aplicativo em foco que não é o próprio app; vazio se nenhum.
    Q_PROPERTY(QString lastApplication READ lastApplication NOTIFY profilesChanged)
    // Nome do perfil -> lista de aplicativos associados.
    Q_PROPERTY(QVariantMap profileApplications READ profileApplications NOTIFY profilesChanged)
    // INC-017: perfil padrão da troca automática (vazio = nenhum) e seções "iguais em todos os perfis".
    Q_PROPERTY(QString defaultProfile READ defaultProfile NOTIFY profilesChanged)
    Q_PROPERTY(bool persistentCamera READ persistentCamera NOTIFY profilesChanged)
    Q_PROPERTY(bool persistentEqualizer READ persistentEqualizer NOTIFY profilesChanged)
    // INC-017 parte 2: as seções de hardware (sidetone, luzes do G733, efeito do G915, DPI do G703).
    Q_PROPERTY(bool persistentSidetone READ persistentSidetone NOTIFY profilesChanged)
    Q_PROPERTY(bool persistentHeadsetLights READ persistentHeadsetLights NOTIFY profilesChanged)
    Q_PROPERTY(bool persistentKeyboardEffect READ persistentKeyboardEffect NOTIFY profilesChanged)
    Q_PROPERTY(bool persistentPointerDpi READ persistentPointerDpi NOTIFY profilesChanged)

public:
    static constexpr std::chrono::milliseconds defaultSettleDelay{500};

    ProfilesController(ProfileBook& book, const ApplyProfile& applyProfile, EqualizerSettingsPort& equalizer,
                       ActiveApplicationSource& source, std::chrono::milliseconds settleDelay = defaultSettleDelay,
                       QObject* parent = nullptr);
    ~ProfilesController() override;

    ProfilesController(const ProfilesController&) = delete;
    ProfilesController& operator=(const ProfilesController&) = delete;
    ProfilesController(ProfilesController&&) = delete;
    ProfilesController& operator=(ProfilesController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QStringList profileNames() const { return names_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QStringList report() const { return report_; }
    [[nodiscard]] bool loadFailed() const { return loadFailed_; }
    [[nodiscard]] bool automaticSwitching() const { return automaticSwitching_; }
    [[nodiscard]] QString switchingStatus() const;
    [[nodiscard]] QString lastApplication() const { return lastApplication_; }
    [[nodiscard]] QVariantMap profileApplications() const { return profileApplications_; }
    [[nodiscard]] QString defaultProfile() const { return defaultProfile_; }
    [[nodiscard]] bool persistentCamera() const { return persistentCamera_; }
    [[nodiscard]] bool persistentEqualizer() const { return persistentEqualizer_; }
    [[nodiscard]] bool persistentSidetone() const { return isPersistent(ProfileSection::Sidetone); }
    [[nodiscard]] bool persistentHeadsetLights() const { return isPersistent(ProfileSection::HeadsetLights); }
    [[nodiscard]] bool persistentKeyboardEffect() const { return isPersistent(ProfileSection::KeyboardEffect); }
    [[nodiscard]] bool persistentPointerDpi() const { return isPersistent(ProfileSection::PointerDpi); }

    // Lê o arquivo de perfis (só leitura). Chamado uma vez pelo composition root.
    Q_INVOKABLE void load();
    // true se já existe perfil com esse nome (comparação depois do trim), pelos nomes já publicados.
    Q_INVOKABLE bool exists(const QString& name) const;
    Q_INVOKABLE void saveCurrent(const QString& name, bool replace);
    Q_INVOKABLE void apply(const QString& name);
    Q_INVOKABLE void remove(const QString& name);
    Q_INVOKABLE void setAutomaticSwitching(bool enabled);
    // Associa `lastApplication` ao perfil; sem aplicativo registrado -> status de erro, nada é gravado.
    Q_INVOKABLE void assignLastApplication(const QString& profile);
    Q_INVOKABLE void unassignApplication(const QString& application);
    // INC-017 R4/R5: duplicar, exportar e importar (caminho local ou URL file://). Importar nunca aplica nada; nome
    // já existente sem `replace` emite `importNeedsReplace` (a janela pergunta "Substituir?" e repete com true).
    Q_INVOKABLE void duplicate(const QString& source, const QString& copyName);
    Q_INVOKABLE void exportProfile(const QString& name, const QString& path);
    Q_INVOKABLE void importProfile(const QString& path, bool replace);
    // INC-017 R6/R7: perfil padrão (vazio = nenhum) e seção persistente ("camera", "equalizer", "sidetone",
    // "headsetLights", "keyboardEffect" ou "pointerDpi").
    Q_INVOKABLE void setDefaultProfile(const QString& name);
    Q_INVOKABLE void setPersistent(const QString& section, bool persistent);

signals:
    void profilesChanged();
    // Houve seção de câmera na aplicação (com sucesso ou não): a janela deve reler a câmera.
    void cameraApplied();
    // A importação encontrou um perfil com o mesmo nome; nada foi gravado.
    void importNeedsReplace(const QString& name, const QString& path);

private:
    enum class Kind : unsigned char {
        Load,
        Save,
        Apply,
        Remove,
        Switching,
        Assign,
        Unassign,
        Duplicate,
        Export,
        Import,
        Default,
        Persistent
    };

    struct Outcome {
        Kind kind = Kind::Load;
        std::optional<shared_kernel::DomainError> error;
        std::optional<ProfileBook::SaveReport> saved;
        std::vector<ApplyProfile::CameraItemResult> cameraItems;
        std::optional<ProfileBook::ImportReport> imported;
        std::vector<ApplyProfile::SectionResult> hardwareItems; // sidetone, luzes e DPI (worker)
    };

    // `viaApplication` vazio = aplicação manual.
    void startApply(const Profile& profile, const QString& viaApplication);
    void startSource();
    void stopSource();
    void onApplicationActivated(const ApplicationId& application);
    void onSourceStatus(const std::optional<shared_kernel::DomainError>& status);
    void onSettled();
    void decideAndApply(const ApplicationId& application);
    void startPersistence(Kind kind, std::function<shared_kernel::Result<void>(ProfileBook&)> work);

    // Recusa com status se há operação em andamento.
    [[nodiscard]] bool refuseIfBusy();
    void start(QFuture<Outcome> future);
    void onFinished();
    void handleFinished();
    void finishSave(const Outcome& outcome);
    void finishApply(const Outcome& outcome);
    void finishImport(const Outcome& outcome);
    void refreshFromBook();
    [[nodiscard]] bool isPersistent(ProfileSection section) const
    {
        return persistent_.test(static_cast<std::size_t>(section));
    }

    ProfileBook* book_;                // não nulo
    ActiveApplicationSource* source_;  // não nulo
    const ApplyProfile* applyProfile_; // não nulo
    EqualizerSettingsPort* equalizer_; // não nulo
    QFutureWatcher<Outcome> watcher_;
    std::optional<Profile> applying_; // perfil em aplicação (cópia)
    QString operationName_;
    QString operationApplication_; // aplicação automática em curso (vazio = manual)
    QString operationPath_;        // arquivo da importação em curso
    bool operationPersistent_ = false;
    SectionSet skip_; // seções persistentes: não são alteradas ao aplicar (INC-017 R7)
    QString defaultProfile_;
    bool persistentCamera_ = false;
    bool persistentEqualizer_ = false;
    SectionSet persistent_; // todas as seções persistentes, publicadas ao fim de cada operação
    AutomaticProfileSwitcher switcher_;
    QTimer settleTimer_;
    std::optional<ApplicationId> pending_;  // aguardando o prazo de foco
    std::optional<ApplicationId> deferred_; // prazo cumprido durante uma operação
    QString lastApplication_;
    QVariantMap profileApplications_;
    std::optional<QString> sourceError_;
    bool automaticSwitching_ = false;
    bool sourceRunning_ = false;
    bool awaitingFirstActivation_ = false;
    QStringList names_;
    QStringList report_;
    QString status_;
    bool loadFailed_ = false;
    bool running_ = false;
};

} // namespace ghub::profiles
