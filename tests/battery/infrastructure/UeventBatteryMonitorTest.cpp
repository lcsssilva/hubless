#include "modules/battery/infrastructure/UeventBatteryMonitor.hpp"

#include <QSignalSpy>
#include <QTest>

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

using ghub::battery::ReceivedDatagram;
using ghub::battery::UeventBatteryMonitor;
using ghub::battery::UniqueFd;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

constexpr auto powerSupplyChange = "change@/devices/x/power_supply/hidpp_battery_0\0ACTION=change\0"
                                   "DEVPATH=/devices/x/power_supply/hidpp_battery_0\0SUBSYSTEM=power_supply\0"sv;

// Par de sockets de datagrama: o monitor lê de `monitorEnd`, o teste escreve em `peer`.
struct DatagramPair {
    DatagramPair()
    {
        std::array<int, 2> fds{-1, -1};
        if (::socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds.data()) != 0) {
            qFatal("socketpair falhou");
        }
        monitorEnd = UniqueFd{fds[0]};
        peer = UniqueFd{fds[1]};
    }

    void send(std::string_view datagram) const
    {
        QVERIFY(::send(peer.get(), datagram.data(), datagram.size(), 0) == static_cast<ssize_t>(datagram.size()));
    }

    UniqueFd monitorEnd;
    UniqueFd peer;
};

// Receptor falso: lê do socket e declara como remetente um socket netlink com o portid dado
// (0 = kernel).
ghub::battery::DatagramReceiver receiveFromPort(std::uint32_t portId)
{
    return [portId](int fd, std::span<char> buffer) -> std::optional<ReceivedDatagram> {
        const auto size = ::recv(fd, buffer.data(), buffer.size(), MSG_DONTWAIT | MSG_TRUNC);
        if (size < 0) {
            return std::nullopt;
        }
        sockaddr_nl sender{};
        sender.nl_family = AF_NETLINK;
        sender.nl_pid = portId;
        return ReceivedDatagram{
            .size = std::min(static_cast<std::size_t>(size), buffer.size()),
            .truncated = static_cast<std::size_t>(size) > buffer.size(),
            .sender = sender,
            .senderLength = sizeof(sockaddr_nl),
        };
    };
}

std::optional<ReceivedDatagram> receiveAsKernel(int fd, std::span<char> buffer)
{
    return receiveFromPort(0)(fd, buffer);
}

// Verdadeiro quando não há datagrama pendente no socket (o monitor já leu tudo).
bool isDrained(const UniqueFd& observer)
{
    char probe{};
    return ::recv(observer.get(), &probe, 1, MSG_DONTWAIT | MSG_PEEK) < 0 && errno == EAGAIN;
}

// Slots do Qt Test não podem ser estáticos.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class UeventBatteryMonitorTest : public QObject {
    Q_OBJECT

private slots:
    void emitsBatteryChanged_ForKernelPowerSupplyUevent()
    {
        DatagramPair pair;
        UeventBatteryMonitor monitor{std::move(pair.monitorEnd), receiveAsKernel};
        QSignalSpy changed(&monitor, &UeventBatteryMonitor::batteryChanged);

        pair.send(powerSupplyChange);

        QVERIFY(changed.wait(5000));
        QCOMPARE(changed.count(), 1);
    }

    void ignoresUevents_FromUserspaceSenders()
    {
        DatagramPair pair;
        const UniqueFd observer{::dup(pair.monitorEnd.get())};
        UeventBatteryMonitor monitor{std::move(pair.monitorEnd), receiveFromPort(1234)};
        QSignalSpy changed(&monitor, &UeventBatteryMonitor::batteryChanged);

        pair.send(powerSupplyChange);

        QTRY_VERIFY(isDrained(observer));
        QCOMPARE(changed.count(), 0);
    }

    void ignoresTruncatedDatagrams()
    {
        DatagramPair pair;
        const UniqueFd observer{::dup(pair.monitorEnd.get())};
        UeventBatteryMonitor monitor{std::move(pair.monitorEnd), receiveAsKernel};
        QSignalSpy changed(&monitor, &UeventBatteryMonitor::batteryChanged);

        pair.send(std::string{powerSupplyChange} + std::string(9000, 'x'));

        QTRY_VERIFY(isDrained(observer));
        QCOMPARE(changed.count(), 0);
    }

    void emitsOnce_ForBurstOfUevents()
    {
        DatagramPair pair;
        const UniqueFd observer{::dup(pair.monitorEnd.get())};
        UeventBatteryMonitor monitor{std::move(pair.monitorEnd), receiveAsKernel};
        QSignalSpy changed(&monitor, &UeventBatteryMonitor::batteryChanged);

        pair.send(powerSupplyChange);
        pair.send(powerSupplyChange);
        pair.send(powerSupplyChange);

        QVERIFY(changed.wait(5000));
        QVERIFY(isDrained(observer));
        QCOMPARE(changed.count(), 1);
    }

    void drainsRealSocket_AndRejectsNonNetlinkSender()
    {
        DatagramPair pair;
        const UniqueFd observer{::dup(pair.monitorEnd.get())};
        UeventBatteryMonitor monitor{std::move(pair.monitorEnd)};
        QSignalSpy changed(&monitor, &UeventBatteryMonitor::batteryChanged);

        pair.send(powerSupplyChange);

        QTRY_VERIFY(isDrained(observer));
        QCOMPARE(changed.count(), 0);
    }

    void emitsBatteryChanged_WhenUeventsWereDropped()
    {
        DatagramPair pair;
        bool reportedOverrun = false;
        auto overrunOnce = [&reportedOverrun](int fd, std::span<char> buffer) -> std::optional<ReceivedDatagram> {
            char discard{};
            if (reportedOverrun || ::recv(fd, &discard, 1, MSG_DONTWAIT) < 0) {
                return std::nullopt;
            }
            reportedOverrun = true;
            static_cast<void>(buffer);
            return ReceivedDatagram{.size = 0, .truncated = false, .sender = {}, .senderLength = 0, .overrun = true};
        };
        UeventBatteryMonitor monitor{std::move(pair.monitorEnd), overrunOnce};
        QSignalSpy changed(&monitor, &UeventBatteryMonitor::batteryChanged);

        pair.send("x");

        QVERIFY(changed.wait(5000));
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(UeventBatteryMonitorTest)
#include "UeventBatteryMonitorTest.moc"
