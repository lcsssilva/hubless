#include "modules/camera/infrastructure/CameraPreviewWorker.hpp"
#include "fakes/FakeVideoCapture.hpp"

#include <QCoreApplication>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <vector>

using namespace std::chrono_literals;
using ghub::camera::CameraPreviewWorker;
using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::PreviewFailure;
using ghub::camera::PreviewMode;
using ghub::camera::VideoCaptureSession;
using ghub::fakes::FakeReadStep;
using ghub::fakes::FakeTimeout;
using ghub::fakes::FakeVideoCapture;
using ghub::shared_kernel::ErrorCode;

Q_DECLARE_METATYPE(ghub::camera::PreviewFailure)

namespace {

PreviewMode yuyv2x2()
{
    return {
        .format = PixelFormat::create("YUYV").value(),
        .width = 2,
        .height = 2,
        .frameRate = FrameRate::create(30, 1).value(),
    };
}

// Quadro YUYV 2x2 cinza uniforme: Y=16 vira preto, Y=235 vira branco.
std::vector<std::byte> grayFrame(unsigned y)
{
    const auto luma = static_cast<std::byte>(y);
    const auto chroma = std::byte{128};
    return {luma, chroma, luma, chroma, luma, chroma, luma, chroma}; // 2x2 = 2 linhas de um par Y0 U Y1 V
}

std::vector<std::byte> garbageFrame()
{
    return {std::byte{1}, std::byte{2}, std::byte{3}}; // tamanho errado para YUYV 2x2
}

// Captura de teste com comportamentos que o FakeVideoCapture não tem: `start` e leituras que bloqueiam
// até o teste liberar (nunca por tempo fixo) e um stream infinito de lixo. Usada a partir da thread de
// trabalho; os contadores são atômicos.
class GatedCapture final : public ghub::camera::VideoCapture {
public:
    QSemaphore startGate;   // `start` espera uma liberação quando `blockStart`
    QSemaphore readEntered; // liberada a cada `readFrame` iniciado
    QSemaphore readRelease; // `readFrame` bloqueia aqui (até o prazo) quando `blockReads`
    bool blockStart = false;
    bool blockReads = false;
    bool garbageForever = false;
    std::atomic<int> live{0};
    std::atomic<int> peak{0};
    std::atomic<int> starts{0};

    [[nodiscard]] ghub::shared_kernel::Result<std::unique_ptr<VideoCaptureSession>>
    start(const PreviewMode& /*mode*/) override
    {
        ++starts;
        if (blockStart) {
            startGate.tryAcquire(1, 10000); // teto de segurança; o teste libera antes
        }
        return std::make_unique<Session>(*this);
    }

private:
    class Session final : public VideoCaptureSession {
    public:
        explicit Session(GatedCapture& owner) : owner_(owner)
        {
            const int now = ++owner_.live;
            int seen = owner_.peak.load();
            while (now > seen && !owner_.peak.compare_exchange_weak(seen, now)) {
            }
        }
        ~Session() override { --owner_.live; }
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] ghub::shared_kernel::Result<bool> readFrame(std::chrono::milliseconds timeout,
                                                                  std::vector<std::byte>& frame) override
        {
            owner_.readEntered.release();
            if (owner_.garbageForever) {
                frame = garbageFrame();
                return true;
            }
            if (owner_.blockReads) {
                owner_.readRelease.tryAcquire(1, static_cast<int>(timeout.count())); // modela o poll com prazo
            }
            return false;
        }

