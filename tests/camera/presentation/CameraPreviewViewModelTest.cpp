#include "modules/camera/presentation/CameraPreviewViewModel.hpp"
#include "fakes/FakeCameraBackend.hpp"
#include "fakes/FakePreviewRunner.hpp"

#include <QImage>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

#include <optional>
#include <string_view>

using ghub::camera::CameraDescription;
using ghub::camera::CameraPreviewViewModel;
using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::PreviewFailure;
using ghub::camera::StartCameraPreview;
using ghub::camera::VideoMode;
using ghub::shared_kernel::ErrorCode;

namespace {

FrameRate fps(std::uint32_t frames, std::uint32_t seconds = 1)
{
    return FrameRate::create(frames, seconds).value();
}

VideoMode mode(std::string_view fourcc, std::uint32_t width, std::uint32_t height, FrameRate rate)
{
    return VideoMode::create(PixelFormat::create(fourcc).value(), width, height, {rate}).value();
}

CameraDescription brio()
{
    return {
        .name = "Brio 100",
        .modes = {mode("YUYV", 640, 480, fps(30)), mode("MJPG", 1280, 720, fps(30))},
        .controls = {},
    };
}

QImage redImage()
{
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::red);
    return image;
}

class CameraPreviewViewModelTest : public QObject {
    Q_OBJECT

private:
    // Cada teste recebe um conjunto novo (init/cleanup); `model` nasce parado.
    ghub::fakes::FakeCameraBackend* backend = nullptr;
    ghub::fakes::FakePreviewRunner* runner = nullptr;
    StartCameraPreview* useCase = nullptr;
    CameraPreviewViewModel* model = nullptr;

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private:` acima e `private slots:` são ambos privados.
private slots:
    void init()
    {
        backend = new ghub::fakes::FakeCameraBackend;
        backend->setResult(std::optional<CameraDescription>{brio()});
        runner = new ghub::fakes::FakePreviewRunner;
        useCase = new StartCameraPreview(*backend, *runner);
        model = new CameraPreviewViewModel(*useCase, *runner);
    }

    void cleanup()
    {
        delete model;
        delete useCase;
        delete runner;
        delete backend;
    }

    void doesNotStartOnConstruction()
    {
        QVERIFY(runner->startedModes().empty());
        QCOMPARE(backend->calls(), 0);
        QVERIFY(!model->active());
        QCOMPARE(model->status(), QStringLiteral("parada"));
        QCOMPARE(model->frameWidth(), 0);
        QCOMPARE(model->frameHeight(), 0);
    }

    void toggleStartsRunner_WithSelectedMode_AndShowsStarting()
    {
        QSignalSpy changed(model, &CameraPreviewViewModel::previewChanged);

        model->toggle();

        QCOMPARE(runner->startedModes().size(), 1U);
        QCOMPARE(runner->startedModes().front().width, 1280U);
        QVERIFY(model->active());
        QCOMPARE(model->status(), QStringLiteral("iniciando…"));
        QCOMPARE(model->frameWidth(), 1280);
        QCOMPARE(model->frameHeight(), 720);
        QCOMPARE(changed.count(), 1);
    }

    void startDoesNotReadCameraMoreThanOnce()
    {
        model->toggle();
        QCOMPARE(backend->calls(), 1);
    }

    void failsNotDetected()
    {
        backend->setResult(std::optional<CameraDescription>{});
        model->toggle();
        QCOMPARE(model->status(), QStringLiteral("câmera não detectada"));
        QVERIFY(!model->active());
        QVERIFY(runner->startedModes().empty());
    }

    void failsUnavailable_WithoutLeakingDetail()
    {
        backend->setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "/dev/video0 negado"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^camera: .*/dev/video0 negado")));
        model->toggle();
        QCOMPARE(model->status(), QStringLiteral("leitura da câmera indisponível"));
        QVERIFY(!model->status().contains(QStringLiteral("/dev")));
        QVERIFY(!model->active());
    }

    void failsNoCompatibleMode()
    {
        backend->setResult(std::optional<CameraDescription>{
            CameraDescription{.name = "Brio 100", .modes = {mode("H264", 1280, 720, fps(30))}, .controls = {}}});
        model->toggle();
        QCOMPARE(model->status(), QStringLiteral("nenhum modo de vídeo compatível com a pré-visualização"));
        QVERIFY(!model->active());
    }

    void onStartedShowsLive_WithModeText()
    {
        model->toggle();
        model->onStarted();
        QCOMPARE(model->status(), QStringLiteral("ao vivo · MJPG 1280×720 a 30 fps"));
        QVERIFY(model->active());
    }

    void onStartedShowsFractionalRate_WithCommaAndNoTrailingZero()
    {
        backend->setResult(std::optional<CameraDescription>{
            CameraDescription{.name = "Brio 100", .modes = {mode("YUYV", 640, 480, fps(45, 2))}, .controls = {}}});
        model->toggle();
        model->onStarted();
        QCOMPARE(model->status(), QStringLiteral("ao vivo · YUYV 640×480 a 22,5 fps"));
    }

    void asyncFailureCameraBusy()
    {
        checkAsyncFailure(PreviewFailure::CameraBusy, QStringLiteral("câmera em uso por outro programa"));
    }
    void asyncFailureCaptureFailed()
    {
        checkAsyncFailure(PreviewFailure::CaptureFailed, QStringLiteral("falha ao iniciar a captura"));
    }
    void asyncFailureStreamFailed()
    {
        checkAsyncFailure(PreviewFailure::StreamFailed, QStringLiteral("a captura foi interrompida"));
    }
    void asyncFailureNoFrames()
    {
        checkAsyncFailure(PreviewFailure::NoFrames, QStringLiteral("a câmera parou de enviar imagens"));
    }

    void toggleWhileActiveStopsRunner_AndShowsStopped()
    {
        model->toggle();
        model->onStarted();

        model->toggle();

        QCOMPARE(runner->stopCalls(), 1);
        QVERIFY(!model->active());
        QCOMPARE(model->status(), QStringLiteral("parada"));
        QCOMPARE(model->frameWidth(), 0);
        QCOMPARE(model->frameHeight(), 0);
    }

    void forwardsFramesOnlyWhileActive()
    {
        QSignalSpy frames(model, &CameraPreviewViewModel::frameReady);
        model->onFrame(redImage());
        QCOMPARE(frames.count(), 0);

        model->toggle();
        model->onStarted();
        model->onFrame(redImage());
        QCOMPARE(frames.count(), 1);
        QCOMPARE(qvariant_cast<QImage>(frames.first().at(0)).pixel(0, 0), qRgb(255, 0, 0));
    }

    void ignoresLateSignalsWhenStopped()
    {
        QSignalSpy changed(model, &CameraPreviewViewModel::previewChanged);
        QSignalSpy frames(model, &CameraPreviewViewModel::frameReady);
        model->toggle();
        model->toggle(); // parado pelo usuário
        changed.clear();

        model->onStarted();
        model->onFrame(redImage());
        model->onEnded(PreviewFailure::StreamFailed, QStringLiteral("atrasado"));

        QCOMPARE(model->status(), QStringLiteral("parada"));
        QVERIFY(!model->active());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(frames.count(), 0);
    }

    void destructionStopsRunner_WhenActive()
    {
        model->toggle();
        delete model;
        model = nullptr;
        QCOMPARE(runner->stopCalls(), 1);
    }

    void destructionDoesNotStopRunner_WhenStopped()
    {
        delete model;
        model = nullptr;
        QCOMPARE(runner->stopCalls(), 0);
    }

    void emitsPreviewChanged_OnlyOnRealChange()
    {
        model->stop(); // já parado: nada muda
        QSignalSpy changed(model, &CameraPreviewViewModel::previewChanged);
        model->stop();
        QCOMPARE(changed.count(), 0);

        model->toggle();
        QCOMPARE(changed.count(), 1);
        model->onStarted();
        QCOMPARE(changed.count(), 2);
        model->onStarted(); // repetido: mesmo estado
        QCOMPARE(changed.count(), 2);
        model->stop();
        QCOMPARE(changed.count(), 3);
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    void checkAsyncFailure(PreviewFailure reason, const QString& expectedText)
    {
        model->toggle();
        model->onStarted();

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^camera: .*/dev/video0: segredo")));
        model->onEnded(reason, QStringLiteral("/dev/video0: segredo")); // o detalhe só vai ao log

        QCOMPARE(model->status(), expectedText);
        QVERIFY(!model->active());
        QCOMPARE(model->frameWidth(), 0);
        QCOMPARE(model->frameHeight(), 0);
        QVERIFY(!model->status().contains(QStringLiteral("/dev")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(CameraPreviewViewModelTest)
#include "CameraPreviewViewModelTest.moc"
