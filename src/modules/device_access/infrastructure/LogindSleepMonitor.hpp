#pragma once

// Monitor de suspensão via logind (ADR-0014 item 3, INC-008 R16): pega um inhibitor "delay" de sleep e, ao
// receber PrepareForSleep(true), avisa os interessados (síncrono) ANTES de soltar o lock; ao retomar, pede o
// inhibitor de novo. Única chamada ao logind: Inhibit. Construir não faz chamada D-Bus.
#include <QDBusConnection>
#include <QObject>
#include <QString>

namespace ghub::device_access {

class LogindSleepMonitor final : public QObject {
    Q_OBJECT

public:
    explicit LogindSleepMonitor(QDBusConnection bus, QObject* parent = nullptr);
    ~LogindSleepMonitor() override;

    LogindSleepMonitor(const LogindSleepMonitor&) = delete;
    LogindSleepMonitor& operator=(const LogindSleepMonitor&) = delete;
    LogindSleepMonitor(LogindSleepMonitor&&) = delete;
    LogindSleepMonitor& operator=(LogindSleepMonitor&&) = delete;

    // Conecta ao sinal e pede o inhibitor. Falha (sem lançar) se o sinal não conectar ou o Inhibit falhar.
    [[nodiscard]] bool start();
    [[nodiscard]] bool holdsInhibitor() const;
    [[nodiscard]] QString lastError() const;

signals:
    // Emitido de forma síncrona com o inhibitor ainda seguro; o lock só é solto depois dos handlers.
    void aboutToSleep();
    void resumed();

private slots:
    void onPrepareForSleep(bool start);

private: // NOLINT(readability-redundant-access-specifiers): `private slots` não aceita membros comuns no moc
    bool acquireInhibitor();
    void releaseInhibitor();

    QDBusConnection bus_;
    int inhibitorFd_ = -1;
    QString lastError_;
};

} // namespace ghub::device_access
