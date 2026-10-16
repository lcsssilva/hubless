#include "modules/profiles/presentation/ProfilesController.hpp"

#include <QDebug>
#include <QPointer>
#include <QUrl>
#include <QtConcurrent>

#include <string>
#include <utility>

namespace ghub::profiles {

namespace {

QString fromStd(const std::string& text)
{
    return QString::fromStdString(text);
}

// Caminho local a partir do que o diálogo de arquivo entrega (caminho ou URL file://).
std::string localPath(const QString& path)
{
    const QUrl url{path};
    return (url.isLocalFile() ? url.toLocalFile() : path).toStdString();
}

// Seção <-> nome usado pela janela e pelo JSON.
struct SectionName {
    const char* key;
    ProfileSection section;
    const char* label; // para a janela
};
constexpr SectionName kSections[] = {{"camera", ProfileSection::Camera, "Câmera"},
                                     {"equalizer", ProfileSection::Equalizer, "EQ"},
                                     {"sidetone", ProfileSection::Sidetone, "Sidetone do G733"},
                                     {"headsetLights", ProfileSection::HeadsetLights, "Luzes do G733"},
                                     {"keyboardEffect", ProfileSection::KeyboardEffect, "Efeito do G915"},
                                     {"pointerDpi", ProfileSection::PointerDpi, "DPI do G703"}};

QString sectionLabel(ProfileSection section)
{
    for (const SectionName& entry : kSections) {
        if (entry.section == section) {
            return QString::fromUtf8(entry.label);
        }
    }
    return {};
}

QString controlLabel(const ApplyProfile::CameraItemResult& item)
{
    return item.name.empty() ? QStringLiteral("0x%1").arg(item.id, 8, 16, QLatin1Char('0')) : fromStd(item.name);
}

} // namespace

ProfilesController::ProfilesController(ProfileBook& book, const ApplyProfile& applyProfile,
                                       EqualizerSettingsPort& equalizer, ActiveApplicationSource& source,
                                       std::chrono::milliseconds settleDelay, QObject* parent)
    : QObject(parent), book_(&book), source_(&source), applyProfile_(&applyProfile), equalizer_(&equalizer)
{
    settleTimer_.setParent(this); // visível por findChild nos testes
    settleTimer_.setObjectName(QStringLiteral("profilesSettleTimer"));
    settleTimer_.setSingleShot(true);
    settleTimer_.setInterval(settleDelay);
    connect(&settleTimer_, &QTimer::timeout, this, &ProfilesController::onSettled);
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &ProfilesController::onFinished);
}

ProfilesController::~ProfilesController()
{
    // As operações não são canceláveis: espera terminar, pois o livro e os ports precisam continuar válidos.
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
    settleTimer_.stop();
    // Depois do worker: nenhum aviso pode chegar com o controlador meio destruído.
    source_->stop();
}

QString ProfilesController::switchingStatus() const
{
    if (!automaticSwitching_) {
        return tr("desligada");
    }
    if (sourceError_.has_value()) {
        return tr("indisponível: %1").arg(*sourceError_);
    }
    return tr("ativa");
}

bool ProfilesController::refuseIfBusy()
{
    if (!running_) {
        return false;
    }
    status_ = tr("aguarde a operação em andamento");
    emit profilesChanged();
    return true;
}

void ProfilesController::start(QFuture<Outcome> future)
{
    running_ = true;
    watcher_.setFuture(future);
    emit profilesChanged();
}

void ProfilesController::load()
{
    if (refuseIfBusy()) {
        return;
    }
    ProfileBook& book = *book_;
    start(QtConcurrent::run([&book]() -> Outcome {
        Outcome outcome{.kind = Kind::Load,
                        .error = std::nullopt,
                        .saved = std::nullopt,
                        .cameraItems = {},
                        .imported = std::nullopt,
                        .hardwareItems = {}};
        if (auto loaded = book.load(); !loaded) {
            outcome.error = loaded.error();
        }
        return outcome;
    }));
}

