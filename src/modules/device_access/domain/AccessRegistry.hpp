#pragma once

#include "modules/device_access/domain/LeaseRequest.hpp"
#include "modules/device_access/domain/PhysicalInterfaceId.hpp"
#include "shared_kernel/Clock.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <memory>

namespace ghub::device_access {

namespace detail {
class LeaseTable;
}

// Direito exclusivo, temporário e revogável de escrever numa interface física.
// Liberado ao ser destruído (RAII). Quem o segura chama verify() antes de cada escrita:
// expiração e revogação (remoção, suspensão) são observadas por ali.
class DeviceLease {
public:
    DeviceLease(DeviceLease&& other) noexcept;
    DeviceLease& operator=(DeviceLease&& other) noexcept;
    DeviceLease(const DeviceLease&) = delete;
    DeviceLease& operator=(const DeviceLease&) = delete;
    ~DeviceLease();

    [[nodiscard]] const PhysicalInterfaceId& interfaceId() const noexcept { return interfaceId_; }

    // Sucesso somente se o lease ainda é o dono ativo da interface e não expirou.
    [[nodiscard]] shared_kernel::Result<void> verify() const;

private:
    friend class AccessRegistry;
    DeviceLease(std::weak_ptr<detail::LeaseTable> table, PhysicalInterfaceId interfaceId, std::uint64_t token);

    void release() noexcept;

    std::weak_ptr<detail::LeaseTable> table_;
    PhysicalInterfaceId interfaceId_;
    std::uint64_t token_;
};

// Concede no máximo um DeviceLease por PhysicalInterfaceId (ADR-0003). Seguro para chamadas
// concorrentes. Pedido conflitante falha imediatamente com DeviceBusy (sem espera).
// O Clock informado precisa sobreviver ao registry.
class AccessRegistry {
public:
    explicit AccessRegistry(const shared_kernel::Clock& clock);
    AccessRegistry(const AccessRegistry&) = delete;
    AccessRegistry& operator=(const AccessRegistry&) = delete;
    AccessRegistry(AccessRegistry&&) = delete;
    AccessRegistry& operator=(AccessRegistry&&) = delete;
    ~AccessRegistry();

    [[nodiscard]] shared_kernel::Result<DeviceLease> acquire(LeaseRequest request);

    // Revoga o lease ativo da interface (dispositivo removido).
    void deviceRemoved(const PhysicalInterfaceId& interfaceId);

    // Revoga todos os leases (sistema entrando em suspensão).
    void systemSuspending();

private:
    // shared_ptr justificado: leases podem sobreviver ao registry (ex.: no encerramento)
    // e observam a tabela por weak_ptr; o registry é o único dono forte.
    std::shared_ptr<detail::LeaseTable> table_;
};

} // namespace ghub::device_access
