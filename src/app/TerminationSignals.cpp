#include "TerminationSignals.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>

namespace ghub::app {

namespace {

constexpr std::array<int, 3> handledSignals{SIGTERM, SIGINT, SIGHUP};

// Lado de escrita do pipe da instância ativa (-1 = nenhuma). Lido pelo tratador: precisa ser lock-free.
std::atomic<int> activeWriteFd{-1}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): estado do tratador
static_assert(std::atomic<int>::is_always_lock_free);

// Tratador async-signal-safe: só write() de um byte com o número do sinal; preserva errno.
extern "C" void forwardSignal(int signalNumber)
{
    const int savedErrno = errno;
    const int fd = activeWriteFd.load(std::memory_order_relaxed);
    if (fd >= 0) {
        const auto byte = static_cast<unsigned char>(signalNumber);
        (void)::write(fd, &byte, 1); // pipe cheio: o pedido anterior ainda não lido já encerra o app
    }
    errno = savedErrno;
}

} // namespace

TerminationSignals::TerminationSignals(QObject* parent) : QObject(parent)
{
    std::array<int, 2> fds{-1, -1};
    if (::pipe2(fds.data(), O_CLOEXEC | O_NONBLOCK) != 0) {
        return;
    }
    int expected = -1;
    if (!activeWriteFd.compare_exchange_strong(expected, fds.at(1))) {
        ::close(fds.at(0));
        ::close(fds.at(1));
        return; // outra instância ativa
    }
    readFd_ = fds.at(0);
    writeFd_ = fds.at(1);

    struct sigaction action{};
    action.sa_handler = forwardSignal;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    for (std::size_t i = 0; i < handledSignals.size(); ++i) {
        if (::sigaction(handledSignals.at(i), &action, &previous_.at(i)) != 0) {
            for (std::size_t j = 0; j < i; ++j) {
                ::sigaction(handledSignals.at(j), &previous_.at(j), nullptr);
            }
            activeWriteFd.store(-1);
            ::close(readFd_);
            ::close(writeFd_);
            readFd_ = writeFd_ = -1;
            return;
        }
    }
    notifier_ = std::make_unique<QSocketNotifier>(readFd_, QSocketNotifier::Read);
    connect(notifier_.get(), &QSocketNotifier::activated, this, &TerminationSignals::drain);
}

TerminationSignals::~TerminationSignals()
{
    if (notifier_ == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < handledSignals.size(); ++i) {
        ::sigaction(handledSignals.at(i), &previous_.at(i), nullptr);
    }
    activeWriteFd.store(-1);
    notifier_.reset();
    ::close(readFd_);
    ::close(writeFd_);
}

void TerminationSignals::drain()
{
    unsigned char byte = 0;
    while (::read(readFd_, &byte, 1) == 1) {
        emit terminationRequested(static_cast<int>(byte));
    }
}

} // namespace ghub::app