    private:
        GatedCapture& owner_;
    };
};

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class CameraPreviewWorkerTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<ghub::camera::PreviewFailure>(); }

    void startReturnsImmediately_AndEmitsStarted()
    {
        GatedCapture capture;
        capture.blockStart = true;
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QSignalSpy started(&worker, &CameraPreviewWorker::started);

        worker.start(yuyv2x2()); // se bloqueasse em `capture.start`, o teste ficaria preso aqui

        QCoreApplication::processEvents();
        QCOMPARE(started.count(), 0);
        capture.startGate.release();
        QTRY_COMPARE(started.count(), 1);
        worker.stop();
        QCOMPARE(capture.live.load(), 0);
    }

    void deliversDecodedFrames()
    {
        FakeVideoCapture capture;
        capture.succeedWith({grayFrame(16), grayFrame(126), grayFrame(235)});
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QSignalSpy frames(&worker, &CameraPreviewWorker::frameReady);

        worker.start(yuyv2x2());

        // "Quadro mais recente vence": os intermediários podem ser substituídos, o último sempre chega.
        QTRY_VERIFY(!frames.isEmpty() && qvariant_cast<QImage>(frames.last().at(0)).pixel(0, 0) == qRgb(255, 255, 255));
        int previous = -1;
        for (const auto& args : std::as_const(frames)) {
            const auto image = qvariant_cast<QImage>(args.at(0));
            QCOMPARE(image.size(), QSize(2, 2));
            const int gray = qGray(image.pixel(0, 0));
            QVERIFY2(gray >= previous, "ordem dos quadros trocada");
            previous = gray;
        }
        worker.stop();
        QCOMPARE(capture.liveSessions(), 0);
    }

    void emitsCameraBusy_WhenCaptureReportsDeviceBusy()
    {
        FakeVideoCapture capture;
        capture.failStartWith({.code = ErrorCode::DeviceBusy, .detail = "ocupada"});
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);
        QSignalSpy started(&worker, &CameraPreviewWorker::started);

        worker.start(yuyv2x2());

        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(0).value<PreviewFailure>(), PreviewFailure::CameraBusy);
        QCOMPARE(ended.first().at(1).toString(), QStringLiteral("ocupada"));
        QCOMPARE(started.count(), 0);
    }

    void emitsCaptureFailed_OnOtherStartError()
    {
        FakeVideoCapture capture;
        capture.failStartWith({.code = ErrorCode::BackendUnavailable, .detail = "S_FMT falhou"});
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);

        worker.start(yuyv2x2());

        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(0).value<PreviewFailure>(), PreviewFailure::CaptureFailed);
        QCOMPARE(ended.first().at(1).toString(), QStringLiteral("S_FMT falhou"));
    }

    void emitsCaptureFailed_OnNullSession()
    {
        FakeVideoCapture capture;
        capture.returnNullSession();
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);

        worker.start(yuyv2x2());

        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(0).value<PreviewFailure>(), PreviewFailure::CaptureFailed);
    }

    void emitsStreamFailed_OnReadError_AndReleasesSession()
    {
        FakeVideoCapture capture;
        int liveWhenEnded = -1;
        capture.succeedWith({
            grayFrame(16),
            ghub::shared_kernel::DomainError{
                .code = ErrorCode::BackendUnavailable,
                .detail = "ENODEV",
            },
        });
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QObject::connect(&worker, &CameraPreviewWorker::ended, &worker,
                         [&] { liveWhenEnded = capture.liveSessions(); });
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);

        worker.start(yuyv2x2());

        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(0).value<PreviewFailure>(), PreviewFailure::StreamFailed);
        QCOMPARE(ended.first().at(1).toString(), QStringLiteral("ENODEV"));
        QCOMPARE(liveWhenEnded, 0); // a câmera já estava liberada quando o sinal chegou
    }

    void emitsNoFrames_AfterStallLimit()
    {
        FakeVideoCapture capture;
        capture.succeedWith({FakeTimeout{}, FakeTimeout{}, FakeTimeout{}}); // depois do roteiro: sempre prazo esgotado
        CameraPreviewWorker worker(capture, 5ms, 50ms);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);

        worker.start(yuyv2x2());

        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
        QCOMPARE(ended.first().at(0).value<PreviewFailure>(), PreviewFailure::NoFrames);
        QCOMPARE(capture.liveSessions(), 0);
    }

    void countsInvalidFrames_AndKeepsRunning()
    {
        FakeVideoCapture capture;
        capture.succeedWith({
            garbageFrame(),
            grayFrame(235),
            garbageFrame(),
            grayFrame(235),
            ghub::shared_kernel::DomainError{
                .code = ErrorCode::BackendUnavailable,
                .detail = "fim",
            },
        });
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QSignalSpy frames(&worker, &CameraPreviewWorker::frameReady);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);

        worker.start(yuyv2x2());

        QTRY_COMPARE(ended.count(), 1); // só o erro de leitura encerra; o lixo antes dele não
        QCOMPARE(ended.first().at(0).value<PreviewFailure>(), PreviewFailure::StreamFailed);
        QCOMPARE(worker.droppedFrames(), 2U);
        QVERIFY(frames.count() >= 1);
    }

    void invalidFramesDoNotResetStall()
    {
        GatedCapture capture;
        capture.garbageForever = true;
        CameraPreviewWorker worker(capture, 5ms, 50ms);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);
        QSignalSpy frames(&worker, &CameraPreviewWorker::frameReady);

        worker.start(yuyv2x2());

        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
        QCOMPARE(ended.first().at(0).value<PreviewFailure>(), PreviewFailure::NoFrames);
        QCOMPARE(frames.count(), 0);
        QVERIFY(worker.droppedFrames() > 0);
    }

    void stopReleasesSession_AndSilencesSignals()
    {
        FakeVideoCapture capture;
        capture.succeedWith({grayFrame(16), grayFrame(235), grayFrame(16), grayFrame(235)});
        CameraPreviewWorker worker(capture, 5ms, 10s);
        QSignalSpy started(&worker, &CameraPreviewWorker::started);
        QSignalSpy frames(&worker, &CameraPreviewWorker::frameReady);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);
        worker.start(yuyv2x2());
        // Sem processar eventos: o que a thread já enfileirou está pendente quando o stop chega.

        worker.stop();

        QCOMPARE(capture.liveSessions(), 0); // lido só depois do join
        QCOMPARE(capture.startedModes().size(), 1U);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(&worker);
        QCOMPARE(started.count(), 0);
        QCOMPARE(frames.count(), 0);
        QCOMPARE(ended.count(), 0);
    }

    void stopDuringBlockedRead_ReturnsAndReleasesSession()
    {
        GatedCapture capture;
        capture.blockReads = true;
        CameraPreviewWorker worker(capture, 50ms, 10s);
        worker.start(yuyv2x2());
        QVERIFY(capture.readEntered.tryAcquire(1, 5000)); // a thread está dentro de uma leitura

        worker.stop(); // espera no máximo ~um frameTimeout

        QCOMPARE(capture.live.load(), 0);
    }

    void destructionDuringStreamReleasesSession()
    {
        GatedCapture capture;
        capture.blockReads = true;
        {
            CameraPreviewWorker worker(capture, 50ms, 10s);
            worker.start(yuyv2x2());
            QVERIFY(capture.readEntered.tryAcquire(1, 5000));
        }
        QCOMPARE(capture.live.load(), 0);
    }

    void restartStopsPreviousRun()
    {
        GatedCapture capture;
        capture.blockReads = true;
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QSignalSpy started(&worker, &CameraPreviewWorker::started);
        worker.start(yuyv2x2());
        QTRY_COMPARE(started.count(), 1);

        worker.start(yuyv2x2());

        QTRY_COMPARE(started.count(), 2);
        QCOMPARE(capture.starts.load(), 2);
        QCOMPARE(capture.peak.load(), 1); // nunca duas sessões ao mesmo tempo
        worker.stop();
        QCOMPARE(capture.live.load(), 0);
    }

    void stopWithoutStartIsNoop()
    {
        FakeVideoCapture capture;
        CameraPreviewWorker worker(capture);
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);

        worker.stop();
        worker.stop();

        QCOMPARE(capture.startedModes().size(), 0U);
        QCOMPARE(ended.count(), 0);
    }

    void signalsAreEmittedInOwnerThread()
    {
        FakeVideoCapture capture;
        capture.succeedWith({
            grayFrame(235),
            ghub::shared_kernel::DomainError{
                .code = ErrorCode::BackendUnavailable,
                .detail = "fim",
            },
        });
        CameraPreviewWorker worker(capture, 20ms, 10s);
        QThread* startedIn = nullptr;
        QThread* frameIn = nullptr;
        QThread* endedIn = nullptr;
        // Sem objeto de contexto: a conexão é direta, então o lambda roda na thread que emitiu.
        connect(&worker, &CameraPreviewWorker::started, [&] { startedIn = QThread::currentThread(); });
        connect(&worker, &CameraPreviewWorker::frameReady, [&](const QImage&) { frameIn = QThread::currentThread(); });
        connect(&worker, &CameraPreviewWorker::ended,
                [&](PreviewFailure, const QString&) { endedIn = QThread::currentThread(); });
        QSignalSpy ended(&worker, &CameraPreviewWorker::ended);

        worker.start(yuyv2x2());

        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(startedIn, QThread::currentThread());
        QCOMPARE(frameIn, QThread::currentThread());
        QCOMPARE(endedIn, QThread::currentThread());
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(CameraPreviewWorkerTest)
#include "CameraPreviewWorkerTest.moc"
