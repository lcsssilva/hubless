#pragma once

// Serviço `org.freedesktop.login1` falso para testes: roda numa thread própria (o adaptador faz chamadas
// bloqueantes) num dbus-daemon privado. `Inhibit` devolve a ponta de escrita de um pipe; o teste fica com a
// ponta de leitura e detecta a liberação do lock vendo EOF/POLLHUP. Registra cada chamada e emite
// `PrepareForSleep` sob demanda. Nunca fala com o logind real.
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QDBusVirtualObject>
#include <QList>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QThread>

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <poll.h>
#include <unistd.h>

namespace ghub::fakes {

inline constexpr QLatin1StringView logindService{"org.freedesktop.login1"};
inline constexpr QLatin1StringView logindPath{"/org/freedesktop/login1"};
inline constexpr QLatin1StringView logindInterface{"org.freedesktop.login1.Manager"};

// dbus-daemon privado (sessão); o endereço vem da primeira linha da saída.
class PrivateBus {
public:
    PrivateBus()
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

    ~PrivateBus()
    {
        process_.terminate();
        constexpr int deadlineMs = 10000;
        if (!process_.waitForFinished(deadlineMs)) {
            process_.kill();
            process_.waitForFinished(deadlineMs);
        }
    }

    PrivateBus(const PrivateBus&) = delete;
    PrivateBus& operator=(const PrivateBus&) = delete;
    PrivateBus(PrivateBus&&) = delete;
    PrivateBus& operator=(PrivateBus&&) = delete;

    [[nodiscard]] const QString& address() const { return address_; }

private:
    QProcess process_;
    QString address_;
};

// Ponta de leitura de um pipe: vê POLLHUP quando todas as pontas de escrita foram fechadas.
class PipeReader {
public:
    explicit PipeReader(int fd) : fd_(fd) {}
    ~PipeReader()
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    PipeReader(const PipeReader&) = delete;
    PipeReader& operator=(const PipeReader&) = delete;
    PipeReader(PipeReader&&) = delete;
    PipeReader& operator=(PipeReader&&) = delete;

    [[nodiscard]] bool writerClosed() const
    {
        pollfd pfd{.fd = fd_, .events = POLLIN, .revents = 0};
        return ::poll(&pfd, 1, 0) > 0 && (static_cast<unsigned>(pfd.revents) & static_cast<unsigned>(POLLHUP)) != 0;
    }

private:
    int fd_;
};

class FakeLogindService {
public:
    // `failInhibit`: Inhibit responde com erro D-Bus.
    explicit FakeLogindService(const QString& address, bool failInhibit = false)
        : worker_(std::make_unique<Worker>(failInhibit))
    {
        worker_->moveToThread(&thread_);
        thread_.start();
        QMetaObject::invokeMethod(
            worker_.get(),
            [this, address] {
                connection_ = QDBusConnection::connectToBus(address, connectionName_);
                started_ = connection_.isConnected() && connection_.registerService(logindService) &&
                           connection_.registerVirtualObject(logindPath, worker_.get(), QDBusConnection::SubPath);
            },
            Qt::BlockingQueuedConnection);
    }

    ~FakeLogindService()
    {
        QMetaObject::invokeMethod(
            worker_.get(),
            [this] {
                connection_.unregisterObject(logindPath);
                connection_.unregisterService(logindService);
                connection_ = QDBusConnection(QString());
                QDBusConnection::disconnectFromBus(connectionName_);
            },
            Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
    }

    FakeLogindService(const FakeLogindService&) = delete;
    FakeLogindService& operator=(const FakeLogindService&) = delete;
    FakeLogindService(FakeLogindService&&) = delete;
    FakeLogindService& operator=(FakeLogindService&&) = delete;

    [[nodiscard]] bool started() const { return started_; }
    // "interface.membro" de cada chamada de método recebida, em ordem.
    [[nodiscard]] QStringList received() const { return worker_->received(); }
    // Argumentos (what, who, why, mode) de cada Inhibit, em ordem.
    [[nodiscard]] QList<QStringList> inhibitArguments() const { return worker_->inhibitArguments(); }
    // Quantidade de pipes entregues e leitura do n-ésimo (posse continua do fake).
    [[nodiscard]] int pipeCount() const { return worker_->pipeCount(); }
    [[nodiscard]] bool pipeWriterClosed(int index) const { return worker_->pipeWriterClosed(index); }

    void emitPrepareForSleep(bool start)
    {
        QMetaObject::invokeMethod(
            worker_.get(),
            [this, start] {
                auto signal =
                    QDBusMessage::createSignal(logindPath, logindInterface, QStringLiteral("PrepareForSleep"));
                signal << start;
                connection_.send(signal);
            },
            Qt::BlockingQueuedConnection);
    }

private:
    class Worker final : public QDBusVirtualObject {
    public:
        explicit Worker(bool failInhibit) : failInhibit_(failInhibit) {}

        [[nodiscard]] QString introspect(const QString& /*path*/) const override { return {}; }

        bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override
        {
            if (message.type() != QDBusMessage::MethodCallMessage) {
                return false;
            }
            const QMutexLocker lock(&mutex_);
            received_.push_back(message.interface() + QLatin1Char('.') + message.member());
            if (message.member() != QLatin1String("Inhibit")) {
                connection.send(message.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("não suportado")));
                return true;
            }
            QStringList args;
            for (const QVariant& arg : message.arguments()) {
                args.push_back(arg.toString());
            }
            inhibitArguments_.push_back(args);
            if (failInhibit_) {
                connection.send(message.createErrorReply(QDBusError::AccessDenied, QStringLiteral("negado")));
                return true;
            }
            std::array<int, 2> fds{-1, -1};
            if (::pipe(fds.data()) != 0) {
                connection.send(message.createErrorReply(QDBusError::Failed, QStringLiteral("pipe")));
                return true;
            }
            readers_.push_back(std::make_unique<PipeReader>(fds.at(0)));
            {
                // O QDBusUnixFileDescriptor duplica o fd; o original é fechado aqui, restando só a duplicata
                // dentro da resposta (e depois a do cliente).
                const QDBusUnixFileDescriptor writer(fds.at(1));
                ::close(fds.at(1));
                connection.send(message.createReply(QVariant::fromValue(writer)));
            }
            return true;
        }

        [[nodiscard]] QStringList received() const
        {
            const QMutexLocker lock(&mutex_);
            return received_;
        }
        [[nodiscard]] QList<QStringList> inhibitArguments() const
        {
            const QMutexLocker lock(&mutex_);
            return inhibitArguments_;
        }
        [[nodiscard]] int pipeCount() const
        {
            const QMutexLocker lock(&mutex_);
            return static_cast<int>(readers_.size());
        }
        [[nodiscard]] bool pipeWriterClosed(int index) const
        {
            const QMutexLocker lock(&mutex_);
            return index >= 0 && std::cmp_less(index, readers_.size()) &&
                   readers_[static_cast<std::size_t>(index)]->writerClosed();
        }

    private:
        bool failInhibit_;
        mutable QMutex mutex_;
        QStringList received_;
        QList<QStringList> inhibitArguments_;
        std::vector<std::unique_ptr<PipeReader>> readers_;
    };

    std::unique_ptr<Worker> worker_;
    QThread thread_;
    QDBusConnection connection_{QString()};
    QString connectionName_ = QStringLiteral("fake-login1-server");
    bool started_ = false;
};

} // namespace ghub::fakes
