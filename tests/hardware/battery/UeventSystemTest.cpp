// Teste de sistema opt-in (GHUB_HW_TESTS=1): socket de uevent real do kernel, somente leitura.
// O caso interativo exige também GHUB_HW_INTERACTIVE=1 e uma ação física do usuário (ex.: ligar o
// cabo de carga do G703/G915) dentro de 300 s.
#include "modules/battery/infrastructure/KernelUevent.hpp"
#include "modules/battery/infrastructure/UeventBatteryMonitor.hpp"

#include <QSignalSpy>
#include <QTest>

#include <cstdlib>
#include <string_view>
#include <utility>

namespace {

bool enabled(const char* variable)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): lido antes de qualquer thread; o teste não altera o ambiente.
    const char* value = std::getenv(variable);
    return value != nullptr && std::string_view{value} == "1";
}

// Slots do Qt Test não podem ser estáticos.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class UeventSystemTest : public QObject {
    Q_OBJECT

private slots:
    void opensKernelUeventSocket_WithoutPrivilege()
    {
        if (!enabled("GHUB_HW_TESTS")) {
            QSKIP("teste de sistema: defina GHUB_HW_TESTS=1");
        }
        auto socket = ghub::battery::openKernelUeventSocket();

        QVERIFY2(socket.has_value(), socket.has_value() ? "" : socket.error().detail.c_str());
        QVERIFY(socket->valid());
    }

    void receivesPowerSupplyUevent_FromKernel()
    {
        if (!enabled("GHUB_HW_TESTS") || !enabled("GHUB_HW_INTERACTIVE")) {
            QSKIP("interativo: defina GHUB_HW_TESTS=1 e GHUB_HW_INTERACTIVE=1 e ligue/desligue o cabo de carga");
        }
        auto socket = ghub::battery::openKernelUeventSocket();
        QVERIFY(socket.has_value());
        ghub::battery::UeventBatteryMonitor monitor{std::move(*socket)};
        QSignalSpy changed(&monitor, &ghub::battery::UeventBatteryMonitor::batteryChanged);

        qInfo("aguardando uevent de power_supply (até 300 s)...");

        QVERIFY(changed.wait(300000));
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(UeventSystemTest)
#include "UeventSystemTest.moc"
