#pragma once

#include "modules/camera/application/SetCameraControl.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <optional>

namespace ghub::camera {

// Grava controles da câmera por ação do usuário, fora da thread de UI (ADR-0010 §7).
// Nada é gravado na construção. Executa `SetCameraControl` numa thread de trabalho, uma operação por vez:
// um pedido feito durante uma operação substitui o pedido pendente (só o último vale) e roda quando a atual
// termina. Sem retentativa. Ao terminar, atualiza `status` (valor relido ou motivo curto da falha) e emite
// controlWritten(), sucesso ou não (o composition root liga isso à releitura da câmera).
// O caso de uso deve sobreviver a este objeto; o destrutor espera a operação em andamento e descarta o
// pendente. Use sempre a partir da thread que o possui.
class CameraControlEditor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY editorChanged)
    Q_PROPERTY(QString status READ status NOTIFY editorChanged)

public:
    explicit CameraControlEditor(const SetCameraControl& setCameraControl, QObject* parent = nullptr);
    ~CameraControlEditor() override;

    CameraControlEditor(const CameraControlEditor&) = delete;
    CameraControlEditor& operator=(const CameraControlEditor&) = delete;
    CameraControlEditor(CameraControlEditor&&) = delete;
    CameraControlEditor& operator=(CameraControlEditor&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] QString status() const { return status_; }

    Q_INVOKABLE void setValue(quint32 id, qint64 value);
    Q_INVOKABLE void restoreDefault(quint32 id);

signals:
    void editorChanged();
    // Uma escrita terminou (sucesso ou falha).
    void controlWritten();

private:
    struct Request {
        std::uint32_t id;
        bool restoreDefault;
        std::int64_t value;
    };
    using Outcome = shared_kernel::Result<CameraControl>;

    void submit(Request request);
    void start(Request request);
    void onFinished();

    const SetCameraControl* setCameraControl_; // não nulo
    QFutureWatcher<Outcome> watcher_;
    std::optional<Request> pending_;
    QString status_;
    bool running_ = false;
};

} // namespace ghub::camera
