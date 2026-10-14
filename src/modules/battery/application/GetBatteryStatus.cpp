#include "modules/battery/application/GetBatteryStatus.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>

namespace ghub::battery {

namespace {

bool hasKnownChargeState(const SourcedBatteryReading& entry)
{
    return entry.reading.has_value() && entry.reading->chargeState() != ChargeState::Unknown;
}

// Mesmo modelo em mais de uma conexão ou fonte (ex.: G703 sem fio "Unknown" + no cabo "Charging"):
// vale a primeira leitura com estado conhecido; sem nenhuma, a primeira entrada. Sem entrada, uma
// fonte que falhou globalmente (a primeira) torna a leitura indisponível; senão, não detectado.
DeviceBatteryStatus statusOf(device_inventory::DeviceModel model, const std::vector<SourcedBatteryReading>& readings,
                             const std::optional<shared_kernel::DomainError>& firstSourceFailure)
{
    auto matches = readings | std::views::filter([model](const SourcedBatteryReading& entry) {
                       const auto identified = device_inventory::identifyDevice(entry.device);
                       return identified.has_value() && identified->model == model;
                   });
    auto match = std::ranges::find_if(matches, hasKnownChargeState);
    if (match == matches.end()) {
        match = matches.begin();
    }
    if (match == matches.end()) {
        if (firstSourceFailure.has_value()) {
            return {.model = model, .state = *firstSourceFailure};
        }
        return {.model = model, .state = NotDetected{}};
    }
    if (!match->reading.has_value()) {
        return {.model = model, .state = match->reading.error()};
    }
    return {.model = model, .state = *match->reading};
}

} // namespace

std::vector<DeviceBatteryStatus> GetBatteryStatus::execute() const
{
    std::vector<SourcedBatteryReading> readings;
    std::optional<shared_kernel::DomainError> firstSourceFailure;
    for (auto* const source : sources_) {
        auto result = source->readAll();
        if (!result.has_value()) {
            if (!firstSourceFailure.has_value()) {
                firstSourceFailure = result.error();
            }
            continue;
        }
        std::ranges::move(*result, std::back_inserter(readings));
    }
    std::vector<DeviceBatteryStatus> statuses;
    statuses.reserve(kBatteryModels.size());
    for (const auto model : kBatteryModels) {
        statuses.push_back(statusOf(model, readings, firstSourceFailure));
    }
    return statuses;
}

} // namespace ghub::battery
