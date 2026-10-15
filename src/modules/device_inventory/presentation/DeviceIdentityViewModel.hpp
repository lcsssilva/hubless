#pragma once

#include "modules/device_inventory/application/ReadDeviceIdentity.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>
#include <vector>

namespace ghub::device_inventory {

// Expõe ao QML a leitura de identidade HID++ de G915, G703 e G733 (INC-015 R1/R8/R9, ADR-0024). Nada é lido na
// construção: só read(key), por clique. A leitura roda numa thread de trabalho (QtConcurrent), uma por dispositivo de
// cada vez; dispositivos diferentes podem ler ao mesmo tempo (cada um tem o seu lease).
//
// `identities` é um mapa chave -> mapa, com as chaves "g915", "g703" e "g733" sempre presentes:
//  - `state`: "idle" | "reading" | "ok" | "error";
//  - `name`, `type`: textos prontos ("não informado" quando o dispositivo não informa);
//  - `firmwares`: lista de textos ("MPK 09.04 b0042 — aplicação (ativo)"); vazia = não informado;
//  - `featureCount`, `featureIds` ("0x1001", ...);
//  - `truncationNote`: avisa quando firmware ou features foram cortados (vazio se não);
//  - `error`: mensagem do erro (vazia fora de "error").
// O caso de uso informado (e a fonte dele) precisa sobreviver ao ViewModel; o destrutor espera as leituras em
// andamento. Use sempre a partir da thread que possui o objeto.
class DeviceIdentityViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap identities READ identities NOTIFY identitiesChanged)

public:
    explicit DeviceIdentityViewModel(const ReadDeviceIdentity& useCase, QObject* parent = nullptr);
    ~DeviceIdentityViewModel() override;

    DeviceIdentityViewModel(const DeviceIdentityViewModel&) = delete;
    DeviceIdentityViewModel& operator=(const DeviceIdentityViewModel&) = delete;
    DeviceIdentityViewModel(DeviceIdentityViewModel&&) = delete;
    DeviceIdentityViewModel& operator=(DeviceIdentityViewModel&&) = delete;

    [[nodiscard]] QVariantMap identities() const;

    // Única porta de entrada: "g915", "g703" ou "g733". Chave desconhecida ou leitura em andamento é ignorada.
    Q_INVOKABLE void read(const QString& key);

signals:
    void identitiesChanged();
    // Uma leitura terminou (com sucesso ou erro); `key` é a chave do dispositivo.
    void readFinished(const QString& key);

private:
    struct Outcome {
        shared_kernel::Result<DeviceIdentity> result;
    };
    struct Slot {
        QString key;
        IdentityTarget target = IdentityTarget::G915;
        QVariantMap view;
        QFutureWatcher<Outcome> watcher;
    };

    void onFinished(Slot& slot);

    const ReadDeviceIdentity* useCase_; // não nulo; não possuído
    std::vector<std::unique_ptr<Slot>> slots_;
};

} // namespace ghub::device_inventory
