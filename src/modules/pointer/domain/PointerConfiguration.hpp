#pragma once

#include "modules/pointer/domain/PointerProfile.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Anomalias do estado que o dispositivo relata. Não impedem a criação da configuração: são o estado real.
enum class PointerWarningKind : std::uint8_t {
    NoActiveProfile,       // nenhum perfil marcado como ativo
    ActiveProfileDisabled, // perfil ativo marcado como desabilitado
    ActiveDpiNotListed,    // estágio ativo com DPI fora da lista de suportados (lista não vazia)
};

struct PointerWarning {
    PointerWarningKind kind{};
    std::optional<std::uint32_t> profileIndex;
    std::optional<std::uint32_t> stageIndex;

    friend bool operator==(const PointerWarning&, const PointerWarning&) = default;
};

// Configuração completa de um mouse lida do ratbagd. Objeto de valor imutável; só existe válida
// (nome não vazio, ao menos um perfil, índices de perfil únicos).
class PointerConfiguration {
public:
    [[nodiscard]] static shared_kernel::Result<PointerConfiguration> create(std::string name,
                                                                            std::vector<PointerProfile> profiles);

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::vector<PointerProfile>& profiles() const noexcept { return profiles_; }

    // Primeiro perfil marcado como ativo; nulo se nenhum. Vale enquanto esta configuração existir.
    [[nodiscard]] const PointerProfile* activeProfile() const noexcept;

    [[nodiscard]] std::vector<PointerWarning> warnings() const;

    friend bool operator==(const PointerConfiguration&, const PointerConfiguration&) = default;

private:
    PointerConfiguration(std::string name, std::vector<PointerProfile> profiles)
        : name_(std::move(name)), profiles_(std::move(profiles))
    {
    }

    std::string name_;
    std::vector<PointerProfile> profiles_;
};

} // namespace ghub::pointer
