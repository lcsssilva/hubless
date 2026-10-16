#include "modules/pointer/domain/ButtonAssignments.hpp"

#include "modules/pointer/domain/OnboardButtons.hpp"

#include <algorithm>
#include <string>

namespace ghub::pointer {

namespace {

constexpr std::size_t kFirstButtonOffset = 0x20;
constexpr std::size_t kEntrySize = 4;
constexpr std::size_t kEndOfButtons = kFirstButtonOffset + (ButtonAssignments::kCount * kEntrySize);
constexpr std::uint32_t kModifierMask = 0x0F;
constexpr std::uint32_t kMaxMouseButton = 5;

std::vector<OnboardKey> buildCatalog()
{
    std::vector<OnboardKey> keys;
    static constexpr std::string_view letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (std::size_t i = 0; i < letters.size(); ++i) {
        keys.push_back({static_cast<std::uint8_t>(0x04 + i), letters.substr(i, 1)});
    }
    static constexpr std::string_view digits = "1234567890";
    for (std::size_t i = 0; i < digits.size(); ++i) {
        keys.push_back({static_cast<std::uint8_t>(0x1e + i), digits.substr(i, 1)});
    }
    keys.insert(keys.end(), {{0x28, "Enter"}, {0x29, "Esc"}, {0x2a, "Backspace"}, {0x2b, "Tab"}, {0x2c, "Espaço"}});
    static constexpr std::array<std::string_view, 12> functionKeys = {"F1", "F2", "F3", "F4",  "F5",  "F6",
                                                                      "F7", "F8", "F9", "F10", "F11", "F12"};
    for (std::size_t i = 0; i < functionKeys.size(); ++i) {
        keys.push_back({static_cast<std::uint8_t>(0x3a + i), functionKeys.at(i)});
    }
    keys.insert(keys.end(), {{0x49, "Insert"},
                             {0x4a, "Home"},
                             {0x4b, "Page Up"},
                             {0x4c, "Delete"},
                             {0x4d, "End"},
                             {0x4e, "Page Down"},
                             {0x4f, "Seta direita"},
                             {0x50, "Seta esquerda"},
                             {0x51, "Seta abaixo"},
                             {0x52, "Seta acima"}});
    return keys;
}

bool isPrimaryChangeSafe(const ButtonAssignment& assignment)
{
    switch (assignment.kind()) {
    case ButtonAssignment::Kind::Keep:
    case ButtonAssignment::Kind::FactoryDefault:
        return true;
    case ButtonAssignment::Kind::MouseButton:
        return assignment.entry() == ButtonAssignment::mouseButton(1)->entry();
    case ButtonAssignment::Kind::Key:
        return false;
    }
    return false;
}

} // namespace

std::span<const OnboardKey> onboardKeyCatalog() noexcept
{
    static const std::vector<OnboardKey> catalog = buildCatalog();
    return catalog;
}

shared_kernel::Result<ButtonAssignment> ButtonAssignment::mouseButton(std::uint32_t number)
{
    if (number < 1 || number > kMaxMouseButton) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "botão do mouse deve ser de 1 a 5 (recebido: " + std::to_string(number) + ")");
    }
    return ButtonAssignment{Kind::MouseButton, {0x80, 0x01, 0x00, static_cast<std::uint8_t>(1U << (number - 1U))}};
}

shared_kernel::Result<ButtonAssignment> ButtonAssignment::key(std::uint32_t modifiers, std::uint32_t keycode)
{
    if ((modifiers & ~kModifierMask) != 0) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "modificadores inválidos: " + std::to_string(modifiers));
    }
    const auto catalog = onboardKeyCatalog();
    if (!std::ranges::any_of(catalog, [keycode](const OnboardKey& key) { return key.code == keycode; })) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "tecla fora da lista permitida: " + std::to_string(keycode));
    }
    return ButtonAssignment{Kind::Key,
                            {0x80, 0x02, static_cast<std::uint8_t>(modifiers), static_cast<std::uint8_t>(keycode)}};
}

ButtonAssignments ButtonAssignments::keepAll()
{
    return ButtonAssignments{std::vector<ButtonAssignment>(kCount, ButtonAssignment::keep())};
}

shared_kernel::Result<ButtonAssignments> ButtonAssignments::make(std::vector<ButtonAssignment> assignments,
                                                                 bool confirmPrimaryChange)
{
    if (assignments.size() != kCount) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "são necessárias " + std::to_string(kCount) +
                                       " atribuições (recebido: " + std::to_string(assignments.size()) + ")");
    }
    if (!confirmPrimaryChange && !isPrimaryChangeSafe(assignments.front())) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "o botão 1 é o clique principal: atribuir outra coisa a ele exige confirmação");
    }
    return ButtonAssignments{std::move(assignments)};
}

shared_kernel::Result<std::vector<ButtonAction>> ButtonAssignments::read(std::span<const std::uint8_t> sector)
{
    auto buttons = decodeOnboardButtons(sector);
    if (buttons.size() < kCount) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "setor onboard com menos de " + std::to_string(kCount) + " botões em uso");
    }
    buttons.resize(kCount, ButtonAction::none());
    return buttons;
}

bool ButtonAssignments::needsReference() const noexcept
{
    return std::ranges::any_of(assignments_, [](const ButtonAssignment& assignment) {
        return assignment.kind() == ButtonAssignment::Kind::FactoryDefault;
    });
}

shared_kernel::Result<std::vector<std::uint8_t>>
ButtonAssignments::applyTo(std::span<const std::uint8_t> original, std::span<const std::uint8_t> reference) const
{
    if (original.size() < kEndOfButtons || reference.size() < kEndOfButtons) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData,
                                   "setor onboard curto demais para os botões");
    }
    std::vector<std::uint8_t> raw{original.begin(), original.end()};
    for (std::size_t i = 0; i < kCount; ++i) {
        const ButtonAssignment& assignment = assignments_.at(i);
        const auto at = static_cast<std::ptrdiff_t>(kFirstButtonOffset + (i * kEntrySize));
        switch (assignment.kind()) {
        case ButtonAssignment::Kind::Keep:
            break;
        case ButtonAssignment::Kind::FactoryDefault:
            std::copy_n(reference.begin() + at, kEntrySize, raw.begin() + at);
            break;
        case ButtonAssignment::Kind::MouseButton:
        case ButtonAssignment::Kind::Key: {
            const auto entry = assignment.entry();
            std::ranges::copy(entry, raw.begin() + at);
            break;
        }
        }
    }
    return raw;
}

} // namespace ghub::pointer
