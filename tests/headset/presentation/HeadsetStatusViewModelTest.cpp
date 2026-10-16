#include "modules/headset/presentation/HeadsetStatusViewModel.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeHeadsetStatusSource.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include <atomic>
#include <memory>
#include <semaphore>
#include <thread>

using ghub::device_access::AccessRegistry;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::headset::EqualizerInfo;
using ghub::headset::HeadsetStatus;
using ghub::headset::HeadsetStatusViewModel;
using ghub::headset::ReadHeadsetStatus;
using ghub::headset::UninterpretedRead;
using ghub::shared_kernel::ErrorCode;

namespace {

// Bytes das capturas C-00/C-01/C-03 do G733 (INC-015 parte 2).
HeadsetStatus capturedStatus()
{
    HeadsetStatus status;
    status.equalizer = EqualizerInfo{.bandCount = 10, .unexplained = {0x0C, 0x01}};
    status.uninterpreted = UninterpretedRead{.bytes = {0, 0, 0, 0}};
    return status;
}

class HeadsetStatusViewModelTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fx = std::make_unique<Fixture>(); }
    void cleanup() { fx.reset(); }

    void constructionReadsNothing()
    {
        const HeadsetStatusViewModel viewModel{fx->useCase};

        QVERIFY(!viewModel.reading());
        QVERIFY(viewModel.status().isEmpty());
        QVERIFY(viewModel.error().isEmpty());
        QCOMPARE(fx->source.reads(), 0);
    }

    void showsReadingUntilTheReadEnds_ThenTheFormattedFields()
    {
        std::binary_semaphore release{0};
        fx->source.setOnRead([&release] { release.acquire(); });
        fx->source.setResult(capturedStatus());
        HeadsetStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &HeadsetStatusViewModel::readFinished);

        viewModel.read();

        QVERIFY(viewModel.reading());
        release.release();
        QVERIFY(finished.wait(5000));
        QVERIFY(!viewModel.reading());
        QVERIFY(viewModel.error().isEmpty());
        const auto status = viewModel.status();
        QCOMPARE(status.value("equalizer").toString(), QStringLiteral("10 bandas"));
        QCOMPARE(status.value("equalizerUnexplained").toString(), QStringLiteral("0c 01 (não interpretado)"));
        QCOMPARE(status.value("uninterpreted").toString(), QStringLiteral("00 00 00 00 (não interpretado)"));
    }

    void showsNotInformedForAbsentFields()
    {
        HeadsetStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &HeadsetStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        const auto status = viewModel.status();
        QCOMPARE(status.value("equalizer").toString(), QStringLiteral("não informado"));
        QCOMPARE(status.value("equalizerUnexplained").toString(), QString{});
        QCOMPARE(status.value("uninterpreted").toString(), QStringLiteral("não informado"));
    }

    void showsTheSourceError_AndClearsItOnTheNextRead()
    {
        fx->source.setResult(
            ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "headset desligado ou fora de alcance"));
        HeadsetStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &HeadsetStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        QCOMPARE(viewModel.error(), QStringLiteral("headset desligado ou fora de alcance"));
        QVERIFY(viewModel.status().isEmpty());
        fx->source.setResult(capturedStatus());
        viewModel.read();
        QVERIFY(finished.wait(5000));
        QVERIFY(viewModel.error().isEmpty());
        QVERIFY(!viewModel.status().isEmpty());
    }

    void showsWhoHoldsTheLease_WhenTheDeviceIsBusy()
    {
        auto held =
            fx->registry.acquire(LeaseRequest{.interfaceId = PhysicalInterfaceId::create("hidpp:046d:0afe").value(),
                                              .holder = LeaseHolder::create("headset.settings").value(),
                                              .purpose = "teste",
                                              .maxDuration = std::chrono::seconds{60}});
        QVERIFY(held.has_value());
        HeadsetStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &HeadsetStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        const auto message = viewModel.error();
        QVERIFY2(message.startsWith(QStringLiteral("ocupado")), qPrintable(message));
        QVERIFY2(message.contains(QStringLiteral("headset.settings")), qPrintable(message));
        QCOMPARE(fx->source.reads(), 0);
    }

    void ignoresASecondReadWhileReading()
    {
        std::binary_semaphore release{0};
        fx->source.setOnRead([&release] { release.acquire(); });
        HeadsetStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &HeadsetStatusViewModel::readFinished);

        viewModel.read();
        viewModel.read();
        release.release();
        QVERIFY(finished.wait(5000));

        QCOMPARE(finished.count(), 1);
        QCOMPARE(fx->source.reads(), 1);
    }

    void emitsTheChangeSignals()
    {
        HeadsetStatusViewModel viewModel{fx->useCase};
        QSignalSpy reading(&viewModel, &HeadsetStatusViewModel::readingChanged);
        QSignalSpy status(&viewModel, &HeadsetStatusViewModel::statusChanged);
        QSignalSpy finished(&viewModel, &HeadsetStatusViewModel::readFinished);

        viewModel.read();
        QCOMPARE(reading.count(), 1);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reading.count(), 2);
        QVERIFY(status.count() >= 1);
    }

    void destructorWaitsForTheReadInProgress()
    {
        std::atomic<bool> started{false};
        std::atomic<bool> ended{false};
        std::binary_semaphore release{0};
        fx->source.setOnRead([&] {
            started = true;
            release.acquire();
            ended = true;
        });
        std::thread releaser{[&] {
            while (!started) {
                std::this_thread::yield();
            }
            release.release();
        }};
        {
            HeadsetStatusViewModel viewModel{fx->useCase};
            viewModel.read();
        }
        QVERIFY2(ended, "o destrutor não pode voltar enquanto a leitura roda");
        releaser.join();
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    struct Fixture {
        ghub::fakes::FakeClock clock;
        AccessRegistry registry{clock};
        ghub::fakes::FakeHeadsetStatusSource source;
        ReadHeadsetStatus useCase{source, registry};
    };

    std::unique_ptr<Fixture> fx;
};

} // namespace

QTEST_GUILESS_MAIN(HeadsetStatusViewModelTest)
#include "HeadsetStatusViewModelTest.moc"
