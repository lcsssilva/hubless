#include "modules/camera/infrastructure/BackgroundCameraReader.hpp"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <thread>

using ghub::camera::BackgroundCameraReader;
using ghub::camera::CameraDescription;
using ghub::shared_kernel::ErrorCode;
using namespace std::chrono_literals;

namespace {

constexpr auto longInterval = std::chrono::hours{1};
constexpr int waitMs = 10000;

using Reading = ghub::shared_kernel::Result<std::optional<CameraDescription>>;

// Backend que pode travar até o teste liberar; conta chamadas e a concorrência máxima observada.
class BlockableBackend final : public ghub::camera::CameraBackend {
public:
    explicit BlockableBackend(bool blocking) : blocking_(blocking) {}

    [[nodiscard]] Reading readCamera() override
    {
        calls_.fetch_add(1);
        const int now = active_.fetch_add(1) + 1;
        int seen = maxActive_.load();
        while (now > seen && !maxActive_.compare_exchange_weak(seen, now)) {
        }
        started_.release();
        if (blocking_) {
            release_.acquire();
        }
        Reading result = [this] {
            const std::scoped_lock lock(mutex_);
            return result_;
        }();
        active_.fetch_sub(1);
        finished_.store(true);
        return result;
    }

    void setResult(Reading result)
    {
        const std::scoped_lock lock(mutex_);
        result_ = std::move(result);
    }

    // Espera a próxima entrada em readCamera().
    [[nodiscard]] bool waitStarted() { return started_.try_acquire_for(10s); }
    void letOneRead() { release_.release(); }
    [[nodiscard]] int calls() const { return calls_.load(); }
    [[nodiscard]] int maxActive() const { return maxActive_.load(); }
    [[nodiscard]] int active() const { return active_.load(); }
    [[nodiscard]] bool finished() const { return finished_.load(); }

private:
    bool blocking_;
    std::atomic<int> calls_{0};
    std::atomic<int> active_{0};
    std::atomic<int> maxActive_{0};
    std::atomic<bool> finished_{false};
    std::counting_semaphore<> started_{0};
    std::counting_semaphore<> release_{0};
    std::mutex mutex_;
    Reading result_{std::nullopt};
};

CameraDescription named(const std::string& name)
{
    return CameraDescription{.name = name, .modes = {}, .controls = {}};
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class BackgroundCameraReaderTest : public QObject {
    Q_OBJECT

private slots:
    void reportsPending_BeforeFirstReadFinishes()
    {
        BlockableBackend inner{true};
        BackgroundCameraReader reader{inner, longInterval};
        QVERIFY(inner.waitStarted());

        const auto reading = reader.readCamera();

        QVERIFY(!reading.has_value());
        QCOMPARE(reading.error().code, ErrorCode::BackendUnavailable);
        QCOMPARE(QString::fromStdString(reading.error().detail), QStringLiteral("primeira leitura em andamento"));
        QSignalSpy spy(&reader, &BackgroundCameraReader::cameraRead);
        inner.letOneRead();
        QVERIFY(spy.wait(waitMs));
    }

    void doesNotBlockCaller_WhileInnerIsBlocked()
    {
        BlockableBackend inner{true};
        BackgroundCameraReader reader{inner, longInterval}; // o construtor retornou com o inner ainda por travar
        QVERIFY(inner.waitStarted());

        const auto cached = reader.readCamera(); // retorna com o inner travado
        Q_UNUSED(cached)

        QCOMPARE(inner.active(), 1);
        QSignalSpy spy(&reader, &BackgroundCameraReader::cameraRead);
        inner.letOneRead();
        QVERIFY(spy.wait(waitMs));
    }

    void exposesResult_AndEmits_WhenReadFinishes()
    {
        BlockableBackend inner{false};
        inner.setResult(Reading{std::optional<CameraDescription>{named("Brio 100")}});
        BackgroundCameraReader reader{inner, longInterval};
        QSignalSpy spy(&reader, &BackgroundCameraReader::cameraRead);

        QVERIFY(spy.wait(waitMs));

        const auto reading = reader.readCamera();
        QVERIFY(reading.has_value());
        if (!reading->has_value()) {
            QFAIL("a leitura deveria conter a câmera");
        }
        QCOMPARE(QString::fromStdString((*reading)->name), QStringLiteral("Brio 100"));
    }

    void propagatesInnerError()
    {
        BlockableBackend inner{false};
        inner.setResult(ghub::shared_kernel::fail(ErrorCode::MalformedData, "quebrado"));
        BackgroundCameraReader reader{inner, longInterval};
        QSignalSpy spy(&reader, &BackgroundCameraReader::cameraRead);

        QVERIFY(spy.wait(waitMs));

        const auto reading = reader.readCamera();
        QVERIFY(!reading.has_value());
        QCOMPARE(reading.error().code, ErrorCode::MalformedData);
        QCOMPARE(QString::fromStdString(reading.error().detail), QStringLiteral("quebrado"));
    }

    void ignoresRefresh_WhileReadInProgress()
    {
        BlockableBackend inner{true};
        BackgroundCameraReader reader{inner, longInterval};
        QVERIFY(inner.waitStarted());
        QSignalSpy spy(&reader, &BackgroundCameraReader::cameraRead);

        reader.refresh();
        reader.refresh();
        reader.refresh();
        inner.letOneRead();
        QVERIFY(spy.wait(waitMs));

        QCOMPARE(inner.calls(), 1);
        QCOMPARE(inner.maxActive(), 1);
        QCOMPARE(spy.count(), 1);
    }

    void refreshesPeriodically()
    {
        BlockableBackend inner{false};
        BackgroundCameraReader reader{inner, 5ms};
        QSignalSpy spy(&reader, &BackgroundCameraReader::cameraRead);

        while (spy.count() < 3) {
            QVERIFY(spy.wait(waitMs));
        }

        QVERIFY(inner.calls() >= 3);
        QCOMPARE(inner.maxActive(), 1);
    }

    void refreshReadsAgain_AfterPreviousFinished()
    {
        BlockableBackend inner{false};
        BackgroundCameraReader reader{inner, longInterval};
        QSignalSpy spy(&reader, &BackgroundCameraReader::cameraRead);
        QVERIFY(spy.wait(waitMs));
        QCOMPARE(inner.calls(), 1);

        inner.setResult(Reading{std::optional<CameraDescription>{named("Segunda")}});
        reader.refresh();
        QVERIFY(spy.wait(waitMs));

        QCOMPARE(inner.calls(), 2);
        const auto reading = reader.readCamera();
        QVERIFY(reading.has_value());
        if (!reading->has_value()) {
            QFAIL("a leitura deveria conter a câmera");
        }
        QCOMPARE(QString::fromStdString((*reading)->name), QStringLiteral("Segunda"));
    }

    void destructorWaitsForRunningRead()
    {
        BlockableBackend inner{true};
        auto reader = std::make_unique<BackgroundCameraReader>(inner, longInterval);
        QVERIFY(inner.waitStarted());
        std::atomic<bool> destroying{false};
        // Libera o inner a partir de outra thread assim que o teste inicia a destruição. A espera do
        // destrutor é o que impede o retorno antes de o inner terminar; sem ela, `finished()` seria
        // falso (ou o inner usaria objetos já destruídos, o que o ASan acusa).
        std::thread releaser([&inner, &destroying] {
            destroying.wait(false);
            inner.letOneRead();
        });

        destroying.store(true);
        destroying.notify_all();
        reader.reset();

        QVERIFY(inner.finished());
        releaser.join();
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(BackgroundCameraReaderTest)
#include "BackgroundCameraReaderTest.moc"
