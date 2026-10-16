#include "modules/lighting/infrastructure/QtDBusScreenCastPortal.hpp"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QEventLoop>
#include <QList>
#include <QObject>
#include <QRandomGenerator>
#include <QTimer>

#include <fcntl.h>

#include <optional>
#include <utility>

namespace ghub::lighting::infrastructure {

namespace {

constexpr auto kService = QLatin1StringView{"org.freedesktop.portal.Desktop"};
constexpr auto kPortalPath = QLatin1StringView{"/org/freedesktop/portal/desktop"};
constexpr auto kScreenCast = QLatin1StringView{"org.freedesktop.portal.ScreenCast"};
constexpr auto kRequest = QLatin1StringView{"org.freedesktop.portal.Request"};
constexpr auto kSession = QLatin1StringView{"org.freedesktop.portal.Session"};

// Bits de `types` e valores de `cursor_mode` da documentação do ScreenCast.
constexpr unsigned kSourceTypeMonitor = 1;
constexpr unsigned kCursorHidden = 1;

constexpr std::chrono::milliseconds kCancelPoll{100};

// Elemento de `streams` (a(ua{sv})): nó PipeWire e propriedades (ignoradas).
struct PortalStream {
    unsigned node = 0;
    QVariantMap properties;
};

QDBusArgument& operator<<(QDBusArgument& argument, const PortalStream& stream)
{
    argument.beginStructure();
    argument << stream.node << stream.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalStream& stream)
{
    argument.beginStructure();
    argument >> stream.node >> stream.properties;
    argument.endStructure();
    return argument;
}

void registerTypes()
{
    static const bool once = [] {
        qDBusRegisterMetaType<PortalStream>();
        qDBusRegisterMetaType<QList<PortalStream>>();
        return true;
    }();
    (void)once;
}

shared_kernel::DomainError unavailable(const QString& reason)
{
    return {.code = shared_kernel::ErrorCode::BackendUnavailable, .detail = reason.toStdString()};
}

// Espera o sinal Response de um pedido do portal.
class ResponseWaiter final : public QObject {
    Q_OBJECT

public:
    [[nodiscard]] bool received() const { return received_; }
    [[nodiscard]] unsigned code() const { return code_; }
    [[nodiscard]] const QVariantMap& results() const { return results_; }
    QEventLoop loop;

public slots:
    void onResponse(unsigned code, const QVariantMap& results)
    {
        received_ = true;
        code_ = code;
        results_ = results;
        loop.quit();
    }

private:
    bool received_ = false;
    unsigned code_ = 2;
    QVariantMap results_;
};

} // namespace

QtDBusScreenCastPortal::QtDBusScreenCastPortal(QDBusConnection connection, std::chrono::milliseconds callDeadline,
                                               std::chrono::milliseconds userDeadline)
    : connection_(std::move(connection)), callDeadline_(callDeadline), userDeadline_(userDeadline)
{
    registerTypes();
}

QString QtDBusScreenCastPortal::senderToken(const QString& uniqueName)
{
    QString token = uniqueName;
    if (token.startsWith(QLatin1Char(':'))) {
        token.remove(0, 1);
    }
    return token.replace(QLatin1Char('.'), QLatin1Char('_'));
}

QString QtDBusScreenCastPortal::requestPath(const QString& uniqueName, const QString& token)
{
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(senderToken(uniqueName), token);
}

QString QtDBusScreenCastPortal::newToken()
{
    return QStringLiteral("ghub_%1").arg(QRandomGenerator::system()->generate64(), 16, 16, QLatin1Char('0'));
}

shared_kernel::Result<std::uint32_t> QtDBusScreenCastPortal::firstStreamNode(const QVariantMap& results)
{
    registerTypes();
    const auto streams = qdbus_cast<QList<PortalStream>>(results.value(QStringLiteral("streams")));
    if (streams.isEmpty()) {
        return std::unexpected(unavailable(QStringLiteral("o portal não concedeu nenhum stream")));
    }
    return static_cast<std::uint32_t>(streams.first().node);
}

shared_kernel::Result<QVariantMap>
QtDBusScreenCastPortal::callAndAwait(const QString& method, const QVariantList& leading, QVariantMap options,
                                     std::chrono::milliseconds wait, const std::atomic<bool>& cancelled)
{
    if (!connection_.isConnected()) {
        return std::unexpected(unavailable(QStringLiteral("sem barramento D-Bus de sessão")));
    }
    const auto token = newToken();
    options.insert(QStringLiteral("handle_token"), token);
    const auto expected = requestPath(connection_.baseService(), token);

    ResponseWaiter waiter;
    if (!connection_.connect(QString(kService), expected, QString(kRequest), QStringLiteral("Response"), &waiter,
                             SLOT(onResponse(uint, QVariantMap)))) {
        return std::unexpected(unavailable(QStringLiteral("não foi possível assinar a resposta do portal")));
    }
    const auto unsubscribe = [&] {
        connection_.disconnect(QString(kService), expected, QString(kRequest), QStringLiteral("Response"), &waiter,
                               SLOT(onResponse(uint, QVariantMap)));
    };

    auto message =
        QDBusMessage::createMethodCall(QString(kService), QString(kPortalPath), QString(kScreenCast), method);
    QVariantList arguments = leading;
    arguments.append(options);
    message.setArguments(arguments);
    const auto reply = connection_.call(message, QDBus::Block, static_cast<int>(callDeadline_.count()));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        unsubscribe();
        return std::unexpected(unavailable(QStringLiteral("portal ScreenCast indisponível (%1)").arg(method)));
    }
    if (reply.arguments().constFirst().value<QDBusObjectPath>().path() != expected) {
        unsubscribe();
        return std::unexpected(unavailable(QStringLiteral("versão do portal sem caminho de requisição previsível")));
    }