bool ProfilesController::exists(const QString& name) const
{
    const auto valid = ProfileName::create(name.toStdString());
    return valid.has_value() && names_.contains(fromStd(valid->value()));
}

void ProfilesController::saveCurrent(const QString& name, bool replace)
{
    if (refuseIfBusy()) {
        return;
    }
    // A curva do EQ só pode ser lida na thread dona; vai pronta para o worker.
    const std::array<int, 10> gains = equalizer_->currentGains();
    operationName_ = name.trimmed();
    ProfileBook& book = *book_;
    const std::string rawName = name.toStdString();
    start(QtConcurrent::run([&book, rawName, gains, replace]() -> Outcome {
        Outcome outcome{.kind = Kind::Save,
                        .error = std::nullopt,
                        .saved = std::nullopt,
                        .cameraItems = {},
                        .imported = std::nullopt,
                        .hardwareItems = {}};
        auto saved = book.saveCurrent(rawName, gains, replace);
        if (saved) {
            outcome.saved = *saved;
        } else {
            outcome.error = saved.error();
        }
        return outcome;
    }));
}

void ProfilesController::apply(const QString& name)
{
    if (refuseIfBusy()) {
        return;
    }
    const auto valid = ProfileName::create(name.toStdString());
    const Profile* found = valid.has_value() ? book_->library().find(*valid) : nullptr;
    if (found == nullptr) {
        status_ = tr("Perfil não encontrado: %1").arg(name);
        report_.clear();
        emit profilesChanged();
        return;
    }
    startApply(*found, QString());
}

void ProfilesController::startApply(const Profile& profile, const QString& viaApplication)
{
    // Manual ou automática, toda aplicação passa a ser o "perfil atual" da troca automática.
    switcher_.noteApplied(profile.name());
    applying_ = profile;
    operationName_ = fromStd(profile.name().value());
    operationApplication_ = viaApplication;
    const ApplyProfile& useCase = *applyProfile_;
    skip_ = book_->library().persistentSections();
    const SectionSet skip = skip_;
    const bool skipCamera = skip.test(static_cast<std::size_t>(ProfileSection::Camera));
    start(QtConcurrent::run([&useCase, profile, skip, skipCamera]() -> Outcome {
        return Outcome{.kind = Kind::Apply,
                       .error = std::nullopt,
                       .saved = std::nullopt,
                       .cameraItems =
                           skipCamera ? std::vector<ApplyProfile::CameraItemResult>{} : useCase.applyCamera(profile),
                       .imported = std::nullopt,
                       .hardwareItems = useCase.applyHardware(profile, skip)};
    }));
}

void ProfilesController::remove(const QString& name)
{
    if (refuseIfBusy()) {
        return;
    }
    operationName_ = name.trimmed();
    const std::string rawName = name.toStdString();
    startPersistence(Kind::Remove, [rawName](ProfileBook& book) { return book.remove(rawName); });
}

void ProfilesController::startPersistence(Kind kind, std::function<shared_kernel::Result<void>(ProfileBook&)> work)
{
    ProfileBook& book = *book_;
    start(QtConcurrent::run([&book, kind, work = std::move(work)]() -> Outcome {
        Outcome outcome{
            .kind = kind, .error = std::nullopt, .saved = std::nullopt, .cameraItems = {}, .imported = std::nullopt,
                        .hardwareItems = {}};
        if (auto done = work(book); !done) {
            outcome.error = done.error();
        }
        return outcome;
    }));
}

void ProfilesController::setAutomaticSwitching(bool enabled)
{
    if (refuseIfBusy()) {
        return;
    }
    startPersistence(Kind::Switching, [enabled](ProfileBook& book) { return book.setAutomaticSwitching(enabled); });
}

void ProfilesController::assignLastApplication(const QString& profile)
{
    if (refuseIfBusy()) {
        return;
    }
    if (lastApplication_.isEmpty()) {
        status_ = tr("Falha: nenhum aplicativo em foco registrado ainda");
        emit profilesChanged();
        return;
    }
    operationName_ = profile.trimmed();
    operationApplication_ = lastApplication_;
    const std::string rawProfile = profile.toStdString();
    const std::string application = lastApplication_.toStdString();
    startPersistence(Kind::Assign, [rawProfile, application](ProfileBook& book) {
        return book.assignApplication(rawProfile, application);
    });
}

