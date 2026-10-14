#include "modules/profiles/infrastructure/KWinActiveApplicationSource.hpp"

#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QStringList>

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ghub::profiles {

namespace {

constexpr auto serviceName = QLatin1StringView{"io.github.hubless.GHubLinux"};
constexpr auto objectPath = QLatin1StringView{"/ActiveWindow"};
constexpr auto pluginName = QLatin1StringView{"ghub-linux-active-window"};
constexpr auto kwinService = QLatin1StringView{"org.kde.KWin"};
constexpr auto kwinScriptingPath = QLatin1StringView{"/Scripting"};
constexpr auto kwinScriptingInterface = QLatin1StringView{"org.kde.kwin.Scripting"};
constexpr auto kwinScriptInterface = QLatin1StringView{"org.kde.kwin.Script"};
constexpr qsizetype maximumFieldBytes = 256;

// Script constante carregado no KWin: só lê resourceClass e desktopFileName (nunca o título) e só chama o nosso
// serviço. Nada de `caption`.
constexpr auto scriptSource = QLatin1StringView{
    R"JS(function ghubSend(w) { if (!w) return; callDBus("io.github.hubless.GHubLinux", "/ActiveWindow", "io.github.hubless.ActiveWindow", "Activated", String(w.resourceClass || ""), String(w.desktopFileName || "")); }
workspace.windowActivated.connect(ghubSend);
ghubSend(workspace.activeWindow);
)JS"};

shared_kernel::DomainError unavailable(const QString& reason)
{
    return {.code = shared_kernel::ErrorCode::BackendUnavailable, .detail = reason.toStdString()};
}

} // namespace

// Objeto D-Bus de recebimento (interface io.github.hubless.ActiveWindow) + estado da fonte.
class KWinActiveApplicationSource::Impl final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.hubless.ActiveWindow")

public:
    Impl(QDBusConnection connection, QString runtimeDir, std::chrono::milliseconds deadline)
        : connection_(std::move(connection)), runtimeDir_(std::move(runtimeDir)), deadline_(deadline)
    {
    }

    ~Impl() override { stop(); }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    shared_kernel::Result<void> start(ActivationListener onActivated, StatusListener onStatus)
    {
        if (started_) {
            return {};
        }
        if (runtimeDir_.isEmpty()) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "XDG_RUNTIME_DIR não definido");
        }
        if (!connection_.isConnected()) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                       "barramento de sessão indisponível");
        }
        if (const auto written = writeScript(); !written) {
            return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, written.error());
        }
        activated_ = std::move(onActivated);
        status_ = std::move(onStatus);
        if (!connection_.registerObject(objectPath, this, QDBusConnection::ExportScriptableSlots)) {
            return abortStart(QStringLiteral("não foi possível registrar o objeto D-Bus"), false, false);
        }
        if (!connection_.registerService(serviceName)) {
            return abortStart(QStringLiteral("não foi possível registrar o serviço D-Bus"), true, false);
        }
        if (const auto loaded = loadAndRun(); !loaded) {
            return abortStart(loaded.error(), true, true);
        }
        watcher_ = std::make_unique<QDBusServiceWatcher>(QString(kwinService), connection_,
                                                         QDBusServiceWatcher::WatchForOwnerChange);
        connect(watcher_.get(), &QDBusServiceWatcher::serviceOwnerChanged, this, &Impl::kwinOwnerChanged);
        started_ = true;
        return {};
    }

    void stop() noexcept
    {
        if (!started_) {
            return;
        }
        started_ = false;
        watcher_.reset();
        // Resultado ignorado: se o KWin já saiu, não há o que descarregar.
        (void)callKwin(QString(kwinScriptingPath), kwinScriptingInterface, QStringLiteral("unloadScript"),
                       {QString(pluginName)});
        connection_.unregisterObject(objectPath);
        connection_.unregisterService(serviceName);
        activated_ = nullptr;
        status_ = nullptr;
    }

public Q_SLOTS:
    Q_SCRIPTABLE void Activated(const QString& resourceClass, const QString& desktopFileName)
    {
        if (!started_ || !activated_) {
            return;
        }
        if (const auto id = pick(desktopFileName)) {
            activated_(*id);
        } else if (const auto fallback = pick(resourceClass)) {
            activated_(*fallback);
        }
    }

