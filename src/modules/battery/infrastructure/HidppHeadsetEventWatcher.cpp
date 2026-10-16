#include "modules/battery/infrastructure/HidppHeadsetEventWatcher.hpp"

#include "modules/battery/domain/HeadsetBroadcast.hpp"

#include <array>
#include <utility>

namespace ghub::battery {

HidppHeadsetEventWatcher::HidppHeadsetEventWatcher(Locator locator, Opener opener,
                                                   std::chrono::milliseconds retryInterval, QObject* parent)
    : QObject(parent), locator_(std::move(locator)), opener_(std::move(opener)), retryTimer_(this)
{
    retryTimer_.setSingleShot(true);
    retryTimer_.setInterval(retryInterval);
    connect(&retryTimer_, &QTimer::timeout, this, &HidppHeadsetEventWatcher::tryOpen);
}

HidppHeadsetEventWatcher::~HidppHeadsetEventWatcher() = default;

void HidppHeadsetEventWatcher::start()
{
    tryOpen();
}

void HidppHeadsetEventWatcher::tryOpen()
{
    const auto path = locator_();
    if (!path.has_value()) {
        retryTimer_.start();
        return;
    }
    auto source = opener_(*path);
    if (!source.has_value()) {
        retryTimer_.start();
        return;
    }
    source_ = std::move(*source);
    notifier_ = std::make_unique<QSocketNotifier>(source_->fd(), QSocketNotifier::Read, this);
    connect(notifier_.get(), &QSocketNotifier::activated, this, &HidppHeadsetEventWatcher::onReadable);
}

void HidppHeadsetEventWatcher::onReadable()
{
    std::array<std::uint8_t, 64> buffer{};
    bool broadcast = false;
    while (source_) {
        const auto n = source_->readReport(buffer);
        if (!n.has_value()) {
            closeAndRetry();
            break;
        }
        if (*n == 0) {
            break;
        }
        // Qualquer outro relatório é descartado sem registro: pode ser tecla do headset.
        broadcast = broadcast || isHidppBroadcastFromHeadset(std::span<const std::uint8_t>{buffer.data(), *n});
    }
    if (broadcast) {
        emit headsetEvent();
    }
}

void HidppHeadsetEventWatcher::closeAndRetry()
{
    if (notifier_) {
        notifier_->setEnabled(false);
        notifier_.release()->deleteLater();
    }
    source_.reset();
    retryTimer_.start();
}

} // namespace ghub::battery
