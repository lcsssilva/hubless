#include "modules/headset/application/ChangeHeadsetLighting.hpp"

#include <chrono>

namespace ghub::headset {

namespace {

constexpr auto leaseHolder = "headset.lighting";
constexpr auto leasePurpose = "cor/efeito de zona do headset";
constexpr std::chrono::seconds leaseDuration{5};

} // namespace

shared_kernel::Result<HeadsetLighting> ChangeHeadsetLighting::apply(HeadsetLighting lighting) const
{
    auto holder = device_access::LeaseHolder::create(leaseHolder);
    if (!holder.has_value()) {
        return std::unexpected(holder.error());
    }
    const auto lease = registry_->acquire({
        .interfaceId = interfaceId_,
        .holder = std::move(*holder),
        .purpose = leasePurpose,
        .maxDuration = leaseDuration,
    });
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    if (const auto held = lease->verify(); !held.has_value()) {
        return std::unexpected(held.error());
    }
    if (const auto done = writer_->applyLighting(lighting); !done.has_value()) {
        return std::unexpected(done.error());
    }
    return lighting;
}

} // namespace ghub::headset