void ProfilesController::unassignApplication(const QString& application)
{
    if (refuseIfBusy()) {
        return;
    }
    operationApplication_ = application.trimmed();
    const std::string raw = application.toStdString();
    startPersistence(Kind::Unassign, [raw](ProfileBook& book) { return book.unassignApplication(raw); });
}

void ProfilesController::duplicate(const QString& source, const QString& copyName)
{
    if (refuseIfBusy()) {
        return;
    }
    operationName_ = copyName.trimmed();
    const std::string from = source.toStdString();
    const std::string to = copyName.toStdString();
    startPersistence(Kind::Duplicate, [from, to](ProfileBook& book) { return book.duplicate(from, to); });
}

void ProfilesController::exportProfile(const QString& name, const QString& path)
{
    if (refuseIfBusy()) {
        return;
    }
    operationName_ = name.trimmed();
    const std::string raw = name.toStdString();
    const std::string file = localPath(path);
    startPersistence(Kind::Export, [raw, file](ProfileBook& book) { return book.exportProfile(raw, file); });
}

void ProfilesController::importProfile(const QString& path, bool replace)
{
    if (refuseIfBusy()) {
        return;
    }
    const std::string file = localPath(path);
    operationPath_ = path;
    ProfileBook& book = *book_;
    start(QtConcurrent::run([&book, file, replace]() -> Outcome {
        Outcome outcome{.kind = Kind::Import,
                        .error = std::nullopt,
                        .saved = std::nullopt,
                        .cameraItems = {},
                        .imported = std::nullopt,
                        .hardwareItems = {}};
        auto imported = book.importProfile(file, replace);
        if (imported) {
            outcome.imported = std::move(*imported);
        } else {
            outcome.error = imported.error();
        }
        return outcome;
    }));
}

void ProfilesController::setDefaultProfile(const QString& name)
{
    if (refuseIfBusy()) {
        return;
    }
    operationName_ = name.trimmed();
    const std::string raw = name.toStdString();
    const bool clear = name.trimmed().isEmpty();
    startPersistence(Kind::Default, [raw, clear](ProfileBook& book) {
        return book.setDefaultProfile(clear ? std::nullopt : std::optional<std::string_view>{raw});
    });
}

void ProfilesController::setPersistent(const QString& section, bool persistent)
{
    if (refuseIfBusy()) {
        return;
    }
    std::optional<ProfileSection> parsed;
    for (const SectionName& entry : kSections) {
        if (section == QLatin1String(entry.key)) {
            parsed = entry.section;
        }
    }
    if (!parsed.has_value()) {
        status_ = tr("Falha: seção desconhecida: %1").arg(section);
        emit profilesChanged();
        return;
    }
    operationName_ = section;
    operationPersistent_ = persistent;
    startPersistence(Kind::Persistent,
                     [wanted = *parsed, persistent](ProfileBook& book) { return book.setPersistent(wanted, persistent); });
}

void ProfilesController::refreshFromBook()
{
    // Só chamado com o worker parado: ler o livro aqui é seguro.
    names_.clear();
    for (const auto& profile : book_->library().profiles()) {
        names_.push_back(fromStd(profile.name().value()));
    }
    loadFailed_ = book_->loadError().has_value();
    automaticSwitching_ = book_->library().automaticSwitching();
    defaultProfile_ =
        book_->library().defaultProfile().has_value() ? fromStd(book_->library().defaultProfile()->value()) : QString();
    persistentCamera_ = book_->library().isPersistent(ProfileSection::Camera);
    persistentEqualizer_ = book_->library().isPersistent(ProfileSection::Equalizer);
    persistent_ = book_->library().persistentSections();
    profileApplications_.clear();
    for (const auto& profile : book_->library().profiles()) {
        QStringList applications;
        for (const auto& application : profile.applications()) {
            applications.push_back(fromStd(application.value()));
        }
        profileApplications_.insert(fromStd(profile.name().value()), applications);
    }
}

