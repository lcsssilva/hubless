#include "modules/camera/presentation/CameraPreviewItem.hpp"

#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

using ghub::camera::CameraPreviewItem;

namespace {

constexpr QRgb sentinel = qRgb(0, 255, 0); // fundo do teste: o que o item não pinta continua assim

QImage solid(int width, int height, QColor color)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(color);
    return image;
}

// Pinta o item numa tela do tamanho dele, já preenchida com `sentinel`.
QImage render(CameraPreviewItem& item)
{
    QImage canvas(static_cast<int>(item.width()), static_cast<int>(item.height()), QImage::Format_RGB32);
    canvas.fill(sentinel);
    QPainter painter(&canvas);
    item.paint(&painter);
    painter.end();
    return canvas;
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class CameraPreviewItemTest : public QObject {
    Q_OBJECT

private slots:
    void withoutFrame_PaintsNothing()
    {
        CameraPreviewItem item;
        item.setSize({40, 40});

        const auto canvas = render(item);

        QVERIFY(!item.hasFrame());
        QCOMPARE(canvas, [] {
            QImage expected(40, 40, QImage::Format_RGB32);
            expected.fill(sentinel);
            return expected;
        }());
    }

    void wideFrameInSquareItem_PaintsCentralBand()
    {
        CameraPreviewItem item;
        item.setSize({100, 100});
        item.setFrame(solid(20, 10, Qt::red)); // 2:1 -> 100x50 centrado, faixa y em [25, 75)

        const auto canvas = render(item);

        QCOMPARE(canvas.pixel(50, 50), qRgb(255, 0, 0));
        QCOMPARE(canvas.pixel(2, 27), qRgb(255, 0, 0));
        QCOMPARE(canvas.pixel(97, 72), qRgb(255, 0, 0));
        QCOMPARE(canvas.pixel(50, 22), sentinel); // margem superior intocada
        QCOMPARE(canvas.pixel(50, 2), sentinel);
        QCOMPARE(canvas.pixel(50, 78), sentinel); // margem inferior intocada
        QCOMPARE(canvas.pixel(50, 97), sentinel);
    }

    void squareFrameInWideItem_PaintsSideMargins()
    {
        CameraPreviewItem item;
        item.setSize({200, 100});
        item.setFrame(solid(10, 10, Qt::blue)); // 1:1 -> 100x100 centrado, colunas x em [50, 150)

        const auto canvas = render(item);

        QCOMPARE(canvas.pixel(100, 50), qRgb(0, 0, 255));
        QCOMPARE(canvas.pixel(52, 2), qRgb(0, 0, 255));
        QCOMPARE(canvas.pixel(147, 97), qRgb(0, 0, 255));
        QCOMPARE(canvas.pixel(47, 50), sentinel);
        QCOMPARE(canvas.pixel(2, 50), sentinel);
        QCOMPARE(canvas.pixel(153, 50), sentinel);
        QCOMPARE(canvas.pixel(197, 50), sentinel);
    }

    void setFrame_SignalsOnlyWhenPresenceChanges()
    {
        CameraPreviewItem item;
        QSignalSpy changed(&item, &CameraPreviewItem::hasFrameChanged);

        item.setFrame(solid(4, 4, Qt::red));
        QVERIFY(item.hasFrame());
        QCOMPARE(changed.count(), 1);

        item.setFrame(solid(4, 4, Qt::blue)); // outro quadro: continua com imagem
        QCOMPARE(changed.count(), 1);
    }

    void nullFrame_ClearsAndSignalsOnce()
    {
        CameraPreviewItem item;
        item.setFrame(solid(4, 4, Qt::red));
        QSignalSpy changed(&item, &CameraPreviewItem::hasFrameChanged);

        item.setFrame(QImage());

        QVERIFY(!item.hasFrame());
        QCOMPARE(changed.count(), 1);
        item.setFrame(QImage()); // já limpo
        QCOMPARE(changed.count(), 1);
    }

    void clear_RemovesFrameAndSignalsOnce()
    {
        CameraPreviewItem item;
        item.setSize({40, 40});
        item.setFrame(solid(4, 4, Qt::red));
        QSignalSpy changed(&item, &CameraPreviewItem::hasFrameChanged);

        item.clear();

        QVERIFY(!item.hasFrame());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(render(item).pixel(20, 20), sentinel);
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_MAIN(CameraPreviewItemTest)
#include "CameraPreviewItemTest.moc"
