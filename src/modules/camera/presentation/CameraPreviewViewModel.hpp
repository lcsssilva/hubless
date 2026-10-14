#pragma once

#include "modules/camera/application/PreviewRunner.hpp"
#include "modules/camera/application/StartCameraPreview.hpp"
#include "modules/camera/domain/PreviewMode.hpp"

#include <QImage>
#include <QObject>
#include <QString>

#include <optional>

namespace ghub::camera {

// Expõe ao QML a pré-visualização da câmera: estado, texto de status e os quadros ao vivo.
//  - `active`: iniciando ou ao vivo;
//  - `status`: "parada", "iniciando…", "ao vivo · <FOURCC> <L>×<A> a <fps> fps" ou o motivo da falha;
//  - `frameWidth`/`frameHeight`: do modo em uso; 0 quando parada.
// O construtor não inicia nada: a pré-visualização só começa em toggle() (ação do usuário).
// onStarted/onFrame/onEnded são ligados no composition root aos sinais do runner assíncrono (o worker);
// recebidos com o ViewModel parado, são ignorados (sinais atrasados). O `detail` das falhas só vai ao log.
// O caso de uso e o runner informados precisam sobreviver ao ViewModel.
class CameraPreviewViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY previewChanged)
    Q_PROPERTY(QString status READ status NOTIFY previewChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY previewChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY previewChanged)

public:
    CameraPreviewViewModel(const StartCameraPreview& startPreview, PreviewRunner& runner, QObject* parent = nullptr);
    ~CameraPreviewViewModel() override; // chama runner.stop() se ativo

    CameraPreviewViewModel(const CameraPreviewViewModel&) = delete;
    CameraPreviewViewModel& operator=(const CameraPreviewViewModel&) = delete;
    CameraPreviewViewModel(CameraPreviewViewModel&&) = delete;
    CameraPreviewViewModel& operator=(CameraPreviewViewModel&&) = delete;

    [[nodiscard]] bool active() const { return state_ != State::Stopped; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] int frameWidth() const { return mode_.has_value() ? static_cast<int>(mode_->width) : 0; }
    [[nodiscard]] int frameHeight() const { return mode_.has_value() ? static_cast<int>(mode_->height) : 0; }

    Q_INVOKABLE void toggle(); // parado inicia; iniciando/ao vivo para
    Q_INVOKABLE void stop();

    Q_SLOT void onStarted();
    Q_SLOT void onFrame(const QImage& image);
    Q_SLOT void onEnded(ghub::camera::PreviewFailure reason, const QString& detail);

signals:
    void previewChanged();
    void frameReady(const QImage& image); // só enquanto ativo

private:
    enum class State : std::uint8_t { Stopped, Starting, Live };

    void fail(PreviewFailure reason, const QString& detail);
    void apply(State state, const std::optional<PreviewMode>& mode, QString status);

    const StartCameraPreview* startPreview_; // não nulo
    PreviewRunner* runner_;                  // não nulo
    State state_ = State::Stopped;
    std::optional<PreviewMode> mode_;
    QString status_;
};

} // namespace ghub::camera
