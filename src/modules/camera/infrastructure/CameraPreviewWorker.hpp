#pragma once

#include "modules/camera/application/PreviewRunner.hpp"
#include "modules/camera/application/VideoCapture.hpp"

#include <QImage>
#include <QObject>
#include <QString>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace ghub::camera {

// Executa a pré-visualização numa thread de trabalho: inicia a captura, lê os quadros, decodifica e
// entrega ao dono do objeto (thread de UI). Todos os sinais são emitidos na thread que possui este objeto.
//
// Thread: std::jthread, não QThread. A thread não precisa de laço de eventos nem de sinais próprios;
// o cancelamento cooperativo (std::stop_token) e o join no destrutor do jthread encaixam no RAII que
// o resto do módulo usa. A entrega à UI é um QMetaObject::invokeMethod enfileirado com um número de
// execução: stop() invalida o número, então nada daquela execução (nem o que já estava na fila) é emitido.
// Os quadros usam "o mais recente vence": no máximo uma notificação pendente, a fila não cresce.
//
// `capture` precisa sobreviver a este objeto. Use start()/stop() sempre a partir da thread dona do objeto.
class CameraPreviewWorker final : public QObject, public PreviewRunner {
    Q_OBJECT

public:
    // `frameTimeout`: espera máxima por quadro em cada leitura (limita a demora do stop).
    // `stallLimit`: tempo sem quadro válido que encerra a pré-visualização.
    explicit CameraPreviewWorker(VideoCapture& capture,
                                 std::chrono::milliseconds frameTimeout = std::chrono::milliseconds{200},
                                 std::chrono::milliseconds stallLimit = std::chrono::milliseconds{5000},
                                 QObject* parent = nullptr);
    ~CameraPreviewWorker() override; // equivale a stop()

    CameraPreviewWorker(const CameraPreviewWorker&) = delete;
    CameraPreviewWorker& operator=(const CameraPreviewWorker&) = delete;
    CameraPreviewWorker(CameraPreviewWorker&&) = delete;
    CameraPreviewWorker& operator=(CameraPreviewWorker&&) = delete;

    // Volta na hora; o resultado chega pelos sinais. Se houver execução ativa, ela é encerrada antes.
    void start(const PreviewMode& mode) override;
    // Pede a parada, espera a thread terminar e destrói a sessão. Depois que volta, nenhum sinal daquela
    // execução é emitido. Sem execução ativa não faz nada.
    void stop() override;

    // Quadros descartados por serem inválidos desde o último start().
    [[nodiscard]] std::uint64_t droppedFrames() const noexcept { return dropped_.load(); }

signals:
    void started();                                                         // a captura abriu
    void frameReady(const QImage& image);                                   // quadro decodificado
    void ended(ghub::camera::PreviewFailure reason, const QString& detail); // terminou sozinho

private:
    void run(const std::stop_token& stopToken, const PreviewMode& mode, std::uint64_t generation);
    void postStarted(std::uint64_t generation);
    void postFrame(std::uint64_t generation, QImage image);
    void postEnded(std::uint64_t generation, PreviewFailure reason, QString detail);
    void deliverFrame(std::uint64_t generation);

    VideoCapture& capture_;
    std::chrono::milliseconds frameTimeout_;
    std::chrono::milliseconds stallLimit_;

    std::uint64_t generation_ = 0; // só na thread dona; incrementado por stop()
    std::jthread thread_;
    std::atomic<std::uint64_t> dropped_{0};

    std::mutex frameMutex_; // protege os dois campos abaixo (worker escreve, dono lê)
    QImage pendingFrame_;
    bool frameNotified_ = false;
};

} // namespace ghub::camera
