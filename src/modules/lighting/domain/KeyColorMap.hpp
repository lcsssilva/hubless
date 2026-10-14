#pragma once

#include "modules/lighting/domain/G915KeyCatalog.hpp"
#include "modules/lighting/domain/RgbColor.hpp"
#include "shared_kernel/Result.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace ghub::lighting::domain {

// Mapa tecla -> cor. Só aceita KeyId (que só o catálogo cria), no máximo uma cor por tecla,
// iteração determinística pela ordem do catálogo. Objeto de valor imutável.
class KeyColorMap {
public:
    using Entries = std::map<KeyId, RgbColor>;
    using const_iterator = Entries::const_iterator;

    KeyColorMap() = default;

    // Chave repetida (mesmo com a mesma cor) é erro InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<KeyColorMap>
    create(const std::vector<std::pair<KeyId, RgbColor>>& entries);

    // Cópia com a tecla definida; substitui a cor se a tecla já existir.
    [[nodiscard]] KeyColorMap with(KeyId key, RgbColor color) const;

    [[nodiscard]] std::optional<RgbColor> colorOf(KeyId key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] const_iterator begin() const noexcept { return entries_.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return entries_.end(); }

    friend bool operator==(const KeyColorMap&, const KeyColorMap&) = default;

private:
    Entries entries_;
};

} // namespace ghub::lighting::domain
