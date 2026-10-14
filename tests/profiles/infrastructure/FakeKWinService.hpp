#pragma once

// KWin falso para testes: dbus-daemon privado (sem ativação de serviços) e um `org.kde.KWin` falso que roda numa
// thread própria (o adaptador faz chamadas bloqueantes). Atende `/Scripting` (org.kde.kwin.Scripting) e
// `/Scripting/Script<n>` (org.kde.kwin.Script), registra cada chamada e pode recusar, atrasar ou sumir.
// Nunca fala com o KWin nem com o barramento de sessão reais.
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QFile>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <memory>

namespace ghub::fakes {

inline constexpr QLatin1StringView kwinService{"org.kde.KWin"};
inline constexpr QLatin1StringView kwinScriptingPath{"/Scripting"};

// dbus-daemon privado, com configuração própria: sem diretórios de serviços (nada é ativado sob demanda).
class FakeKWinBus {
public:
    FakeKWinBus()
    {
        if (!dir_.isValid()) {
            return;
        }
        const QString config = dir_.filePath(QStringLiteral("bus.conf"));
        QFile file(config);
        if (!file.open(QIODevice::WriteOnly)) {
            return;
        }
        file.write("<!DOCTYPE busconfig PUBLIC \"-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN\" "
                   "\"http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd\">\n"
                   "<busconfig><type>session</type><listen>unix:tmpdir=");
        file.write(dir_.path().toUtf8());
        file.write("</listen><auth>EXTERNAL</auth>"
                   "<policy context=\"default\"><allow send_destination=\"*\" eavesdrop=\"true\"/>"
                   "<allow eavesdrop=\"true\"/><allow own=\"*\"/></policy></busconfig>\n");
        file.close();
        process_.setProgram(QStringLiteral("dbus-daemon"));
        process_.setArguments({QStringLiteral("--config-file=") + config, QStringLiteral("--nofork"),
                               QStringLiteral("--print-address=1")});
        process_.start();
        constexpr int deadlineMs = 10000;
        if (process_.waitForStarted(deadlineMs) && process_.waitForReadyRead(deadlineMs)) {
            address_ = QString::fromUtf8(process_.readLine()).trimmed();
        }
    }

    ~FakeKWinBus()
    {
        process_.terminate();
        constexpr int deadlineMs = 10000;
        if (!process_.waitForFinished(deadlineMs)) {
            process_.kill();
            process_.waitForFinished(deadlineMs);
        }
    }

    FakeKWinBus(const FakeKWinBus&) = delete;
    FakeKWinBus& operator=(const FakeKWinBus&) = delete;
    FakeKWinBus(FakeKWinBus&&) = delete;
    FakeKWinBus& operator=(FakeKWinBus&&) = delete;

    [[nodiscard]] const QString& address() const { return address_; }

private:
    QTemporaryDir dir_;
    QProcess process_;
    QString address_;
};

class FakeKWinService {
public:
    // `appear`: registra o nome já no construtor.
    explicit FakeKWinService(const QString& address, bool appear = true) : worker_(std::make_unique<Worker>())
    {
        worker_->moveToThread(&thread_);
        thread_.start();
        QMetaObject::invokeMethod(
            worker_.get(),
            [this, address, appear] {
                connection_ = QDBusConnection::connectToBus(address, connectionName_);
                started_ =
                    connection_.isConnected() &&
                    connection_.registerVirtualObject(kwinScriptingPath, worker_.get(), QDBusConnection::SubPath) &&
                    (!appear || connection_.registerService(kwinService));
            },
            Qt::BlockingQueuedConnection);
    }

    ~FakeKWinService()
    {
        QMetaObject::invokeMethod(
            worker_.get(),
            [this] {
                connection_.unregisterObject(kwinScriptingPath);
                connection_.unregisterService(kwinService);
                connection_ = QDBusConnection(QString());
                QDBusConnection::disconnectFromBus(connectionName_);
            },
            Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
    }

    FakeKWinService(const FakeKWinService&) = delete;
    FakeKWinService& operator=(const FakeKWinService&) = delete;
    FakeKWinService(FakeKWinService&&) = delete;
    FakeKWinService& operator=(FakeKWinService&&) = delete;

    [[nodiscard]] bool started() const { return started_; }
    // Uma entrada por chamada recebida, em ordem: "unloadScript|plugin", "loadScript|caminho|plugin",
    // "run|/Scripting/Script<n>" ou "interface.membro" para qualquer outra.
    [[nodiscard]] QStringList calls() const { return worker_->calls(); }
    void clearCalls() { worker_->clearCalls(); }
    // Identificador devolvido por loadScript (-1 = recusa).
    void setLoadResult(int id) { worker_->loadResult = id; }
    // Atraso aplicado a toda chamada (para estourar o prazo do cliente).
    void setDelayMs(int ms) { worker_->delayMs = ms; }

    void disappear()
    {
        QMetaObject::invokeMethod(
            worker_.get(), [this] { connection_.unregisterService(kwinService); }, Qt::BlockingQueuedConnection);
    }
    void reappear()
    {
        QMetaObject::invokeMethod(
            worker_.get(), [this] { connection_.registerService(kwinService); }, Qt::BlockingQueuedConnection);
    }

private:
    class Worker final : public QDBusVirtualObject {
    public:
        std::atomic<int> loadResult{7};
        std::atomic<int> delayMs{0};

        [[nodiscard]] QString introspect(const QString& /*path*/) const override { return {}; }

        bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override
        {
            if (message.type() != QDBusMessage::MethodCallMessage) {
                return false;
            }
            if (delayMs > 0) {
                QThread::msleep(static_cast<unsigned long>(delayMs.load()));
            }
            const QString member = message.member();
            QVariant result;
            bool hasResult = false;
            QString entry;
            if (message.path() == QLatin1String("/Scripting") &&
                message.interface() == QLatin1String("org.kde.kwin.Scripting") &&
                member == QLatin1String("unloadScript")) {
                entry = QStringLiteral("unloadScript|") + message.arguments().value(0).toString();
                result = true;
                hasResult = true;
            } else if (message.path() == QLatin1String("/Scripting") &&
                       message.interface() == QLatin1String("org.kde.kwin.Scripting") &&
                       member == QLatin1String("loadScript")) {
                entry = QStringLiteral("loadScript|") + message.arguments().value(0).toString() + QLatin1Char('|') +
                        message.arguments().value(1).toString();
                result = loadResult.load();
                hasResult = true;
            } else if (message.path().startsWith(QLatin1String("/Scripting/Script")) &&
                       message.interface() == QLatin1String("org.kde.kwin.Script") && member == QLatin1String("run")) {
                entry = QStringLiteral("run|") + message.path();
            } else {
                entry = message.interface() + QLatin1Char('.') + member;
                const QMutexLocker lock(&mutex_);
                calls_.push_back(entry);
                connection.send(message.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("não suportado")));
                return true;
            }
            {
                const QMutexLocker lock(&mutex_);
                calls_.push_back(entry);
            }
            connection.send(hasResult ? message.createReply(result) : message.createReply());
            return true;
        }

        [[nodiscard]] QStringList calls() const
        {
            const QMutexLocker lock(&mutex_);
            return calls_;
        }
        void clearCalls()
        {
            const QMutexLocker lock(&mutex_);
            calls_.clear();
        }

    private:
        mutable QMutex mutex_;
        QStringList calls_;
    };

    std::unique_ptr<Worker> worker_;
    QThread thread_;
    QDBusConnection connection_{QString()};
    QString connectionName_ = QStringLiteral("fake-kwin-server");
    bool started_ = false;
};

} // namespace ghub::fakes