void ProfilesController::onFinished()
{
    handleFinished();
    // Um prazo cumprido durante a operação é avaliado agora, só com o aplicativo mais recente.
    if (!running_ && deferred_.has_value()) {
        const ApplicationId application = *deferred_;
        deferred_.reset();
        decideAndApply(application);
    }
}

void ProfilesController::handleFinished()
{
    const Outcome outcome = watcher_.result();
    running_ = false;
    refreshFromBook();

    if (outcome.kind == Kind::Import && outcome.error.has_value() &&
        outcome.error->code == shared_kernel::ErrorCode::InvalidArgument &&
        outcome.error->detail.starts_with(ProfileBook::alreadyExistsPrefix)) {
        const QString existing = fromStd(outcome.error->detail.substr(ProfileBook::alreadyExistsPrefix.size()));
        status_ = tr("Já existe um perfil com o nome %1").arg(existing);
        emit profilesChanged();
        emit importNeedsReplace(existing, operationPath_);
        return;
    }
    if (outcome.error.has_value() && outcome.kind != Kind::Apply) {
        qWarning("profiles: operação falhou (código %d): %s", static_cast<int>(outcome.error->code),
                 outcome.error->detail.c_str());
        status_ = outcome.kind == Kind::Load ? tr("Falha ao ler os perfis: %1").arg(fromStd(outcome.error->detail))
                                             : tr("Falha: %1").arg(fromStd(outcome.error->detail));
        emit profilesChanged();
        return;
    }
    switch (outcome.kind) {
    case Kind::Load:
        status_ = tr("Perfis carregados: %1").arg(names_.size());
        if (automaticSwitching_) {
            startSource();
        }
        break;
    case Kind::Save:
        finishSave(outcome);
        break;
    case Kind::Remove:
        status_ = tr("Perfil removido: %1").arg(operationName_);
        break;
    case Kind::Switching:
        if (automaticSwitching_) {
            startSource();
        } else {
            stopSource();
        }
        status_ = automaticSwitching_ ? tr("Troca automática ligada") : tr("Troca automática desligada");
        break;
    case Kind::Assign:
        status_ = tr("Aplicativo %1 associado ao perfil %2").arg(operationApplication_, operationName_);
        break;
    case Kind::Unassign:
        status_ = tr("Aplicativo %1 desassociado").arg(operationApplication_);
        break;
    case Kind::Duplicate:
        status_ = tr("Perfil duplicado: %1").arg(operationName_);
        break;
    case Kind::Export:
        status_ = tr("Perfil exportado: %1").arg(operationName_);
        break;
    case Kind::Import:
        finishImport(outcome);
        break;
    case Kind::Default:
        status_ =
            defaultProfile_.isEmpty() ? tr("Perfil padrão removido") : tr("Perfil padrão: %1").arg(defaultProfile_);
        break;
    case Kind::Persistent:
        status_ = operationPersistent_ ? tr("Seção %1: igual em todos os perfis").arg(operationName_)
                                       : tr("Seção %1: deixou de ser igual em todos os perfis").arg(operationName_);
        break;
    case Kind::Apply:
        finishApply(outcome);
        return; // finishApply emite os sinais
    }
    emit profilesChanged();
}

void ProfilesController::finishSave(const Outcome& outcome)
{
    report_.clear();
    const ProfileBook::SaveReport& saved = *outcome.saved;
    if (saved.cameraIncluded) {
        const Profile* profile = nullptr;
        if (const auto name = ProfileName::create(operationName_.toStdString()); name.has_value()) {
            profile = book_->library().find(*name);
        }
        report_.push_back(profile != nullptr ? tr("Câmera: %1 controles incluídos").arg(profile->camera().size())
                                             : tr("Câmera incluída"));
    } else if (saved.cameraOmittedReason.has_value()) {
        report_.push_back(tr("Câmera não incluída: %1").arg(fromStd(*saved.cameraOmittedReason)));
    }
    if (saved.equalizerIncluded) {
        report_.push_back(tr("EQ: curva incluída"));
    }
    for (const ProfileBook::SectionNote& note : saved.hardware) {
        report_.push_back(note.included ? tr("%1: incluído").arg(sectionLabel(note.section))
                                        : tr("%1 não incluído: %2")
                                              .arg(sectionLabel(note.section), fromStd(note.omittedReason)));
    }
    status_ = tr("Perfil salvo: %1").arg(operationName_);
}

