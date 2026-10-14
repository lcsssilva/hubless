#pragma once

// Serviço `org.freedesktop.ratbag1` falso para testes: roda numa thread própria (o cliente faz chamadas
// bloqueantes) num dbus-daemon privado, responde só a Properties.Get/GetAll e registra toda mensagem de
// método recebida (interface + membro), para provar que o adaptador nunca envia outra coisa.
#include <QByteArray>
#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QList>
#include <QMap>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

#include <memory>
#include <string>

namespace ghub::fakes {

// caminho do objeto -> interface -> propriedades
using RatbagTree = QMap<QString, QMap<QString, QVariantMap>>;

inline constexpr QLatin1StringView ratbagService{"org.freedesktop.ratbag1"};
inline constexpr QLatin1StringView ratbagRoot{"/org/freedesktop/ratbag1"};

// dbus-daemon privado (sessão) iniciado pelo teste; o endereço vem da primeira linha da saída.
class PrivateDbusDaemon {
public:
    PrivateDbusDaemon()
    {
        process_.setProgram(QStringLiteral("dbus-daemon"));
        process_.setArguments(
            {QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
        process_.start();
        constexpr int deadlineMs = 10000;
        if (process_.waitForStarted(deadlineMs) && process_.waitForReadyRead(deadlineMs)) {
            address_ = QString::fromUtf8(process_.readLine()).trimmed();
        }
    }

    ~PrivateDbusDaemon()
    {
        process_.terminate();
        constexpr int deadlineMs = 10000;
        if (!process_.waitForFinished(deadlineMs)) {
            process_.kill();
            process_.waitForFinished(deadlineMs);
        }
    }

    PrivateDbusDaemon(const PrivateDbusDaemon&) = delete;
    PrivateDbusDaemon& operator=(const PrivateDbusDaemon&) = delete;
    PrivateDbusDaemon(PrivateDbusDaemon&&) = delete;
    PrivateDbusDaemon& operator=(PrivateDbusDaemon&&) = delete;

    [[nodiscard]] const QString& address() const { return address_; }

private:
    QProcess process_;
    QString address_;
};

// Construtores de valores D-Bus para a árvore falsa.
namespace dbusval {

inline QVariant variant(const QVariant& inner)
{
    return QVariant::fromValue(QDBusVariant(inner));
}

inline QVariant uintArray(const QList<uint>& values)
{
    return QVariant::fromValue(values);
}

inline QVariant pathArray(const QStringList& paths)
{
    QList<QDBusObjectPath> list;
    for (const QString& path : paths) {
        list.push_back(QDBusObjectPath(path));
    }
    return QVariant::fromValue(list);
}

// Estruturas D-Bus como tipos próprios (com operator<<) registrados no QtDBus.
struct UintPair {
    uint first;
    uint second;
};

struct UintTriple {
    uint first;
    uint second;
    uint third;
};

struct Mapping {
    uint type{};
    QDBusVariant content;
};

struct MacroStep {
    uint kind;
    uint value;
};

inline QDBusArgument& operator<<(QDBusArgument& arg, const UintPair& v)
{
    arg.beginStructure();
    arg << v.first << v.second;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline const QDBusArgument& operator>>(const QDBusArgument& arg, UintPair& v)
{
    arg.beginStructure();
    arg >> v.first >> v.second;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline QDBusArgument& operator<<(QDBusArgument& arg, const UintTriple& v)
{
    arg.beginStructure();
    arg << v.first << v.second << v.third;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline const QDBusArgument& operator>>(const QDBusArgument& arg, UintTriple& v)
{
    arg.beginStructure();
    arg >> v.first >> v.second >> v.third;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline QDBusArgument& operator<<(QDBusArgument& arg, const Mapping& v)
{
    arg.beginStructure();
    arg << v.type << v.content;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline const QDBusArgument& operator>>(const QDBusArgument& arg, Mapping& v)
{
    arg.beginStructure();
    arg >> v.type >> v.content;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline QDBusArgument& operator<<(QDBusArgument& arg, const MacroStep& v)
{
    arg.beginStructure();
    arg << v.kind << v.value;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline const QDBusArgument& operator>>(const QDBusArgument& arg, MacroStep& v)
{
    arg.beginStructure();
    arg >> v.kind >> v.value;
    arg.endStructure();
    return arg; // NOLINT(bugprone-return-const-ref-from-parameter): idioma do QtDBus (encadeamento de << e >>)
}

inline void registerStructs()
{
    static const bool done = [] {
        qDBusRegisterMetaType<UintPair>();
        qDBusRegisterMetaType<UintTriple>();
        qDBusRegisterMetaType<Mapping>();
        qDBusRegisterMetaType<MacroStep>();
        qDBusRegisterMetaType<QList<MacroStep>>();
        return true;
    }();
    (void)done;
}

inline QVariant uintPair(uint first, uint second)
{
    registerStructs();
    return QVariant::fromValue(UintPair{.first = first, .second = second});
}

inline QVariant uintTriple(uint first, uint second, uint third)
{
    registerStructs();
    return QVariant::fromValue(UintTriple{.first = first, .second = second, .third = third});
}

// Mapping `(uv)`.
inline QVariant mapping(uint type, const QVariant& content)
{
    registerStructs();
    return QVariant::fromValue(Mapping{.type = type, .content = QDBusVariant(content)});
}

// Macro `a(uu)` com `steps` passos.
inline QVariant macroSteps(int steps)
{
    registerStructs();
    QList<MacroStep> list;
    for (int i = 0; i < steps; ++i) {
        list.push_back(MacroStep{.kind = 1U, .value = static_cast<uint>(i)});
    }
    return QVariant::fromValue(list);
}

} // namespace dbusval

class FakeRatbagService {
public:
    // `silent`: recebe as chamadas e não responde (prazo esgotado no cliente).
    FakeRatbagService(const QString& address, RatbagTree tree, bool silent = false)
        : worker_(std::make_unique<Worker>(std::move(tree), silent))
    {
        worker_->moveToThread(&thread_);
        thread_.start();
        const QString connectionName = QStringLiteral("fake-ratbag-server");
        QMetaObject::invokeMethod(
            worker_.get(),
            [this, address, connectionName] {
                connectionName_ = connectionName;
                connection_ = QDBusConnection::connectToBus(address, connectionName);
                started_ = connection_.isConnected() && connection_.registerService(ratbagService) &&
                           connection_.registerVirtualObject(ratbagRoot, worker_.get(), QDBusConnection::SubPath);
            },
            Qt::BlockingQueuedConnection);
    }

    ~FakeRatbagService()
    {
        QMetaObject::invokeMethod(
            worker_.get(),
            [this] {
                connection_.unregisterObject(ratbagRoot);
                connection_.unregisterService(ratbagService);
                connection_ = QDBusConnection(QString());
                QDBusConnection::disconnectFromBus(connectionName_);
            },
            Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
    }

    FakeRatbagService(const FakeRatbagService&) = delete;
    FakeRatbagService& operator=(const FakeRatbagService&) = delete;
    FakeRatbagService(FakeRatbagService&&) = delete;
    FakeRatbagService& operator=(FakeRatbagService&&) = delete;

    [[nodiscard]] bool started() const { return started_; }
    // "interface.membro" de cada chamada de método recebida, em ordem.
    [[nodiscard]] QStringList received() const { return worker_->received(); }

private:
    class Worker final : public QDBusVirtualObject {
    public:
        Worker(RatbagTree tree, bool silent) : tree_(std::move(tree)), silent_(silent) {}

        [[nodiscard]] QString introspect(const QString& /*path*/) const override { return {}; }

        bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override
        {
            if (message.type() != QDBusMessage::MethodCallMessage) {
                return false;
            }
            {
                const QMutexLocker lock(&mutex_);
                received_.push_back(message.interface() + QLatin1Char('.') + message.member());
            }
            if (silent_) {
                return true;
            }
            connection.send(answer(message));
            return true;
        }

        [[nodiscard]] QStringList received() const
        {
            const QMutexLocker lock(&mutex_);
            return received_;
        }

    private:
        [[nodiscard]] QDBusMessage answer(const QDBusMessage& message) const
        {
            if (message.interface() != QLatin1String("org.freedesktop.DBus.Properties")) {
                return message.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("método não suportado"));
            }
            const auto object = tree_.constFind(message.path());
            if (object == tree_.cend() || message.arguments().isEmpty()) {
                return message.createErrorReply(QDBusError::UnknownObject, message.path());
            }
            const auto interface = object->constFind(message.arguments().at(0).toString());
            if (interface == object->cend()) {
                return message.createErrorReply(QDBusError::UnknownInterface, message.path());
            }
            if (message.member() == QLatin1String("GetAll")) {
                return message.createReply(QVariant(*interface));
            }
            const auto property = interface->constFind(message.arguments().value(1).toString());
            if (message.member() != QLatin1String("Get") || property == interface->cend()) {
                return message.createErrorReply(QDBusError::UnknownProperty, message.member());
            }
            return message.createReply(QVariant::fromValue(QDBusVariant(*property)));
        }

        RatbagTree tree_;
        bool silent_;
        mutable QMutex mutex_;
        QStringList received_;
    };

    std::unique_ptr<Worker> worker_;
    QThread thread_;
    QDBusConnection connection_{QString()};
    QString connectionName_;
    bool started_ = false;
};

} // namespace ghub::fakes