    QTimer poll;
    QTimer deadline;
    deadline.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&poll, &QTimer::timeout, &waiter.loop, [&] {
        if (cancelled) {
            waiter.loop.quit();
        }
    });
    QObject::connect(&deadline, &QTimer::timeout, &waiter.loop, [&] {
        timedOut = true;
        waiter.loop.quit();
    });
    poll.start(static_cast<int>(kCancelPoll.count()));
    deadline.start(static_cast<int>(wait.count()));
    if (!cancelled) {
        waiter.loop.exec();
    }
    poll.stop();
    deadline.stop();
    unsubscribe();

    if (!waiter.received()) {
        // Pedido pendente (cancelado ou sem resposta): Request.Close encerra o diálogo, sem sinal Response.
        auto close =
            QDBusMessage::createMethodCall(QString(kService), expected, QString(kRequest), QStringLiteral("Close"));
        connection_.call(close, QDBus::NoBlock);
        return std::unexpected(
            unavailable(timedOut ? QStringLiteral("o portal não respondeu a tempo") : QStringLiteral("cancelado")));
    }
    switch (waiter.code()) {
    case 0:
        return waiter.results();
    case 1:
        return std::unexpected(unavailable(QStringLiteral("o usuário não permitiu o compartilhamento da tela")));
    default:
        break;
    }
    return std::unexpected(unavailable(QStringLiteral("o portal encerrou o pedido")));
}

shared_kernel::Result<std::string> QtDBusScreenCastPortal::createSession(const std::atomic<bool>& cancelled)
{
    QVariantMap options;
    options.insert(QStringLiteral("session_handle_token"), newToken());
    auto results = callAndAwait(QStringLiteral("CreateSession"), {}, std::move(options), callDeadline_, cancelled);
    if (!results.has_value()) {
        return std::unexpected(results.error());
    }
    const auto handle = results->value(QStringLiteral("session_handle")).toString();
    if (handle.isEmpty()) {
        return std::unexpected(unavailable(QStringLiteral("o portal não devolveu a sessão")));
    }
    return handle.toStdString();
}

shared_kernel::Result<void> QtDBusScreenCastPortal::selectSources(const std::string& session,
                                                                  const std::atomic<bool>& cancelled)
{
    QVariantMap options;
    options.insert(QStringLiteral("types"), kSourceTypeMonitor);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("cursor_mode"), kCursorHidden);
    // Sem persist_mode e sem restore_token: o consentimento é pedido a cada sessão.
    auto results = callAndAwait(QStringLiteral("SelectSources"),
                                {QVariant::fromValue(QDBusObjectPath{QString::fromStdString(session)})},
                                std::move(options), callDeadline_, cancelled);
    if (!results.has_value()) {
        return std::unexpected(results.error());
    }
    return {};
}

shared_kernel::Result<std::uint32_t> QtDBusScreenCastPortal::start(const std::string& session,
                                                                   const std::atomic<bool>& cancelled)
{
    auto results = callAndAwait(QStringLiteral("Start"),
                                {QVariant::fromValue(QDBusObjectPath{QString::fromStdString(session)}), QString{}}, {},
                                userDeadline_, cancelled);
    if (!results.has_value()) {
        return std::unexpected(results.error());
    }
    return firstStreamNode(*results); // o restore_token eventual da resposta é descartado aqui
}

shared_kernel::Result<ScreenFd> QtDBusScreenCastPortal::openPipeWireRemote(const std::string& session)
{
    auto message = QDBusMessage::createMethodCall(QString(kService), QString(kPortalPath), QString(kScreenCast),
                                                  QStringLiteral("OpenPipeWireRemote"));
    message.setArguments({QVariant::fromValue(QDBusObjectPath{QString::fromStdString(session)}), QVariantMap{}});
    const auto reply = connection_.call(message, QDBus::Block, static_cast<int>(callDeadline_.count()));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return std::unexpected(unavailable(QStringLiteral("o portal não abriu a conexão PipeWire")));
    }
    const auto descriptor = reply.arguments().constFirst().value<QDBusUnixFileDescriptor>();
    if (!descriptor.isValid()) {
        return std::unexpected(unavailable(QStringLiteral("descritor do PipeWire inválido")));
    }
    // O QDBusUnixFileDescriptor mantém o seu; o fluxo recebe uma cópia própria.
    const int copy = ::fcntl(descriptor.fileDescriptor(), F_DUPFD_CLOEXEC, 3);
    if (copy < 0) {
        return std::unexpected(unavailable(QStringLiteral("não foi possível duplicar o descritor do PipeWire")));
    }
    return ScreenFd{copy};
}

void QtDBusScreenCastPortal::closeSession(const std::string& session) noexcept
{
    try {
        auto message = QDBusMessage::createMethodCall(QString(kService), QString::fromStdString(session),
                                                      QString(kSession), QStringLiteral("Close"));
        connection_.call(message, QDBus::Block, static_cast<int>(callDeadline_.count()));
    } catch (...) {
        // melhor esforço: a sessão também acaba quando este processo sai do barramento
    }
}

} // namespace ghub::lighting::infrastructure

#include "QtDBusScreenCastPortal.moc"
