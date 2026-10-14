#include "modules/device_access/domain/AccessRegistry.hpp"

#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace ghub::device_access {

using shared_kernel::ErrorCode;
using shared_kernel::fail;
using shared_kernel::Instant;
using shared_kernel::Result;

namespace detail {

// Estado compartilhado entre o registry e seus leases. Toda operação é atômica sob mutex_.
// Leases encerrados (expirados ou revogados) ficam em closed_ até o DeviceLease ser destruído,
// para que verify() informe o motivo correto.
class LeaseTable {
public:
    explicit LeaseTable(const shared_kernel::Clock& clock) : clock_(clock) {}

    Result<std::uint64_t> grant(const LeaseRequest& request)
    {
        const std::scoped_lock lock{mutex_};
        const Instant now = clock_.now();
        if (const auto it = active_.find(request.interfaceId); it != active_.end()) {
            if (now < it->second.deadline) {
                return fail(ErrorCode::DeviceBusy,
                            request.interfaceId.value() + " em uso por " + it->second.holder.name());
            }
            close(it, ErrorCode::LeaseExpired);
        }
        const std::uint64_t token = ++lastToken_;
        active_.emplace(request.interfaceId,
                        Entry{.token = token, .holder = request.holder, .deadline = now + request.maxDuration});
        return token;
    }

    Result<void> verify(const PhysicalInterfaceId& interfaceId, std::uint64_t token)
    {
        const std::scoped_lock lock{mutex_};
        if (const auto it = active_.find(interfaceId); it != active_.end() && it->second.token == token) {
            if (clock_.now() < it->second.deadline) {
                return {};
            }
            close(it, ErrorCode::LeaseExpired);
        }
        if (const auto closed = closed_.find(token); closed != closed_.end()) {
            return fail(closed->second, interfaceId.value());
        }
        return fail(ErrorCode::LeaseRevoked, interfaceId.value());
    }

    void release(const PhysicalInterfaceId& interfaceId, std::uint64_t token) noexcept
    {
        const std::scoped_lock lock{mutex_};
        if (const auto it = active_.find(interfaceId); it != active_.end() && it->second.token == token) {
            active_.erase(it);
        }
        closed_.erase(token);
    }

    void revoke(const PhysicalInterfaceId& interfaceId)
    {
        const std::scoped_lock lock{mutex_};
        if (const auto it = active_.find(interfaceId); it != active_.end()) {
            close(it, ErrorCode::LeaseRevoked);
        }
    }

    void revokeAll()
    {
        const std::scoped_lock lock{mutex_};
        while (!active_.empty()) {
            close(active_.begin(), ErrorCode::LeaseRevoked);
        }
    }

private:
    struct Entry {
        std::uint64_t token;
        LeaseHolder holder;
        Instant deadline;
    };
    using ActiveMap = std::map<PhysicalInterfaceId, Entry>;

    void close(ActiveMap::iterator it, ErrorCode reason)
    {
        closed_.insert_or_assign(it->second.token, reason);
        active_.erase(it);
    }

    const shared_kernel::Clock& clock_;
    std::mutex mutex_;
    ActiveMap active_;
    std::unordered_map<std::uint64_t, ErrorCode> closed_;
    std::uint64_t lastToken_ = 0;
};

} // namespace detail

DeviceLease::DeviceLease(std::weak_ptr<detail::LeaseTable> table, PhysicalInterfaceId interfaceId, std::uint64_t token)
    : table_(std::move(table)), interfaceId_(std::move(interfaceId)), token_(token)
{
}

DeviceLease::DeviceLease(DeviceLease&& other) noexcept
    : table_(std::exchange(other.table_, {})),
      interfaceId_(other.interfaceId_), // NOLINT(performance-move-constructor-init): cópia preserva o invariante
      token_(other.token_)              // "PhysicalInterfaceId nunca vazio" no objeto movido
{
}

DeviceLease& DeviceLease::operator=(DeviceLease&& other) noexcept
{
    if (this != &other) {
        release();
        table_ = std::exchange(other.table_, {});
        interfaceId_ = other.interfaceId_;
        token_ = other.token_;
    }
    return *this;
}

DeviceLease::~DeviceLease()
{
    release();
}

void DeviceLease::release() noexcept
{
    if (const auto table = table_.lock()) {
        table->release(interfaceId_, token_);
    }
    table_.reset();
}

Result<void> DeviceLease::verify() const
{
    const auto table = table_.lock();
    if (!table) {
        return fail(ErrorCode::LeaseRevoked, interfaceId_.value());
    }
    return table->verify(interfaceId_, token_);
}

AccessRegistry::AccessRegistry(const shared_kernel::Clock& clock) : table_(std::make_shared<detail::LeaseTable>(clock))
{
}

AccessRegistry::~AccessRegistry() = default;

Result<DeviceLease> AccessRegistry::acquire(LeaseRequest request)
{
    if (request.maxDuration <= shared_kernel::Duration::zero()) {
        return fail(ErrorCode::InvalidArgument, "maxDuration deve ser positivo");
    }
    if (request.purpose.empty()) {
        return fail(ErrorCode::InvalidArgument, "purpose vazio");
    }
    return table_->grant(request).transform(
        [&](std::uint64_t token) { return DeviceLease{table_, std::move(request.interfaceId), token}; });
}

void AccessRegistry::deviceRemoved(const PhysicalInterfaceId& interfaceId)
{
    table_->revoke(interfaceId);
}

void AccessRegistry::systemSuspending()
{
    table_->revokeAll();
}

} // namespace ghub::device_access
