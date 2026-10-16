#include "modules/lighting/presentation/KeyboardStatusViewModel.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeKeyboardStatusSource.hpp"

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
using ghub::lighting::BrightnessLimits;
using ghub::lighting::KeyboardStatus;
using ghub::lighting::ReportRateSupport;
using ghub::lighting::application::ReadKeyboardStatus;
using ghub::lighting::presentation::KeyboardStatusViewModel;
using ghub::shared_kernel::ErrorCode;

namespace {

// Bytes da captura C-00 (INC-015 parte 2).
KeyboardStatus capturedStatus()
{
    KeyboardStatus status;
    status.brightnessLimits = BrightnessLimits{.maximum = 100, .unexplained = {0x05, 0x03}};
    status.currentBrightness = 100;
    status.reportRateSupport = ReportRateSupport::fromBitmap(0x8B).value();
    status.currentPeriodMs = 1;
    return status;
}

class KeyboardStatusViewModelTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fx = std::make_unique<Fixture>(); }
    void cleanup() { fx.reset(); }

    void constructionReadsNothing()
    {
        const KeyboardStatusViewModel viewModel{fx->useCase};

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
        KeyboardStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &KeyboardStatusViewModel::readFinished);

        viewModel.read();

        QVERIFY(viewModel.reading());
        release.release();
        QVERIFY(finished.wait(5000));
        QVERIFY(!viewModel.reading());
        QVERIFY(viewModel.error().isEmpty());
        const auto status = viewModel.status();
        QCOMPARE(status.value("brightness").toString(), QStringLiteral("100 de 100"));
        QCOMPARE(status.value("brightnessUnexplained").toString(), QStringLiteral("05 03 (não interpretado)"));
        QCOMPARE(status.value("reportRate").toString(), QStringLiteral("1000 Hz (1 ms)"));
        QCOMPARE(status.value("supportedReportRates").toString(), QStringLiteral("1000, 500, 250, 125 Hz"));
    }

    void showsNotInformedForAbsentFields()
    {
        KeyboardStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &KeyboardStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        const auto status = viewModel.status();
        QCOMPARE(status.value("brightness").toString(), QStringLiteral("não informado"));
        QCOMPARE(status.value("brightnessUnexplained").toString(), QString{});
        QCOMPARE(status.value("reportRate").toString(), QStringLiteral("não informado"));
        QCOMPARE(status.value("supportedReportRates").toString(), QStringLiteral("não informado"));
    }

    void flagsAReportRateOutsideTheSupportedList()
    {
        auto status = capturedStatus();
        status.currentPeriodMs = 3;
        fx->source.setResult(status);
        KeyboardStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &KeyboardStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        const auto text = viewModel.status().value("reportRate").toString();
        QVERIFY2(text.startsWith(QStringLiteral("333 Hz (3 ms)")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("fora da lista")), qPrintable(text));
    }

    void showsTheSourceError_AndClearsItOnTheNextRead()
    {
        fx->source.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable,
                                                       "sem resposta (dispositivo dormindo ou desligado?)"));
        KeyboardStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &KeyboardStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        QCOMPARE(viewModel.error(), QStringLiteral("sem resposta (dispositivo dormindo ou desligado?)"));
        QVERIFY(viewModel.status().isEmpty());
        fx->source.setResult(capturedStatus());
        viewModel.read();
        QVERIFY(finished.wait(5000));
        QVERIFY(viewModel.error().isEmpty());
        QVERIFY(!viewModel.status().isEmpty());
    }

    void showsWhoHoldsTheLease_WhenTheLiveSessionIsOpen()
    {
        auto held =
            fx->registry.acquire(LeaseRequest{.interfaceId = PhysicalInterfaceId::create("hidpp:046d:407c").value(),
                                              .holder = LeaseHolder::create("lighting.live").value(),
                                              .purpose = "teste",
                                              .maxDuration = std::chrono::seconds{60}});
        QVERIFY(held.has_value());
        KeyboardStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &KeyboardStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        const auto message = viewModel.error();
        QVERIFY2(message.startsWith(QStringLiteral("ocupado")), qPrintable(message));
        QVERIFY2(message.contains(QStringLiteral("lighting.live")), qPrintable(message));
        QCOMPARE(fx->source.reads(), 0);
    }

    void ignoresASecondReadWhileReading()
    {
        std::binary_semaphore release{0};
        fx->source.setOnRead([&release] { release.acquire(); });
        KeyboardStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &KeyboardStatusViewModel::readFinished);

        viewModel.read();
        viewModel.read();
        release.release();
        QVERIFY(finished.wait(5000));

        QCOMPARE(finished.count(), 1);
        QCOMPARE(fx->source.reads(), 1);
    }

    void emitsTheChangeSignals()
    {
        KeyboardStatusViewModel viewModel{fx->useCase};
        QSignalSpy reading(&viewModel, &KeyboardStatusViewModel::readingChanged);
        QSignalSpy status(&viewModel, &KeyboardStatusViewModel::statusChanged);
        QSignalSpy finished(&viewModel, &KeyboardStatusViewModel::readFinished);

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
            KeyboardStatusViewModel viewModel{fx->useCase};
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
        ghub::fakes::FakeKeyboardStatusSource source;
        ReadKeyboardStatus useCase{source, registry};
    };

    std::unique_ptr<Fixture> fx;
};

} // namespace

QTEST_GUILESS_MAIN(KeyboardStatusViewModelTest)
#include "KeyboardStatusViewModelTest.moc"