void ProfilesController::finishImport(const Outcome& outcome)
{
    report_.clear();
    const ProfileBook::ImportReport& imported = *outcome.imported;
    status_ = imported.replaced ? tr("Perfil importado (substituído): %1").arg(fromStd(imported.name))
                                : tr("Perfil importado: %1").arg(fromStd(imported.name));
    if (!imported.ignoredFields.empty()) {
        QStringList fields;
        for (const std::string& field : imported.ignoredFields) {
            fields.push_back(fromStd(field));
        }
        report_.push_back(tr("Campos ignorados: %1").arg(fields.join(QStringLiteral(", "))));
    }
}

void ProfilesController::finishApply(const Outcome& outcome)
{
    report_.clear();
    bool allOk = true;
    for (const auto& item : outcome.cameraItems) {
        if (item.outcome.has_value()) {
            report_.push_back(tr("%1: %2 aplicado").arg(controlLabel(item)).arg(*item.outcome));
        } else {
            allOk = false;
            report_.push_back(tr("%1: falhou — %2").arg(controlLabel(item), fromStd(item.outcome.error().detail)));
        }
    }
    const auto skipped = [this](ProfileSection section) { return skip_.test(static_cast<std::size_t>(section)); };
    const HardwareSections& wanted = applying_->hardware();
    const bool skipCamera = skipped(ProfileSection::Camera);
    const bool skipEqualizer = skipped(ProfileSection::Equalizer);
    if (skipCamera && !applying_->camera().empty()) {
        report_.push_back(tr("Câmera: igual em todos os perfis (não alterada)"));
    }
    if (skipEqualizer && applying_->equalizer().has_value()) {
        report_.push_back(tr("EQ: igual em todos os perfis (não alterado)"));
    }
    const std::pair<ProfileSection, bool> hardwarePresent[] = {
        {ProfileSection::Sidetone, wanted.sidetone.has_value()},
        {ProfileSection::HeadsetLights, !wanted.headsetLights.empty()},
        {ProfileSection::KeyboardEffect, wanted.keyboardEffect.has_value()},
        {ProfileSection::PointerDpi, wanted.pointerDpi.has_value()}};
    for (const auto& [section, present] : hardwarePresent) {
        if (present && skipped(section)) {
            report_.push_back(tr("%1: igual em todos os perfis (não alterado)").arg(sectionLabel(section)));
        }
    }
    // Seções de hardware (R11): cada uma lista o que foi aplicado e o que falhou.
    const auto reportSection = [this, &allOk](const ApplyProfile::SectionResult& item) {
        if (item.outcome.has_value()) {
            report_.push_back(tr("%1: aplicado — %2").arg(fromStd(item.label), fromStd(*item.outcome)));
        } else {
            allOk = false;
            report_.push_back(tr("%1: falhou — %2").arg(fromStd(item.label), fromStd(item.outcome.error().detail)));
        }
    };
    for (const auto& item : outcome.hardwareItems) {
        reportSection(item);
    }
    // O efeito do G915 e o EQ só são tocados aqui, na thread dona.
    for (const auto& item : applyProfile_->applyKeyboardEffect(*applying_, skip_)) {
        reportSection(item);
    }
    const auto equalizerResult = skipEqualizer ? std::optional<shared_kernel::Result<void>>{}
                                               : applyProfile_->applyEqualizer(*applying_);
    if (equalizerResult.has_value()) {
        if (equalizerResult->has_value()) {
            report_.push_back(tr("EQ: curva definida (vale com o EQ ligado)"));
        } else {
            allOk = false;
            report_.push_back(tr("EQ: falhou — %1").arg(fromStd(equalizerResult->error().detail)));
        }
    }
    const bool hadCamera = !applying_->camera().empty() && !skipCamera;
    applying_.reset();
    if (operationApplication_.isEmpty()) {
        status_ = allOk ? tr("Perfil aplicado: %1").arg(operationName_)
                        : tr("Perfil aplicado com falhas: %1").arg(operationName_);
    } else {
        status_ =
            allOk ? tr("Perfil %1 aplicado automaticamente (aplicativo %2)").arg(operationName_, operationApplication_)
                  : tr("Perfil %1 aplicado automaticamente, com falhas (aplicativo %2)")
                        .arg(operationName_, operationApplication_);
        report_.push_front(tr("Aplicação automática pelo aplicativo %1").arg(operationApplication_));
    }
    operationApplication_.clear();
    emit profilesChanged();
    if (hadCamera) {
        emit cameraApplied();
    }
}

