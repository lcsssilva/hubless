#include "modules/headset/infrastructure/HeadsetControlActionJson.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <string>

namespace ghub::headset {

namespace {

using shared_kernel::ErrorCode;
using shared_kernel::fail;

constexpr qsizetype maxMessageChars = 200;

[[nodiscard]] bool hasSupportedApiMajor(const QJsonObject& root)
{
    const auto version = root.value("api_version");
    return version.isString() && version.toString().startsWith(QLatin1String("1."));
}

} // namespace

shared_kernel::Result<void> parseHeadsetControlAction(const QByteArray& json, std::string_view capability)
{
    const auto document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        return fail(ErrorCode::MalformedData, "saída não é um objeto JSON");
    }
    const auto root = document.object();
    if (!hasSupportedApiMajor(root)) {
        return fail(ErrorCode::MalformedData, "api_version ausente ou incompatível");
    }
    const auto count = root.value("device_count");
    if (!count.isDouble()) {
        return fail(ErrorCode::MalformedData, "device_count ausente");
    }
    if (count.toInt(-1) < 1) {
        return fail(ErrorCode::BackendUnavailable, "headset não encontrado");
    }
    // As ações ficam na raiz (cli/output/output_data.hpp:289-310 do HeadsetControl 4.1.0).
    if (!root.value("actions").isArray()) {
        return fail(ErrorCode::MalformedData, "campo actions ausente");
    }
    const auto wanted = QString::fromUtf8(capability.data(), static_cast<qsizetype>(capability.size()));
    for (const auto& entry : root.value("actions").toArray()) {
        const auto action = entry.toObject();
        if (action.value("capability").toString() != wanted) {
            continue;
        }
        const auto status = action.value("status").toString();
        if (status == QLatin1String("success")) {
            return {};
        }
        if (status == QLatin1String("failure")) {
            auto message = action.value("error_message").toString();
            if (message.isEmpty()) {
                message = QStringLiteral("ação recusada pelo headsetcontrol");
            }
            return fail(ErrorCode::BackendUnavailable, message.left(maxMessageChars).toStdString());
        }
        return fail(ErrorCode::MalformedData, "status da ação desconhecido");
    }
    return fail(ErrorCode::MalformedData, "ação ausente na saída");
}

} // namespace ghub::headset
