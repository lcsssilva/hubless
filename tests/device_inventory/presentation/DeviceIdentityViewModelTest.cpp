#include "modules/device_inventory/presentation/DeviceIdentityViewModel.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeIdentitySource.hpp"

#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <memory>
#include <semaphore>
#include <thread>

using ghub::device_access::AccessRegistry;
using ghub::device_access::LeaseHolder;
using ghub::device_access::LeaseRequest;
using ghub::device_access::PhysicalInterfaceId;
using ghub::device_inventory::DeviceIdentity;
using ghub::device_inventory::DeviceIdentityParts;
using ghub::device_inventory::DeviceIdentityViewModel;
using ghub::device_inventory::IdentityTarget;
using ghub::device_inventory::ReadDeviceIdentity;
using ghub::shared_kernel::ErrorCode;

namespace {

QVariantMap entry(const DeviceIdentityViewModel& viewModel, const char* key)
{
    return viewModel.identities().value(QString::fromLatin1(key)).toMap();
}

QString state(const DeviceIdentityViewModel& viewModel, const char* key)
{
    return entry(viewModel, key).value(QStringLiteral("state")).toString();
}

DeviceIdentity sampleIdentity()
{
    DeviceIdentityParts parts;
    parts.name = "G915 X LIGHTSPEED";
    parts.deviceType = 0;
    parts.firmwares.emplace_back(0, std::array<std::uint8_t, 3>{'M', 'P', 'K'}, 0x09, 0x04, 0x0042, true);
    parts.firmwares.emplace_back(1, std::array<std::uint8_t, 3>{'B', 'L', 'K'}, 0x01, 0x00, 0x0001, false);
    parts.featureIds = {0x0001, 0x1001, 0x1B04, 0x8071};
    return DeviceIdentity{std::move(parts)};
}

class DeviceIdentityViewModelTest : public QObject {
    Q_OBJECT

private slots:
    // Qt Test usa um único objeto para todos os testes: cada teste ganha fakes novos (sem ganchos pendentes).
    void init() { fx = std::make_unique<Fixture>(); }
    void cleanup() { fx.reset(); }

    void startsIdleForEveryDevice_AndReadsNothing()
    {
        const DeviceIdentityViewModel viewModel{fx->useCase};

        for (const char* key : {"g915", "g703", "g733"}) {
            QCOMPARE(state(viewModel, key), QStringLiteral("idle"));
        }
        QVERIFY(fx->source.targets().empty());
    }

    void showsReadingUntilTheReadEnds_ThenTheIdentity()
    {
        std::binary_semaphore release{0};
        fx->source.setOnRead([&release] { release.acquire(); });
        fx->source.setResult(sampleIdentity());
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g915"));

        QCOMPARE(state(viewModel, "g915"), QStringLiteral("reading"));
        QCOMPARE(state(viewModel, "g703"), QStringLiteral("idle"));
        release.release();
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.takeFirst().at(0).toString(), QStringLiteral("g915"));
        const auto result = entry(viewModel, "g915");
        QCOMPARE(result.value("state").toString(), QStringLiteral("ok"));
        QCOMPARE(result.value("name").toString(), QStringLiteral("G915 X LIGHTSPEED"));
        QCOMPARE(result.value("type").toString(), QStringLiteral("teclado"));
        QCOMPARE(result.value("firmwares").toStringList(),
                 (QStringList{QStringLiteral("MPK 09.04 b0042 — aplicação (ativo)"),
                              QStringLiteral("BLK 01.00 b0001 — bootloader")}));
        QCOMPARE(result.value("featureCount").toInt(), 4);
        QCOMPARE(result.value("featureIds").toStringList(),
                 (QStringList{QStringLiteral("0x0001"), QStringLiteral("0x1001"), QStringLiteral("0x1b04"),
                              QStringLiteral("0x8071")}));
        QCOMPARE(result.value("truncationNote").toString(), QString{});
        QCOMPARE(result.value("error").toString(), QString{});
        QCOMPARE(fx->source.targets(), (std::vector{IdentityTarget::G915}));
    }

    void mapsEachKeyToItsDevice()
    {
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g703"));
        QVERIFY(finished.wait(5000));
        viewModel.read(QStringLiteral("g733"));
        QVERIFY(finished.wait(5000));

        QCOMPARE(fx->source.targets(), (std::vector{IdentityTarget::G703, IdentityTarget::G733}));
        QCOMPARE(state(viewModel, "g703"), QStringLiteral("ok"));
        QCOMPARE(state(viewModel, "g733"), QStringLiteral("ok"));
        QCOMPARE(state(viewModel, "g915"), QStringLiteral("idle"));
    }

    void ignoresASecondReadOfTheSameDeviceWhileReading()
    {
        std::binary_semaphore release{0};
        fx->source.setOnRead([&release] { release.acquire(); });
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g915"));
        viewModel.read(QStringLiteral("g915"));
        release.release();
        QVERIFY(finished.wait(5000));

        QCOMPARE(finished.count(), 1);
        QCOMPARE(fx->source.targets().size(), std::size_t{1});
    }

