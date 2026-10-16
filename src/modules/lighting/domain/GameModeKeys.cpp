#include "modules/lighting/domain/GameModeKeys.hpp"

#include "modules/lighting/domain/OnboardSlot.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace ghub::lighting::domain {

namespace {

constexpr std::uint8_t zoneKeys = 0x01;
constexpr std::uint8_t zoneModifiers = 0x04;

bool isKeyboardUsage(const KeyDefinition& key) noexcept
{
    return key.zone == zoneKeys || key.zone == zoneModifiers;
}

} // namespace

shared_kernel::Result<GameModeKeys> GameModeKeys::create(std::span<const KeyId> keys)
{
    std::vector<std::uint8_t> codes;
    for (const auto id : keys) {
        const auto& definition = G915KeyCatalog::definition(id);
        if (!isKeyboardUsage(definition)) {
            return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                       "tecla sem código HID de teclado: " + std::string{definition.name});
        }
        if (std::ranges::find(codes, definition.index) == codes.end()) {
            codes.push_back(definition.index);
        }
    }
    if (codes.size() > kMaxKeys) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "o modo de jogo aceita no máximo " + std::to_string(kMaxKeys) + " teclas");
    }
    return GameModeKeys{std::move(codes)};
}

GameModeKeys GameModeKeys::suggested()
{
    std::vector<std::uint8_t> codes;
    for (const char* name : {"Key: Left Windows", "Key: Right Windows", "Key: Menu"}) {
        if (const auto id = G915KeyCatalog::findByName(name); id.has_value()) {
            codes.push_back(G915KeyCatalog::definition(*id).index);
        }
    }
    return GameModeKeys{std::move(codes)};
}

std::span<const KeyId> GameModeKeys::eligibleKeys()
{
    static const std::vector<KeyId> eligible = [] {
        std::vector<KeyId> result;
        for (const auto& key : G915KeyCatalog::keys()) {
            if (isKeyboardUsage(key)) {
                result.push_back(key.id);
            }
        }
        return result;
    }();
    return eligible;
}

bool GameModeKeys::isEligibleCode(std::uint8_t hidCode) noexcept
{
    const auto all = G915KeyCatalog::keys();
    return std::ranges::any_of(
        all, [hidCode](const KeyDefinition& key) { return isKeyboardUsage(key) && key.index == hidCode; });
}

shared_kernel::Result<OnboardSlot> OnboardSlot::create(std::uint8_t number)
{
    if (number < kMin || number > kMax) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "slot onboard fora da faixa 1..3: " + std::to_string(number));
    }
    return OnboardSlot{number};
}

} // namespace ghub::lighting::domain
