#pragma once

#include <QObject>
#include <QSocketNotifier>

#include <array>
#include <csignal>
#include <memory>

namespace ghub::app {

// Converte SIGTERM, SIGINT e SIGHUP em um sinal Qt entregue na thread do objeto (self-pipe), para que o
// encerramento pedido pelo sistema passe por QCoreApplication::quit() e rode os destrutores. Sem isso o processo
// morria sem devolver o controle do G915 ao firmware (teste físico T2, 2026-10-14). SIGKILL não é tratável.
// Uma instância ativa por processo: a segunda fica inativa e não altera nada. O destrutor restaura as
// disposições anteriores.
class TerminationSignals final : public QObject {
    Q_OBJECT

public:
    explicit TerminationSignals(QObject* parent = nullptr);
    ~TerminationSignals() override;
    TerminationSignals(const TerminationSignals&) = delete;
    TerminationSignals& operator=(const TerminationSignals&) = delete;
    TerminationSignals(TerminationSignals&&) = delete;
    TerminationSignals& operator=(TerminationSignals&&) = delete;

    // false quando outra instância já está ativa ou o pipe/sigaction falhou.
    [[nodiscard]] bool isActive() const noexcept { return notifier_ != nullptr; }

signals:
    void terminationRequested(int signalNumber);

private:
    void drain();

    int readFd_ = -1;
    int writeFd_ = -1;
    std::unique_ptr<QSocketNotifier> notifier_; // nulo quando inativa
    std::array<struct sigaction, 3> previous_{};
};

} // namespace ghub::app