private:
    // Resposta do KWin ou o motivo da falha.
    struct CallOutcome {
        bool ok = false;
        QVariant value;
        QString error;
    };

    static std::optional<ApplicationId> pick(const QString& raw)
    {
        const QByteArray bytes = raw.toUtf8();
        if (bytes.size() > maximumFieldBytes) {
            return std::nullopt;
        }
        auto id = ApplicationId::create(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
        if (!id) {
            return std::nullopt;
        }
        return *id;
    }

    CallOutcome callKwin(const QString& path, QLatin1StringView interface, const QString& member,
                         const QVariantList& arguments)
    {
        auto message = QDBusMessage::createMethodCall(QString(kwinService), path, QString(interface), member);
        message.setArguments(arguments);
        const QDBusMessage reply = connection_.call(message, QDBus::Block, static_cast<int>(deadline_.count()));
        if (reply.type() == QDBusMessage::ErrorMessage) {
            return {.ok = false, .value = {}, .error = QStringLiteral("KWin: ") + reply.errorMessage()};
        }
        return {.ok = true, .value = reply.arguments().value(0), .error = {}};
    }

    using Outcome = std::expected<void, QString>;

    // unloadScript (resultado ignorado) -> loadScript -> run. Em falha parcial, descarrega o que carregou.
    Outcome loadAndRun()
    {
        (void)callKwin(QString(kwinScriptingPath), kwinScriptingInterface, QStringLiteral("unloadScript"),
                       {QString(pluginName)});
        const auto loaded = callKwin(QString(kwinScriptingPath), kwinScriptingInterface, QStringLiteral("loadScript"),
                                     {scriptPath(), QString(pluginName)});
        if (!loaded.ok) {
            return std::unexpected(loaded.error);
        }
        bool numeric = false;
        const int id = loaded.value.toInt(&numeric);
        if (!numeric || id < 0) {
            return std::unexpected(QStringLiteral("o KWin recusou carregar o script"));
        }
        const auto ran = callKwin(QStringLiteral("/Scripting/Script") + QString::number(id), kwinScriptInterface,
                                  QStringLiteral("run"), {});
        if (!ran.ok) {
            (void)callKwin(QString(kwinScriptingPath), kwinScriptingInterface, QStringLiteral("unloadScript"),
                           {QString(pluginName)});
            return std::unexpected(ran.error);
        }
        return {};
    }

    shared_kernel::Result<void> abortStart(const QString& reason, bool objectRegistered, bool serviceRegistered)
    {
        if (serviceRegistered) {
            connection_.unregisterService(serviceName);
        }
        if (objectRegistered) {
            connection_.unregisterObject(objectPath);
        }
        activated_ = nullptr;
        status_ = nullptr;
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, reason.toStdString());
    }

    [[nodiscard]] QString scriptPath() const
    {
        return QDir(runtimeDir_).filePath(QStringLiteral("ghub-linux/active-window.js"));
    }

    // Diretório 0700 e arquivo 0600, sobrescrito a cada início.
    std::expected<void, std::string> writeScript() const
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path dir = fs::path(runtimeDir_.toStdString()) / "ghub-linux";
        fs::create_directories(dir, ec);
        if (ec) {
            return std::unexpected(std::string("não foi possível criar o diretório do script"));
        }
        fs::permissions(dir, fs::perms::owner_all, fs::perm_options::replace, ec);
        if (ec) {
            return std::unexpected(std::string("não foi possível restringir o diretório do script"));
        }
        QFile file(scriptPath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return std::unexpected(std::string("não foi possível gravar o script"));
        }
        if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            return std::unexpected(std::string("não foi possível restringir o arquivo do script"));
        }
        const QByteArray content = QByteArray(scriptSource.data(), scriptSource.size());
        if (file.write(content) != content.size() || !file.flush()) {
            return std::unexpected(std::string("não foi possível gravar o script"));
        }
        return {};
    }

    void kwinOwnerChanged(const QString& /*service*/, const QString& /*oldOwner*/, const QString& newOwner)
    {
        if (!started_) {
            return;
        }
        if (newOwner.isEmpty()) {
            notify(unavailable(QStringLiteral("KWin saiu")));
            return;
        }
        const auto reloaded = loadAndRun();
        notify(reloaded ? std::optional<shared_kernel::DomainError>{} : std::optional{unavailable(reloaded.error())});
    }

    void notify(std::optional<shared_kernel::DomainError> status) const
    {
        if (status_) {
            status_(std::move(status));
        }
    }

    QDBusConnection connection_;
    QString runtimeDir_;
    std::chrono::milliseconds deadline_;
    bool started_ = false;
    ActivationListener activated_;
    StatusListener status_;
    std::unique_ptr<QDBusServiceWatcher> watcher_;
};

KWinActiveApplicationSource::KWinActiveApplicationSource(QDBusConnection connection, QString runtimeDir,
                                                         std::chrono::milliseconds deadline)
    : impl_(std::make_unique<Impl>(std::move(connection), std::move(runtimeDir), deadline))
{
}

KWinActiveApplicationSource::~KWinActiveApplicationSource() = default;

shared_kernel::Result<void> KWinActiveApplicationSource::start(ActivationListener onActivated, StatusListener onStatus)
{
    return impl_->start(std::move(onActivated), std::move(onStatus));
}

void KWinActiveApplicationSource::stop() noexcept
{
    impl_->stop();
}

} // namespace ghub::profiles

#include "KWinActiveApplicationSource.moc"