    void readsDifferentDevicesIndependently()
    {
        std::counting_semaphore<2> release{0};
        fx->source.setOnRead([&release] { release.acquire(); });
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g915"));
        viewModel.read(QStringLiteral("g733"));

        QCOMPARE(state(viewModel, "g915"), QStringLiteral("reading"));
        QCOMPARE(state(viewModel, "g733"), QStringLiteral("reading"));
        release.release(2);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 5000);
        QCOMPARE(state(viewModel, "g915"), QStringLiteral("ok"));
        QCOMPARE(state(viewModel, "g733"), QStringLiteral("ok"));
    }

    void ignoresUnknownKeys()
    {
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy changed(&viewModel, &DeviceIdentityViewModel::identitiesChanged);

        viewModel.read(QStringLiteral("brio"));
        viewModel.read(QString{});

        QVERIFY(fx->source.targets().empty());
        QCOMPARE(changed.count(), 0);
    }

    void showsTheSourceErrorAndAllowsReadingAgain()
    {
        fx->source.setResult(
            ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "headset desligado ou fora de alcance"));
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g733"));
        QVERIFY(finished.wait(5000));

        QCOMPARE(state(viewModel, "g733"), QStringLiteral("error"));
        QCOMPARE(entry(viewModel, "g733").value("error").toString(),
                 QStringLiteral("headset desligado ou fora de alcance"));
        fx->source.setResult(sampleIdentity());
        viewModel.read(QStringLiteral("g733"));
        QVERIFY(finished.wait(5000));
        QCOMPARE(state(viewModel, "g733"), QStringLiteral("ok"));
        QCOMPARE(entry(viewModel, "g733").value("error").toString(), QString{});
    }

    void showsWhoHoldsTheLease_WhenTheDeviceIsBusy()
    {
        auto held =
            fx->registry.acquire(LeaseRequest{.interfaceId = PhysicalInterfaceId::create("hidpp:046d:407c").value(),
                                              .holder = LeaseHolder::create("lighting.live").value(),
                                              .purpose = "teste",
                                              .maxDuration = std::chrono::seconds{60}});
        QVERIFY(held.has_value());
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g915"));
        QVERIFY(finished.wait(5000));

        QCOMPARE(state(viewModel, "g915"), QStringLiteral("error"));
        const auto message = entry(viewModel, "g915").value("error").toString();
        QVERIFY2(message.startsWith(QStringLiteral("ocupado")), qPrintable(message));
        QVERIFY2(message.contains(QStringLiteral("lighting.live")), qPrintable(message));
        QVERIFY(fx->source.targets().empty());
    }

    void showsNotInformedForUnknownFields()
    {
        fx->source.setResult(DeviceIdentity{});
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g703"));
        QVERIFY(finished.wait(5000));

        const auto result = entry(viewModel, "g703");
        QCOMPARE(result.value("state").toString(), QStringLiteral("ok"));
        QCOMPARE(result.value("name").toString(), QStringLiteral("não informado"));
        QCOMPARE(result.value("type").toString(), QStringLiteral("não informado"));
        QVERIFY(result.value("firmwares").toStringList().isEmpty());
        QCOMPARE(result.value("featureCount").toInt(), 0);
    }

    void tellsWhenTheListsWereCut()
    {
        DeviceIdentityParts parts;
        parts.firmwareTruncated = true;
        parts.featuresTruncated = true;
        fx->source.setResult(DeviceIdentity{std::move(parts)});
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g915"));
        QVERIFY(finished.wait(5000));

        const auto note = entry(viewModel, "g915").value("truncationNote").toString();
        QVERIFY2(note.contains(QStringLiteral("firmware")), qPrintable(note));
        QVERIFY2(note.contains(QStringLiteral("features")), qPrintable(note));
        QVERIFY2(note.contains(QStringLiteral("8")), qPrintable(note));
        QVERIFY2(note.contains(QStringLiteral("64")), qPrintable(note));
    }

    void emitsIdentitiesChanged_WhenTheStateChanges()
    {
        DeviceIdentityViewModel viewModel{fx->useCase};
        QSignalSpy changed(&viewModel, &DeviceIdentityViewModel::identitiesChanged);
        QSignalSpy finished(&viewModel, &DeviceIdentityViewModel::readFinished);

        viewModel.read(QStringLiteral("g915"));
        QVERIFY(changed.count() >= 1);
        QVERIFY(finished.wait(5000));

        QVERIFY(changed.count() >= 2);
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
            DeviceIdentityViewModel viewModel{fx->useCase};
            viewModel.read(QStringLiteral("g915"));
        }
        QVERIFY2(ended, "o destrutor não pode voltar enquanto a leitura roda");
        releaser.join();
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    struct Fixture {
        ghub::fakes::FakeClock clock;
        AccessRegistry registry{clock};
        ghub::fakes::FakeIdentitySource source;
        ReadDeviceIdentity useCase{source, registry};
    };

    std::unique_ptr<Fixture> fx;
};

} // namespace

QTEST_GUILESS_MAIN(DeviceIdentityViewModelTest)
#include "DeviceIdentityViewModelTest.moc"
