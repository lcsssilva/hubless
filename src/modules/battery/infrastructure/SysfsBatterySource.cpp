#include "modules/battery/infrastructure/SysfsBatterySource.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ghub::battery {

namespace {

namespace fs = std::filesystem;

using shared_kernel::ErrorCode;
using shared_kernel::fail;
using shared_kernel::Result;

// Atributos sysfs cabem numa página; acima disso o conteúdo não é do ABI (KERNEL_POWER_SUPPLY.md §7).
constexpr std::size_t maxAttributeBytes = 4096;

// Conteúdo do atributo sem o '\n' final; nullopt se o arquivo não existe ou não abre.
Result<std::optional<std::string>> readAttribute(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return std::optional<std::string>{};
    }
    std::string content(maxAttributeBytes + 1, '\0');
    stream.read(content.data(), static_cast<std::streamsize>(content.size()));
    content.resize(static_cast<std::size_t>(stream.gcount()));
    if (content.size() > maxAttributeBytes) {
        return fail(ErrorCode::MalformedData, file.filename().string() + " excede 4 KiB");
    }
    if (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }
    return std::optional<std::string>{std::move(content)};
}

// VID:PID do diretório apontado pelo link `device`; vazio se o link não existe ou não é HID.
std::optional<device_inventory::UsbId> hidIdOf(const fs::path& entry)
{
    std::error_code error;
    const auto target = fs::read_symlink(entry / "device", error);
    if (error) {
        return std::nullopt;
    }
    return device_inventory::parseHidDeviceName(target.filename().string());
}

// Somente baterias de periférico (hid-logitech-hidpp registra type=Battery, scope=Device);
// bateria do notebook, fontes AC/USB e entradas sem scope ficam de fora.
bool isPeripheralBattery(const fs::path& entry)
{
    const auto hasValue = [&entry](const char* attribute, std::string_view expected) {
        const auto content = readAttribute(entry / attribute);
        return content.has_value() && content->has_value() && **content == expected;
    };
    return hasValue("type", "Battery") && hasValue("scope", "Device");
}

// Valores de `status` no ABI sysfs-class-power (kernel v7.2.9).
std::optional<ChargeState> parseStatus(std::string_view text)
{
    static constexpr auto table = std::to_array<std::pair<std::string_view, ChargeState>>({
        {"Unknown", ChargeState::Unknown},
        {"Charging", ChargeState::Charging},
        {"Discharging", ChargeState::Discharging},
        {"Not charging", ChargeState::NotCharging},
        {"Full", ChargeState::Full},
    });
    for (const auto& [name, state] : table) {
        if (text == name) {
            return state;
        }
    }
    return std::nullopt;
}

// `capacity` é opcional: o driver só o registra quando o dispositivo informa percentual.
Result<std::optional<BatteryLevel>> parseCapacity(const std::optional<std::string>& text)
{
    if (!text.has_value()) {
        return std::optional<BatteryLevel>{};
    }
    int percent{};
    const auto* const end = std::to_address(text->end());
    const auto [stop, error] = std::from_chars(text->data(), end, percent);
    if (error != std::errc{} || stop != end) {
        return fail(ErrorCode::MalformedData, "capacity não numérico");
    }
    const auto level = BatteryLevel::fromPercent(percent);
    if (!level.has_value()) {
        return fail(ErrorCode::MalformedData, "capacity fora de 0-100");
    }
    return std::optional<BatteryLevel>{*level};
}

Result<BatteryReading> readEntry(const fs::path& entry)
{
    const auto statusText = readAttribute(entry / "status");
    if (!statusText.has_value()) {
        return std::unexpected(statusText.error());
    }
    const auto state = parseStatus(statusText->value_or(""));
    if (!state.has_value()) {
        return fail(ErrorCode::MalformedData, "status não reconhecido");
    }
    const auto capacityText = readAttribute(entry / "capacity");
    if (!capacityText.has_value()) {
        return std::unexpected(capacityText.error());
    }
    return parseCapacity(*capacityText).transform([&state](std::optional<BatteryLevel> level) {
        return BatteryReading{*state, level};
    });
}

} // namespace

Result<std::vector<SourcedBatteryReading>> SysfsBatterySource::readAll()
{
    // Ordem do diretório não é especificada; ordenar torna "primeira entrada" determinística (INC-002 S1).
    // increment(error) em vez de range-for: operator++ lança, e readAll() roda em slot de QTimer.
    std::error_code error;
    std::vector<fs::path> paths;
    for (fs::directory_iterator it(classRoot_, error); !error && it != fs::directory_iterator{}; it.increment(error)) {
        paths.push_back(it->path());
    }
    if (error) {
        return fail(ErrorCode::BackendUnavailable, classRoot_.string() + ": " + error.message());
    }
    std::ranges::sort(paths);

    std::vector<SourcedBatteryReading> readings;
    for (const auto& path : paths) {
        if (!isPeripheralBattery(path)) {
            continue;
        }
        const auto device = hidIdOf(path);
        if (!device.has_value()) {
            continue;
        }
        readings.push_back({.device = *device, .reading = readEntry(path)});
    }
    return readings;
}

} // namespace ghub::battery
