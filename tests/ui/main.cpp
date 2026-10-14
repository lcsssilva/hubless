#include "modules/camera/presentation/CameraPreviewItem.hpp"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQml/qqml.h>
#include <QtQuickTest/quicktest.h>

namespace {

// Auxiliar de teste: QML não consegue produzir um QImage sozinho; este objeto devolve uma imagem sólida
// 4×2 para simular um quadro do ViewModel de pré-visualização.
class TestImages : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE static QImage solid()
    {
        QImage image(4, 2, QImage::Format_RGB32);
        image.fill(QColor(Qt::red));
        return image;
    }
};

// Registra o tipo QML do item de pré-visualização (feito no composition root no aplicativo real)
// e expõe o auxiliar de imagens como propriedade de contexto `testImages`.
class Setup : public QObject {
    Q_OBJECT

public slots:
    static void applicationAvailable()
    {
        qmlRegisterType<ghub::camera::CameraPreviewItem>("GHub.Camera", 1, 0, "CameraPreviewItem");
    }

    void qmlEngineAvailable(QQmlEngine* engine)
    {
        engine->rootContext()->setContextProperty(QStringLiteral("testImages"), &images_);
    }

private:
    TestImages images_;
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(ghub_ui, Setup)

#include "main.moc"
