#include "modules/battery/infrastructure/HidppBatterySource.hpp"

#include <QtConcurrent>

#include <utility>

namespace ghub::battery {

namespace {

constexpr device_inventory::UsbId g733{.vendorId = 0x046d, .productId = 0x0afe};
constexpr auto leaseHolder = "battery.hidpp";
constexpr auto leasePurpose = "consulta de bateria do G733";
// Um pedido HID++ (prazo de 2 s) mais a consulta Root, com folga.
constexpr std::chrono::seconds leaseDuration{5};

shared_kernel::Result<BatteryReading> readUnderLease(const HidppBatterySource::Reader& reader,
                                                     device_access::AccessRegistry& registry,
                                                     const device_access::PhysicalInterfaceId& interfaceId)
{
    auto holder = device_access::LeaseHolder::create(leaseHolder);
    if (!holder.has_value()) {
        return std::unexpected(holder.error());
    }
    const auto lease = registry.acquire({
        .interfaceId = interfaceId,
        .holder = std::move(*holder),
        .purpose = leasePurpose,
        .maxDuration = leaseDuration,
    });
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    return reader();
}

} // namespace

HidppBatterySource::HidppBatterySource(Reader reader, device_access::AccessRegistry& registry,
                                       device_access::PhysicalInterfaceId interfaceId,
                                       std::chrono::milliseconds pollInterval, QObject* parent)
    : QObject(parent), reader_(std::move(reader)), registry_(registry), interfaceId_(std::move(interfaceId)),
      pollTimer_(this)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &HidppBatterySource::onFinished);
    pollTimer_.setInterval(pollInterval);
    connect(&pollTimer_, &QTimer::timeout, this, &HidppBatterySource::requestUpdate);
    pollTimer_.start();
    QTimer::singleShot(0, this, &HidppBatterySource::requestUpdate);
}

HidppBatterySource::~HidppBatterySource()
{
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

shared_kernel::Result<std::vector<SourcedBatteryReading>> HidppBatterySource::readAll()
{
    return cache_;
}

void HidppBatterySource::requestUpdate()
{
    if (running_) {
        return;
    }
    running_ = true;
    watcher_.setFuture(QtConcurrent::run([this] { return readUnderLease(reader_, registry_, interfaceId_); }));
}

void HidppBatterySource::onFinished()
{
    auto outcome = watcher_.result();
    running_ = false;
    cache_.clear();
    cache_.push_back(SourcedBatteryReading{.device = g733, .reading = std::move(outcome)});
    emit readingsChanged();
}

} // namespace ghub::battery
