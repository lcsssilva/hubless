#pragma once

#include <QImage>
#include <QQuickItem>
#include <QQuickPaintedItem>

class QPainter;

namespace ghub::camera {

// Item QML que mostra o quadro ao vivo da câmera, centralizado e com a proporção preservada (letterbox).
// Não decodifica nem conhece a câmera: recebe imagens prontas (ex.: do sinal frameReady do ViewModel).
// Sem quadro não desenha nada. Registrar o tipo no QML é responsabilidade do composition root.
// Não é `final`: qmlRegisterType instancia uma classe derivada (QQmlPrivate::QQmlElement<T>).
class CameraPreviewItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)

public:
    explicit CameraPreviewItem(QQuickItem* parent = nullptr);

    [[nodiscard]] bool hasFrame() const { return !frame_.isNull(); }

    Q_INVOKABLE void setFrame(const QImage& image); // imagem nula = limpar
    Q_INVOKABLE void clear();

    void paint(QPainter* painter) override;

signals:
    void hasFrameChanged();

private:
    QImage frame_;
};

} // namespace ghub::camera
