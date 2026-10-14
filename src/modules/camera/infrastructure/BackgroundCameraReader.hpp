#pragma once

#include "modules/camera/application/CameraBackend.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <optional>

namespace ghub::camera {

// Leitura da câmera fora da thread de UI (ADR-0008 §6, R12). Executa `inner.readCamera()` numa
// thread de trabalho, guarda a última leitura e emite cameraRead() ao terminar; readCamera() só
// devolve o cache, sem I/O. Uma leitura por vez; o destrutor espera a leitura em andamento.
// `inner` deve sobreviver a este objeto. Use sempre a partir da thread que o possui.
class BackgroundCameraReader final : public QObject, public CameraBackend {
    Q_OBJECT

public:
    BackgroundCameraReader(CameraBackend& inner, std::chrono::milliseconds refreshInterval, QObject* parent = nullptr);
    ~BackgroundCameraReader() override;

    BackgroundCameraReader(const BackgroundCameraReader&) = delete;
    BackgroundCameraReader& operator=(const BackgroundCameraReader&) = delete;
    BackgroundCameraReader(BackgroundCameraReader&&) = delete;
    BackgroundCameraReader& operator=(BackgroundCameraReader&&) = delete;

    [[nodiscard]] shared_kernel::Result<std::optional<CameraDescription>> readCamera() override;

    // Dispara uma leitura; ignorado se já houver uma em andamento.
    Q_SLOT void refresh(); // Q_SLOT em vez de `public slots:` para não repetir o especificador `public:`

signals:
    // Uma leitura terminou (mudou ou não; quem decide é o consumidor).
    void cameraRead();

private:
    using Reading = shared_kernel::Result<std::optional<CameraDescription>>;

    void onReadFinished();

    CameraBackend& inner_;
    Reading cache_;
    QFutureWatcher<Reading> watcher_;
    QTimer timer_;
    bool running_ = false;
};

} // namespace ghub::camera
