#include "LogindSleepMonitor.hpp"

#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>

#include <fcntl.h>
#include <unistd.h>

namespace ghub::device_access {

namespace {

constexpr QLatin1StringView service{"org.freedesktop.login1"};
constexpr QLatin1StringView path{"/org/freedesktop/login1"};
constexpr QLatin1StringView interface{"org.freedesktop.login1.Manager"};
constexpr int callTimeoutMs = 5000;

} // namespace

LogindSleepMonitor::LogindSleepMonitor(QDBusConnection bus, QObject* parent) : QObject(parent), bus_(std::move(bus)) {}

LogindSleepMonitor::~LogindSleepMonitor()
{
    releaseInhibitor();
}

bool LogindSleepMonitor::start()
{
    lastError_.clear();
    if (!bus_.isConnected()) {
        lastError_ = QStringLiteral("sem conexão com o barramento D-Bus");
        return false;
    }
    if (!bus_.connect(service, path, interface, QStringLiteral("PrepareForSleep"), this,
                      SLOT(onPrepareForSleep(bool)))) {
        lastError_ = QStringLiteral("não foi possível conectar ao sinal PrepareForSleep");
        return false;
    }
    return acquireInhibitor();
}

bool LogindSleepMonitor::holdsInhibitor() const
{
    return inhibitorFd_ >= 0;
}

QString LogindSleepMonitor::lastError() const
{
    return lastError_;
}

void LogindSleepMonitor::onPrepareForSleep(bool start)
{
    if (start) {
        // Os handlers rodam com o lock ainda seguro; só depois o logind é liberado para suspender.
        emit aboutToSleep();
        releaseInhibitor();
        return;
    }
    // Retomada: o lock anterior foi solto; pede outro para a próxima suspensão (melhor esforço).
    static_cast<void>(acquireInhibitor());
    emit resumed();
}

bool LogindSleepMonitor::acquireInhibitor()
{
    releaseInhibitor();
    QDBusMessage call = QDBusMessage::createMethodCall(service, path, interface, QStringLiteral("Inhibit"));
    call << QStringLiteral("sleep") << QStringLiteral("ghub-linux")
         << QStringLiteral("devolver o controle dos dispositivos antes de suspender") << QStringLiteral("delay");
    const QDBusReply<QDBusUnixFileDescriptor> reply = bus_.call(call, QDBus::Block, callTimeoutMs);
    if (!reply.isValid()) {
        lastError_ = reply.error().name() + QStringLiteral(": ") + reply.error().message();
        return false;
    }
    // Duplicata própria: o descritor da resposta é fechado quando a mensagem some. CLOEXEC: processos filhos
    // (ex.: headsetcontrol) não podem herdar o lock e atrasar a suspensão.
    const int duplicate = ::fcntl(reply.value().fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (duplicate < 0) {
        lastError_ = QStringLiteral("falha ao duplicar o descritor do inhibitor");
        return false;
    }
    inhibitorFd_ = duplicate;
    return true;
}

void LogindSleepMonitor::releaseInhibitor()
{
    if (inhibitorFd_ >= 0) {
        ::close(inhibitorFd_);
        inhibitorFd_ = -1;
    }
}

} // namespace ghub::device_access
