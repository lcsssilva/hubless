#include "modules/headset/presentation/HeadsetLightingController.hpp"
#include "fakes/FakeClock.hpp"

#include <QSignalSpy>
#include <QTest>

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

using ghub::device_access::AccessRegistry;
using ghub::device_access::PhysicalInterfaceId;
using ghub::headset::ChangeHeadsetLighting;
using ghub::headset::HeadsetLighting;
using ghub::headset::HeadsetLightingController;
using ghub::headset::HeadsetLightingWriter;
using ghub::shared_kernel::ErrorCode;

namespace {

class RecordingWriter final : public HeadsetLightingWriter {
public:
    [[nodiscard]] ghub::shared_kernel::Result<void> applyLighting(HeadsetLighting lighting) override
    {
        const std::scoped_lock lock{mutex};
        applied.push_back(lighting.payload());
        return result;
    }

    std::mutex mutex;
    std::vector<std::vector<std::uint8_t>> applied;
    ghub::shared_kernel::Result<void> result;
};

struct Fixture {
    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    RecordingWriter writer;
    ChangeHeadsetLighting useCase{writer, registry, PhysicalInterfaceId::create("hidpp:046d:0afe").value()};
};

class HeadsetLightingControllerTest : public QObject {
    Q_OBJECT

private slots:
    void init() { fixture = std::make_unique<Fixture>(); }
    void cleanup() { fixture.reset(); }

    void construction_WritesNothing()
    {
        const HeadsetLightingController controller{fixture->useCase};

        QVERIFY(fixture->writer.applied.empty());
        QVERIFY(!controller.busy());
        QVERIFY(controller.status().isEmpty());
    }

    void apply_SendsTheBytesOfTheChosenEffect_ThenReportsVolatileSuccess()
    {
        HeadsetLightingController controller{fixture->useCase};
        QSignalSpy changed(&controller, &HeadsetLightingController::lightingChanged);

        controller.apply(1, 2, 0, 255, 255, 5000, 100); // zona superior, respiração

        QTRY_VERIFY(!controller.busy() && !controller.status().isEmpty());
        QCOMPARE(fixture->writer.applied.size(), std::size_t{1});
        QCOMPARE(fixture->writer.applied.front(),
                 (std::vector<std::uint8_t>{0x01, 0x02, 0x00, 0xFF, 0xFF, 0x13, 0x88, 0x00, 0x64}));
        QVERIFY(controller.status().contains(QStringLiteral("volátil")));
        QVERIFY(changed.count() >= 2);
    }

    void apply_RejectsValuesOutsideTheRangesWithoutWriting()
    {
        HeadsetLightingController controller{fixture->useCase};

        controller.apply(2, 0, 0, 0, 0, 5000, 100); // zona 2
        QVERIFY(controller.status().startsWith(QStringLiteral("Falha")));
        controller.apply(0, 4, 0, 0, 0, 5000, 100); // efeito 4
        QVERIFY(controller.status().startsWith(QStringLiteral("Falha")));
        controller.apply(0, 3, 0, 0, 0, 999, 100); // período
        QVERIFY(controller.status().startsWith(QStringLiteral("Falha")));
        controller.apply(0, 3, 256, 0, 0, 5000, 100); // cor
        QVERIFY(controller.status().startsWith(QStringLiteral("Falha")));

        QVERIFY(!controller.busy());
        QVERIFY(fixture->writer.applied.empty());
    }

    void apply_FailureShowsTheDetail()
    {
        fixture->writer.result =
            ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "headset desligado ou fora de alcance");
        HeadsetLightingController controller{fixture->useCase};

        controller.apply(0, 0, 0, 0, 0, 0, 0);

        QTRY_VERIFY(!controller.busy() && !controller.status().isEmpty());
        QCOMPARE(controller.status(), QStringLiteral("Falha: headset desligado ou fora de alcance"));
    }

private:
    std::unique_ptr<Fixture> fixture;
};

} // namespace

QTEST_GUILESS_MAIN(HeadsetLightingControllerTest)
#include "HeadsetLightingControllerTest.moc"
