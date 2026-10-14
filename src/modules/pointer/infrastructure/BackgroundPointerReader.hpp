#pragma once

#include "modules/pointer/application/PointerReadRunner.hpp"
#include "modules/pointer/application/ReadPointerConfiguration.hpp"

#include <QFutureWatcher>
#include <QObject>

#include <optional>

namespace ghub::pointer {

// Leitura do mouse fora da thread de UI (ADR-0011 §4). `start()` executa o caso de uso numa thread de
// trabalho (QtConcurrent), uma leitura por vez e sem timer; ao terminar, guarda o resultado e emite
// readFinished(). O destrutor espera a leitura em andamento. O caso de uso (e seus colaboradores) deve
// sobreviver a este objeto. Use sempre a partir da thread que o possui.
class BackgroundPointerReader final : public QObject, public PointerReadRunner {
    Q_OBJECT

public:
    explicit BackgroundPointerReader(const ReadPointerConfiguration& useCase, QObject* parent = nullptr);
    ~BackgroundPointerReader() override;

    BackgroundPointerReader(const BackgroundPointerReader&) = delete;
    BackgroundPointerReader& operator=(const BackgroundPointerReader&) = delete;
    BackgroundPointerReader(BackgroundPointerReader&&) = delete;
    BackgroundPointerReader& operator=(BackgroundPointerReader&&) = delete;

    void start() override;
    [[nodiscard]] bool isReading() const override { return running_; }
    [[nodiscard]] std::optional<PointerStatus> lastStatus() const override { return last_; }

signals:
    // Uma leitura terminou; o resultado está em lastStatus().
    void readFinished();

private:
    void onFinished();

    const ReadPointerConfiguration& useCase_;
    std::optional<PointerStatus> last_;
    QFutureWatcher<PointerStatus> watcher_;
    bool running_ = false;
};

} // namespace ghub::pointer
