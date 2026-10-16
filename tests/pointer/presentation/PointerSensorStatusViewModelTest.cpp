#include "modules/pointer/presentation/PointerSensorStatusViewModel.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakePointerSensorSource.hpp"

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
using ghub::pointer::DpiList;
using ghub::pointer::PointerSensorStatus;
using ghub::pointer::PointerSensorStatusViewModel;
using ghub::pointer::ReadPointerSensorStatus;
using ghub::pointer::ReportRateSupport;
using ghub::shared_kernel::ErrorCode;

namespace {

// Bytes da captura C-00 do G703 (INC-015 parte 2).
PointerSensorStatus capturedStatus()
{
    PointerSensorStatus status;
    status.dpiList = DpiList::parse(std::vector<std::uint8_t>{0x00, 0x00, 0x64, 0xE0, 0x32, 0x64, 0x00});
    status.dpiSetting = ghub::pointer::parseDpiSetting(std::vector<std::uint8_t>{0x00, 0x1B, 0x8A, 0x03, 0x20}).value();
    status.reportRateSupport = ReportRateSupport::fromBitmap(0x8B).value();
    status.currentPeriodMs = 1;
    return status;
}

class PointerSensorStatusViewModelTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fx = std::make_unique<Fixture>(); }
    void cleanup() { fx.reset(); }

    void constructionReadsNothing()
    {
        const PointerSensorStatusViewModel viewModel{fx->useCase};

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
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

        viewModel.read();

        QVERIFY(viewModel.reading());
        release.release();
        QVERIFY(finished.wait(5000));
        QVERIFY(!viewModel.reading());
        QVERIFY(viewModel.error().isEmpty());
        const auto status = viewModel.status();
        QCOMPARE(status.value("dpiRange").toString(), QStringLiteral("100–25600, passo 50"));
        QCOMPARE(status.value("dpi").toString(), QStringLiteral("7050 (padrão 800)"));
        QCOMPARE(status.value("reportRate").toString(), QStringLiteral("1000 Hz (1 ms)"));
        QCOMPARE(status.value("supportedReportRates").toString(), QStringLiteral("1000, 500, 250, 125 Hz"));
    }

    void formatsLooseValuesAndRangesTogether()
    {
        auto status = capturedStatus();
        status.dpiList = DpiList::parse(
            std::vector<std::uint8_t>{0x00, 0x01, 0x90, 0x03, 0x20, 0xE0, 0x64, 0x06, 0x40, 0x0C, 0x80, 0x00, 0x00});
        fx->source.setResult(status);
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        QCOMPARE(viewModel.status().value("dpiRange").toString(), QStringLiteral("400; 800–1600, passo 100; 3200"));
    }

    void showsRawBytesWhenTheDpiListIsInvalid()
    {
        auto status = capturedStatus();
        status.dpiList = DpiList::parse(std::vector<std::uint8_t>{0x00, 0x00, 0x64, 0xE0, 0x00, 0x64, 0x00});
        fx->source.setResult(status);
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        QCOMPARE(viewModel.status().value("dpiRange").toString(),
                 QStringLiteral("lista inválida (bytes brutos: 00 00 64 e0 00 64 00)"));
    }

    void showsNotInformedForAbsentFields()
    {
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        const auto status = viewModel.status();
        for (const char* key : {"dpiRange", "dpi", "reportRate", "supportedReportRates"}) {
            QCOMPARE(status.value(key).toString(), QStringLiteral("não informado"));
        }
    }

    void showsTheSourceError_AndClearsItOnTheNextRead()
    {
        fx->source.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable,
                                                       "sem resposta (dispositivo dormindo ou desligado?)"));
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

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

    void showsWhoHoldsTheLease_WhenTheDeviceIsBusy()
    {
        auto held =
            fx->registry.acquire(LeaseRequest{.interfaceId = PhysicalInterfaceId::create("hidpp:046d:c539").value(),
                                              .holder = LeaseHolder::create("pointer.onboard").value(),
                                              .purpose = "teste",
                                              .maxDuration = std::chrono::seconds{60}});
        QVERIFY(held.has_value());
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

        viewModel.read();
        QVERIFY(finished.wait(5000));

        const auto message = viewModel.error();
        QVERIFY2(message.startsWith(QStringLiteral("ocupado")), qPrintable(message));
        QVERIFY2(message.contains(QStringLiteral("pointer.onboard")), qPrintable(message));
        QCOMPARE(fx->source.reads(), 0);
    }

    void ignoresASecondReadWhileReading()
    {
        std::binary_semaphore release{0};
        fx->source.setOnRead([&release] { release.acquire(); });
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

        viewModel.read();
        viewModel.read();
        release.release();
        QVERIFY(finished.wait(5000));

        QCOMPARE(finished.count(), 1);
        QCOMPARE(fx->source.reads(), 1);
    }

    void emitsTheChangeSignals()
    {
        PointerSensorStatusViewModel viewModel{fx->useCase};
        QSignalSpy reading(&viewModel, &PointerSensorStatusViewModel::readingChanged);
        QSignalSpy status(&viewModel, &PointerSensorStatusViewModel::statusChanged);
        QSignalSpy finished(&viewModel, &PointerSensorStatusViewModel::readFinished);

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
            PointerSensorStatusViewModel viewModel{fx->useCase};
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
        ghub::fakes::FakePointerSensorSource source;
        ReadPointerSensorStatus useCase{source, registry};
    };

    std::unique_ptr<Fixture> fx;
};

} // namespace

QTEST_GUILESS_MAIN(PointerSensorStatusViewModelTest)
#include "PointerSensorStatusViewModelTest.moc"
