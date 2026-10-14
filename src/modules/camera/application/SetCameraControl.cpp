#include "modules/camera/application/SetCameraControl.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace ghub::camera {

namespace {

constexpr std::string_view leaseHolder = "camera.v4l2";
constexpr std::string_view leasePurpose = "escrita de controle UVC";
// Duas transferências de controle de até 5 s no driver, com folga (ADR-0010 decisão 4).
constexpr std::chrono::seconds leaseDuration{15};

} // namespace

SetCameraControl::SetCameraControl(CameraControlAccess& access, device_access::AccessRegistry& registry,
                                   device_access::PhysicalInterfaceId interfaceId)
    : access_(&access), registry_(&registry), interfaceId_(std::move(interfaceId))
{
}

shared_kernel::Result<CameraControl> SetCameraControl::setValue(std::uint32_t id, std::int64_t value) const
{
    return write(id, Target::GivenValue, value);
}

shared_kernel::Result<CameraControl> SetCameraControl::restoreDefault(std::uint32_t id) const
{
    return write(id, Target::DefaultValue, 0);
}

shared_kernel::Result<CameraControl> SetCameraControl::write(std::uint32_t id, Target target, std::int64_t value) const
{
    auto holder = device_access::LeaseHolder::create(std::string{leaseHolder});
    if (!holder.has_value()) {
        return std::unexpected(holder.error());
    }
    auto lease = registry_->acquire({
        .interfaceId = interfaceId_,
        .holder = std::move(*holder),
        .purpose = std::string{leasePurpose},
        .maxDuration = leaseDuration,
    });
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    // A sessão e o lease são liberados em qualquer saída (RAII); a sessão fecha antes do lease.
    auto session = access_->open();
    if (!session.has_value()) {
        return std::unexpected(session.error());
    }
    if (*session == nullptr) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "sessão de controle ausente");
    }
    auto before = (*session)->readControl(id);
    if (!before.has_value()) {
        return std::unexpected(before.error());
    }
    const std::int64_t wanted = target == Target::DefaultValue ? before->defaultValue() : value;
    if (const auto valid = before->validateValue(wanted); !valid.has_value()) {
        return std::unexpected(valid.error());
    }
    // Expiração ou revogação desde o início da operação impedem a escrita.
    if (const auto held = lease->verify(); !held.has_value()) {
        return std::unexpected(held.error());
    }
    if (const auto written = (*session)->writeControl(id, wanted); !written.has_value()) {
        return std::unexpected(written.error());
    }
    return (*session)->readControl(id);
}

} // namespace ghub::camera
