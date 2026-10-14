#pragma once

#include "modules/camera/application/GetCameraStatus.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <chrono>

namespace ghub::camera {

// Expõe ao QML o estado da câmera (somente leitura) e relê periodicamente.
//  - `status`: "não detectada", "leitura indisponível" ou o nome da câmera;
//  - `modes`: mapas com `text`, um por formato de pixel (resumo de summarizeModes);
//  - `controls`: mapas, na ordem da descrição, com `name` e `summary` (texto) e os campos para edição:
//    `id`, `kind` ("integer" | "boolean" | "menu"), `minimum`, `maximum`, `step`, `defaultValue`,
//    `value` (ausente sem valor atual), `options` (mapas `value`/`label`; vazia fora de menu) e
//    `editable` (gravável, ativo e com valor atual).
// O caso de uso informado precisa sobreviver ao ViewModel.
class CameraViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY cameraChanged)
    Q_PROPERTY(QVariantList modes READ modes NOTIFY cameraChanged)
    Q_PROPERTY(QVariantList controls READ controls NOTIFY cameraChanged)

public:
    CameraViewModel(const GetCameraStatus& getCameraStatus, std::chrono::milliseconds refreshInterval,
                    QObject* parent = nullptr);

    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QVariantList modes() const { return modes_; }
    [[nodiscard]] QVariantList controls() const { return controls_; }

    Q_INVOKABLE void refresh();

signals:
    void cameraChanged();

private:
    const GetCameraStatus* getCameraStatus_; // não nulo
    QTimer timer_;
    QString status_;
    QVariantList modes_;
    QVariantList controls_;
};

} // namespace ghub::camera
