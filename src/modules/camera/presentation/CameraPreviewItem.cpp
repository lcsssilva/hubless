#include "modules/camera/presentation/CameraPreviewItem.hpp"

#include <QPainter>

namespace ghub::camera {

CameraPreviewItem::CameraPreviewItem(QQuickItem* parent) : QQuickPaintedItem(parent) {}

void CameraPreviewItem::setFrame(const QImage& image)
{
    const bool hadFrame = hasFrame();
    frame_ = image;
    update();
    if (hadFrame != hasFrame()) {
        emit hasFrameChanged();
    }
}

void CameraPreviewItem::clear()
{
    setFrame(QImage());
}

void CameraPreviewItem::paint(QPainter* painter)
{
    if (frame_.isNull()) {
        return;
    }
    // Letterbox: o maior retângulo com a proporção do quadro que cabe no item, centralizado.
    const QSizeF scaled = QSizeF(frame_.size()).scaled(size(), Qt::KeepAspectRatio);
    const QRectF target(QPointF((width() - scaled.width()) / 2.0, (height() - scaled.height()) / 2.0), scaled);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target, frame_);
}

} // namespace ghub::camera
