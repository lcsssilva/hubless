#include "modules/device_inventory/presentation/DeviceIdentityViewModel.hpp"

#include "modules/device_inventory/domain/DeviceType.hpp"

#include <QStringList>
#include <QtConcurrent>

#include <array>
#include <utility>

namespace ghub::device_inventory {

namespace {

QString notInformed()
{
    return QStringLiteral("não informado");
}

QVariantMap emptyView(const QString& state)
{
    return {
        {QStringLiteral("state"), state},
        {QStringLiteral("name"), QString{}},
        {QStringLiteral("type"), QString{}},
        {QStringLiteral("firmwares"), QStringList{}},
        {QStringLiteral("featureCount"), 0},
        {QStringLiteral("featureIds"), QStringList{}},
        {QStringLiteral("truncationNote"), QString{}},
        {QStringLiteral("error"), QString{}},
    };
}

QString truncationNote(const DeviceIdentity& identity)
{
    QStringList notes;
    if (identity.firmwareTruncated()) {
        notes << QStringLiteral("o dispositivo tem mais entidades de firmware do que as %1 lidas")
                     .arg(DeviceIdentity::maxFirmwareEntries);
    }
    if (identity.featuresTruncated()) {
        notes << QStringLiteral("o dispositivo tem mais features do que as %1 lidas").arg(DeviceIdentity::maxFeatures);
    }
    return notes.join(QStringLiteral("; "));
}

QVariantMap okView(const DeviceIdentity& identity)
{
    QStringList firmwares;
    for (const auto& firmware : identity.firmwares()) {
        firmwares << QString::fromStdString(firmware.describe());
    }
    QStringList featureIds;
    for (const auto id : identity.featureIds()) {
        featureIds << QStringLiteral("0x%1").arg(id, 4, 16, QLatin1Char('0'));
    }
    auto view = emptyView(QStringLiteral("ok"));
    view.insert(QStringLiteral("name"), identity.name() ? QString::fromStdString(*identity.name()) : notInformed());
    view.insert(QStringLiteral("type"),
                identity.deviceType() ? QString::fromStdString(deviceTypeText(*identity.deviceType())) : notInformed());
    view.insert(QStringLiteral("firmwares"), firmwares);
    view.insert(QStringLiteral("featureCount"), static_cast<int>(featureIds.size()));
    view.insert(QStringLiteral("featureIds"), featureIds);
    view.insert(QStringLiteral("truncationNote"), truncationNote(identity));
    return view;
}

QVariantMap errorView(const shared_kernel::DomainError& error)
{
    auto view = emptyView(QStringLiteral("error"));
    const auto detail = QString::fromStdString(error.detail);
    view.insert(QStringLiteral("error"),
                error.code == shared_kernel::ErrorCode::DeviceBusy ? QStringLiteral("ocupado: ") + detail : detail);
    return view;
}

} // namespace

DeviceIdentityViewModel::DeviceIdentityViewModel(const ReadDeviceIdentity& useCase, QObject* parent)
    : QObject(parent), useCase_(&useCase)
{
    using Device = std::pair<const char*, IdentityTarget>;
    const std::array<Device, 3> devices{
        Device{"g915", IdentityTarget::G915},
        Device{"g703", IdentityTarget::G703},
        Device{"g733", IdentityTarget::G733},
    };
    for (const auto& [key, target] : devices) {
        auto slot = std::make_unique<Slot>();
        slot->key = QString::fromLatin1(key);
        slot->target = target;
        slot->view = emptyView(QStringLiteral("idle"));
        Slot& ref = *slot;
        connect(&slot->watcher, &QFutureWatcher<Outcome>::finished, this, [this, &ref] { onFinished(ref); });
        slots_.push_back(std::move(slot));
    }
}

DeviceIdentityViewModel::~DeviceIdentityViewModel()
{
    // A leitura não é cancelável e usa o caso de uso: espera terminar antes de destruir qualquer coisa.
    for (const auto& slot : slots_) {
        slot->watcher.disconnect(this);
        slot->watcher.waitForFinished();
    }
}

QVariantMap DeviceIdentityViewModel::identities() const
{
    QVariantMap all;
    for (const auto& slot : slots_) {
        all.insert(slot->key, slot->view);
    }
    return all;
}

void DeviceIdentityViewModel::read(const QString& key)
{
    for (const auto& slot : slots_) {
        if (slot->key != key) {
            continue;
        }
        if (slot->view.value(QStringLiteral("state")).toString() == QLatin1String("reading")) {
            return;
        }
        slot->view = emptyView(QStringLiteral("reading"));
        const ReadDeviceIdentity& useCase = *useCase_;
        const IdentityTarget target = slot->target;
        slot->watcher.setFuture(QtConcurrent::run([&useCase, target] { return Outcome{useCase.execute(target)}; }));
        emit identitiesChanged();
        return;
    }
}

void DeviceIdentityViewModel::onFinished(Slot& slot)
{
    const Outcome outcome = slot.watcher.result();
    slot.view = outcome.result.has_value() ? okView(*outcome.result) : errorView(outcome.result.error());
    emit identitiesChanged();
    emit readFinished(slot.key);
}

} // namespace ghub::device_inventory
