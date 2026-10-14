#include "modules/audio_processing/infrastructure/PipeWireGraphQueries.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <format>
#include <optional>
#include <string_view>

namespace ghub::audio_processing {
namespace {

constexpr std::uint32_t g733Vendor = 0x046d;
constexpr std::uint32_t g733Product = 0x0afe;

[[nodiscard]] const std::string* prop(const GraphObject& object, const std::string& key)
{
    const auto it = object.props.find(key);
    return it == object.props.end() ? nullptr : &it->second;
}

// "0x046d" ou "0x046D" -> valor; qualquer outra forma -> nullopt.
[[nodiscard]] std::optional<std::uint32_t> parseHexId(const std::string* text)
{
    if (text == nullptr || text->size() < 3 || !text->starts_with("0x")) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    const char* const first = text->data() + 2;
    const char* const last = text->data() + text->size();
    const auto result = std::from_chars(first, last, value, 16);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] bool isHeadsetDevice(const GraphObject& device)
{
    return parseHexId(prop(device, "device.vendor.id")) == g733Vendor &&
           parseHexId(prop(device, "device.product.id")) == g733Product;
}

// Leitor mínimo de objeto JSON plano: {"chave":"texto", ...}. Valores que não são texto são aceitos e
// ignorados, exceto a chave "name", que deve ser texto.
class FlatObjectReader {
public:
    explicit FlatObjectReader(std::string_view text) : text_(text) {}

    [[nodiscard]] std::optional<std::string> readName()
    {
        skipSpace();
        if (!consume('{')) {
            return std::nullopt;
        }
        std::optional<std::string> name;
        skipSpace();
        if (consume('}')) {
            return finish(name);
        }
        while (true) {
            skipSpace();
            const auto key = readString();
            if (!key) {
                return std::nullopt;
            }
            skipSpace();
            if (!consume(':')) {
                return std::nullopt;
            }
            skipSpace();
            if (peek() == '"') {
                auto value = readString();
                if (!value) {
                    return std::nullopt;
                }
                if (*key == "name") {
                    name = std::move(value);
                }
            } else {
                if (*key == "name" || !skipScalar()) {
                    return std::nullopt;
                }
            }
            skipSpace();
            if (consume(',')) {
                continue;
            }
            if (consume('}')) {
                return finish(name);
            }
            return std::nullopt;
        }
    }

private:
    [[nodiscard]] std::optional<std::string> finish(const std::optional<std::string>& name)
    {
        skipSpace();
        return pos_ == text_.size() ? name : std::nullopt;
    }
    [[nodiscard]] char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    bool consume(char c)
    {
        if (peek() == c && pos_ < text_.size()) {
            ++pos_;
            return true;
        }
        return false;
    }
    void skipSpace()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }
    bool skipScalar()
    {
        const auto start = pos_;
        while (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '}' && text_[pos_] != '{' &&
               text_[pos_] != '[' && text_[pos_] != '"') {
            ++pos_;
        }
        return pos_ > start;
    }
    [[nodiscard]] std::optional<std::string> readString()
    {
        if (!consume('"')) {
            return std::nullopt;
        }
        std::string out;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return out;
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) {
                return std::nullopt;
            }
            const char e = text_[pos_++];
            switch (e) {
            case '"':
            case '\\':
            case '/':
                out += e;
                break;
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                out += '\r';
                break;
            case 'u': {
                unsigned code = 0;
                if (pos_ + 4 > text_.size() ||
                    std::from_chars(text_.data() + pos_, text_.data() + pos_ + 4, code, 16).ptr !=
                        text_.data() + pos_ + 4 ||
                    code > 0x7f) {
                    return std::nullopt;
                }
                out += static_cast<char>(code);
                pos_ += 4;
                break;
            }
            default:
                return std::nullopt;
            }
        }
        return std::nullopt; // string não terminada
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

} // namespace

namespace {

shared_kernel::Result<std::string> findHeadsetNodeName(const GraphSnapshot& snapshot, const char* wantedClass)
{
    for (const auto& node : snapshot.nodes) {
        const auto* mediaClass = prop(node, "media.class");
        const auto* name = prop(node, "node.name");
        const auto* deviceId = prop(node, "device.id");
        if (mediaClass == nullptr || *mediaClass != wantedClass || name == nullptr || name->empty() ||
            deviceId == nullptr) {
            continue;
        }
        const bool fromHeadset = std::ranges::any_of(snapshot.devices, [&](const GraphObject& device) {
            return std::to_string(device.id) == *deviceId && isHeadsetDevice(device);
        });
        if (fromHeadset) {
            return *name;
        }
    }
    return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable, "G733 não encontrado");
}

} // namespace

shared_kernel::Result<std::string> findHeadsetSinkName(const GraphSnapshot& snapshot)
{
    return findHeadsetNodeName(snapshot, "Audio/Sink");
}

shared_kernel::Result<std::string> findHeadsetSourceName(const GraphSnapshot& snapshot)
{
    return findHeadsetNodeName(snapshot, "Audio/Source");
}

std::string makeDefaultNodeValue(const std::string& sinkName)
{
    std::string out = "{\"name\":\"";
    for (const char c : sinkName) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (u < 0x20) {
            out += std::format("\\u{:04x}", static_cast<unsigned>(u));
        } else {
            out += c;
        }
    }
    out += "\"}";
    return out;
}

shared_kernel::Result<std::string> parseDefaultNodeValue(const std::string& value, const std::string& metadataKey)
{
    auto name = FlatObjectReader{value}.readName();
    if (!name) {
        return shared_kernel::fail(shared_kernel::ErrorCode::MalformedData, "valor de " + metadataKey + " mal formado");
    }
    return *std::move(name);
}

std::string makeDefaultSinkValue(const std::string& sinkName)
{
    return makeDefaultNodeValue(sinkName);
}

shared_kernel::Result<std::string> parseDefaultSinkValue(const std::string& value)
{
    return parseDefaultNodeValue(value, "default.configured.audio.sink");
}

} // namespace ghub::audio_processing
