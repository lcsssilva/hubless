// Teste físico opt-in (GHUB_HW_TESTS=1): o adaptador de produção executa o HeadsetControl real
// (`-d 0x046d:0x0afe -b -o json`, consulta de bateria liberada em
// evidence/2026-10-13-g733-features-bateria.md §2). Exige o G733 ligado e nenhum software em conflito.
#include "modules/battery/infrastructure/HeadsetControlBatterySource.hpp"
#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/infrastructure/ProcExternalProcessProbe.hpp"
#include "modules/device_access/infrastructure/SteadyClock.hpp"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <chrono>
#include <cstdlib>
#include <string_view>

namespace {

bool enabled()
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv("GHUB_HW_TESTS");
    return value != nullptr && std::string_view{value} == "1";
}

// Slots do Qt Test não podem ser estáticos.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class HeadsetControlHardwareTest : public QObject {
    Q_OBJECT

private slots:
    void readsG733Battery_ThroughRealHeadsetControl()
    {
        if (!enabled()) {
            QSKIP("teste físico: defina GHUB_HW_TESTS=1");
        }
        using namespace std::chrono_literals;
        ghub::device_access::ProcExternalProcessProbe probe;
        const ghub::device_access::SteadyClock clock;
        ghub::device_access::AccessRegistry registry{clock};
        auto interfaceId = ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:0afe");
        QVERIFY(interfaceId.has_value());
        ghub::battery::HeadsetControlBatterySource source{
            QStandardPaths::findExecutable(QStringLiteral("headsetcontrol")), probe, registry, *interfaceId, 1h, 15s};
        QSignalSpy changed(&source, &ghub::battery::HeadsetControlBatterySource::readingsChanged);

        QVERIFY(changed.wait(20000));

        const auto readings = source.readAll();
        QVERIFY(readings.has_value());
        QCOMPARE(readings->size(), 1U);
        const auto& entry = readings->front();
        QVERIFY2(entry.reading.has_value(), entry.reading.has_value() ? "" : entry.reading.error().detail.c_str());
        const auto level = entry.reading->currentLevel();
        qInfo("G733: estado %d, nível %s", static_cast<int>(entry.reading->chargeState()),
              level ? qPrintable(QString::number(level->percent())) : "oculto");
        QVERIFY(entry.reading->chargeState() != ghub::battery::ChargeState::Unknown);
        QVERIFY(level.has_value());
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(HeadsetControlHardwareTest)
#include "HeadsetControlHardwareTest.moc"
