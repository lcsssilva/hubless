#include "modules/battery/infrastructure/HidppHeadsetEventWatcher.hpp"

#include <QSignalSpy>
#include <QTest>

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

using namespace std::chrono_literals;
using ghub::battery::FdPassiveReportSource;
using ghub::battery::HidppHeadsetEventWatcher;
using ghub::battery::PassiveReportSource;
using ghub::battery::UniqueFd;
using ghub::shared_kernel::ErrorCode;

namespace {

constexpr std::array<std::uint8_t, 20> powerOn{0x11, 0xff, 0x08, 0x00, 0x0e, 0xf1, 0x01};
constexpr std::array<std::uint8_t, 20> ownResponse{0x11, 0xff, 0x08, 0x0b, 0x0e, 0xe6, 0x01};

// Par de pipes em modo pacote (O_DIRECT, como o hidraw: um relatório por read); a ponta de leitura vira a "fonte
// passiva"; o teste escreve na outra ponta.
struct Pipe {
    int readFd = -1;
    int writeFd = -1;
    Pipe()
    {
        int fds[2];
        if (::pipe2(fds, O_DIRECT | O_NONBLOCK | O_CLOEXEC) == 0) {
            readFd = fds[0];
            writeFd = fds[1];
        }
    }
    ~Pipe()
    {
        if (readFd >= 0) {
            ::close(readFd);
        }
        closeWriter();
    }
    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    void closeWriter()
    {
        if (writeFd >= 0) {
            ::close(writeFd);
            writeFd = -1;
        }
    }
    void send(const std::array<std::uint8_t, 20>& report) const
    {
        QVERIFY2(::write(writeFd, report.data(), report.size()) == static_cast<ssize_t>(report.size()),
                 "escrita no pipe falso");
    }
    // Entrega a ponta de leitura (duplicada, para o Pipe continuar dono do original).
    [[nodiscard]] std::unique_ptr<PassiveReportSource> source() const
    {
        return std::make_unique<FdPassiveReportSource>(UniqueFd{::dup(readFd)});
    }
};

class HidppHeadsetEventWatcherTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        pipes.clear();
        pipes.push_back(std::make_unique<Pipe>());
        locations = 0;
        opens = 0;
        locateOk = true;
    }

    void broadcast_EmitsHeadsetEvent()
    {
        HidppHeadsetEventWatcher watcher{locator(), opener(), 50ms};
        QSignalSpy spy(&watcher, &HidppHeadsetEventWatcher::headsetEvent);
        watcher.start();

        pipes[0]->send(powerOn);

        QTRY_COMPARE(spy.count(), 1);
    }

    void responseToOwnRequest_DoesNotEmit()
    {
        HidppHeadsetEventWatcher watcher{locator(), opener(), 50ms};
        QSignalSpy spy(&watcher, &HidppHeadsetEventWatcher::headsetEvent);
        watcher.start();

        pipes[0]->send(ownResponse);
        pipes[0]->send(powerOn); // sentinela: quando chegar, a resposta anterior já foi processada

        QTRY_COMPARE(spy.count(), 1);
    }

    void readFailure_ReopensAfterTheRetryInterval()
    {
        pipes.push_back(std::make_unique<Pipe>());
        HidppHeadsetEventWatcher watcher{locator(), opener(), 50ms};
        QSignalSpy spy(&watcher, &HidppHeadsetEventWatcher::headsetEvent);
        watcher.start();
        QCOMPARE(opens, 1);

        pipes[0]->closeWriter(); // EOF = receptor removido

        QTRY_COMPARE(opens, 2);
        pipes[1]->send(powerOn);
        QTRY_COMPARE(spy.count(), 1);
    }

    void locateFailure_RetriesLater()
    {
        locateOk = false;
        HidppHeadsetEventWatcher watcher{locator(), opener(), 50ms};
        QSignalSpy spy(&watcher, &HidppHeadsetEventWatcher::headsetEvent);
        watcher.start();
        QCOMPARE(opens, 0);

        locateOk = true;

        QTRY_COMPARE(opens, 1);
        QVERIFY(locations >= 2);
        pipes[0]->send(powerOn);
        QTRY_COMPARE(spy.count(), 1);
    }

private:
    [[nodiscard]] HidppHeadsetEventWatcher::Locator locator()
    {
        return [this]() -> ghub::shared_kernel::Result<std::filesystem::path> {
            ++locations;
            if (!locateOk) {
                return ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "ausente");
            }
            return std::filesystem::path{"/dev/hidraw-falso"};
        };
    }

    [[nodiscard]] HidppHeadsetEventWatcher::Opener opener()
    {
        return [this](const std::filesystem::path&) -> std::expected<std::unique_ptr<PassiveReportSource>, int> {
            const auto index = static_cast<std::size_t>(opens++);
            return pipes.at(index)->source();
        };
    }

    std::vector<std::unique_ptr<Pipe>> pipes;
    int locations = 0;
    int opens = 0;
    bool locateOk = true;
};

} // namespace

QTEST_GUILESS_MAIN(HidppHeadsetEventWatcherTest)
#include "HidppHeadsetEventWatcherTest.moc"
