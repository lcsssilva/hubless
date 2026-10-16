#pragma once

#include "modules/lighting/domain/RgbColor.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ghub::lighting::application {

// INC-021 R5 (ADR-0030): "Sincronizar cor". Aplica a mesma cor fixa ao G915 (ao vivo, volátil), às duas zonas do
// G733 (volátil) e aos LEDs do G703 (gravados no perfil onboard ativo: persistente).

enum class SyncDevice : std::uint8_t { Keyboard, Headset, Mouse };
enum class SyncStatus : std::uint8_t { Applied, Skipped, Failed };

struct SyncOutcome {
    SyncDevice device;
    SyncStatus status;
    std::string detail; // motivo do pulo ou da falha; vazio quando aplicado
};

struct SyncRequest {
    domain::RgbColor color;
    bool keyboard;
    bool headset;
    bool mouse;
    // O G703 grava no mouse (persistente): sem esta confirmação explícita ele é pulado com aviso.
    bool confirmPersistentMouse;
};

// Ports (implementados sobre as api/ dos módulos donos; ver SyncColorTargets.hpp).
class KeyboardColorTarget {
public:
    virtual ~KeyboardColorTarget() = default;
    // false = o modo ao vivo do G915 não está ligado: a cor não é enviada e o teclado é pulado.
    [[nodiscard]] virtual bool ready() const = 0;
    [[nodiscard]] virtual shared_kernel::Result<void> applyFixed(const domain::RgbColor& color) = 0;

protected:
    KeyboardColorTarget() = default;
    KeyboardColorTarget(const KeyboardColorTarget&) = default;
    KeyboardColorTarget& operator=(const KeyboardColorTarget&) = default;
    KeyboardColorTarget(KeyboardColorTarget&&) = default;
    KeyboardColorTarget& operator=(KeyboardColorTarget&&) = default;
};

class HeadsetColorTarget {
public:
    virtual ~HeadsetColorTarget() = default;
    [[nodiscard]] virtual shared_kernel::Result<void> applyFixed(const domain::RgbColor& color) = 0;

protected:
    HeadsetColorTarget() = default;
    HeadsetColorTarget(const HeadsetColorTarget&) = default;
    HeadsetColorTarget& operator=(const HeadsetColorTarget&) = default;
    HeadsetColorTarget(HeadsetColorTarget&&) = default;
    HeadsetColorTarget& operator=(HeadsetColorTarget&&) = default;
};

class MouseColorTarget {
public:
    virtual ~MouseColorTarget() = default;
    [[nodiscard]] virtual shared_kernel::Result<void> applyFixed(const domain::RgbColor& color) = 0;

protected:
    MouseColorTarget() = default;
    MouseColorTarget(const MouseColorTarget&) = default;
    MouseColorTarget& operator=(const MouseColorTarget&) = default;
    MouseColorTarget(MouseColorTarget&&) = default;
    MouseColorTarget& operator=(MouseColorTarget&&) = default;
};

// Caso de uso. Bloqueante (o G733 e o G703 falam com o dispositivo): quem chama escolhe a thread. Os alvos precisam
// sobreviver ao caso de uso. Os dispositivos são tentados em ordem teclado, headset, mouse; falha ou pulo de um não
// impede os outros. Só os dispositivos selecionados aparecem no resultado. Nada é lido nem escrito na construção.
class SyncColor {
public:
    SyncColor(KeyboardColorTarget& keyboard, HeadsetColorTarget& headset, MouseColorTarget& mouse) noexcept
        : keyboard_(&keyboard), headset_(&headset), mouse_(&mouse)
    {
    }

    [[nodiscard]] std::vector<SyncOutcome> apply(const SyncRequest& request) const;

private:
    KeyboardColorTarget* keyboard_; // não nulo
    HeadsetColorTarget* headset_;   // não nulo
    MouseColorTarget* mouse_;       // não nulo
};

} // namespace ghub::lighting::application