void ProfilesController::startSource()
{
    if (sourceRunning_) {
        return;
    }
    // Os avisos chegam de qualquer thread: só reenfileiram para a thread dona.
    const QPointer<ProfilesController> self{this};
    awaitingFirstActivation_ = true;
    auto started = source_->start(
        [self](ApplicationId application) {
            if (self != nullptr) {
                QMetaObject::invokeMethod(
                    self.data(), [self, application] { self->onApplicationActivated(application); },
                    Qt::QueuedConnection);
            }
        },
        [self](std::optional<shared_kernel::DomainError> status) {
            if (self != nullptr) {
                QMetaObject::invokeMethod(
                    self.data(), [self, status = std::move(status)] { self->onSourceStatus(status); },
                    Qt::QueuedConnection);
            }
        });
    if (started.has_value()) {
        sourceRunning_ = true;
        sourceError_.reset();
    } else {
        sourceError_ = fromStd(started.error().detail);
    }
}

void ProfilesController::stopSource()
{
    source_->stop();
    sourceRunning_ = false;
    sourceError_.reset();
    awaitingFirstActivation_ = false;
    settleTimer_.stop();
    pending_.reset();
    deferred_.reset();
}

void ProfilesController::onSourceStatus(const std::optional<shared_kernel::DomainError>& status)
{
    if (!sourceRunning_) {
        return; // aviso atrasado de uma fonte já parada
    }
    if (status.has_value()) {
        sourceError_ = fromStd(status->detail);
    } else {
        // Recarga: o script volta a informar a janela ativa; esse primeiro aviso não pode aplicar nada.
        sourceError_.reset();
        awaitingFirstActivation_ = true;
    }
    emit profilesChanged();
}

void ProfilesController::onApplicationActivated(const ApplicationId& application)
{
    if (!sourceRunning_ || !automaticSwitching_) {
        return; // aviso atrasado
    }
    if (awaitingFirstActivation_) {
        awaitingFirstActivation_ = false;
        if (application != ApplicationId::ownApplication()) {
            lastApplication_ = fromStd(application.value());
            emit profilesChanged();
        }
        return;
    }
    if (application == ApplicationId::ownApplication()) {
        return;
    }
    lastApplication_ = fromStd(application.value());
    pending_ = application;
    deferred_.reset(); // o foco mudou: o aviso guardado ficou velho
    settleTimer_.start();
    emit profilesChanged();
}

void ProfilesController::onSettled()
{
    if (!pending_.has_value() || !automaticSwitching_) {
        return;
    }
    const ApplicationId application = *pending_;
    pending_.reset();
    if (running_) {
        deferred_ = application; // reavaliado ao fim da operação em andamento
        return;
    }
    decideAndApply(application);
}

void ProfilesController::decideAndApply(const ApplicationId& application)
{
    const std::optional<ProfileName> decision = switcher_.decide(book_->library(), application);
    if (!decision.has_value()) {
        return;
    }
    const Profile* profile = book_->library().find(*decision);
    if (profile == nullptr) {
        return;
    }
    startApply(*profile, fromStd(application.value()));
}

} // namespace ghub::profiles
