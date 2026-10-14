#include "modules/headset/application/ChangeHeadsetSetting.hpp"

#include <chrono>
#include <string>
#include <utility>

namespace ghub::headset {

namespace {

constexpr auto leaseHolder = "headset.headsetcontrol";
constexpr auto leasePurpose = "escrita de sidetone/luzes";
// Uma execução do HeadsetControl (timeout de 10 s) com folga (ADR-0015 decisão 4).
constexpr std::chrono::seconds leaseDuration{15};

// Probe -> lease -> verify -> `apply` (uma vez). Devolve `applied` se tudo der certo.
template <typename Value, typename Apply>
shared_kernel::Result<Value>
guarded(device_access::ExternalProcessProbe& probe, device_access::AccessRegistry& registry,
        const device_access::PhysicalInterfaceId& interfaceId, Value applied, const Apply& apply)
{
    const auto owners = probe.runningOwners();
    if (!owners.has_value()) {
        return std::unexpected(owners.error());
    }
    if (!owners->empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::DeviceBusy,
                                   "em uso por " + std::string{device_access::processName(owners->front())});
    }
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
    // Expiração ou revogação desde a aquisição impedem a escrita.
    if (const auto held = lease->verify(); !held.has_value()) {
        return std::unexpected(held.error());
    }
    if (const auto done = apply(); !done.has_value()) {
        return std::unexpected(done.error());
    }
    return applied;
}

} // namespace

shared_kernel::Result<SidetoneLevel> ChangeHeadsetSetting::setSidetone(SidetoneLevel level) const
{
    return guarded(*probe_, *registry_, interfaceId_, level, [&] { return writer_->applySidetone(level); });
}

shared_kernel::Result<HeadsetLights> ChangeHeadsetSetting::setLights(HeadsetLights lights) const
{
    return guarded(*probe_, *registry_, interfaceId_, lights, [&] { return writer_->applyLights(lights); });
}

} // namespace ghub::headset
