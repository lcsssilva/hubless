#pragma once

#include <cstdint>
#include <string>

namespace ghub::lighting::infrastructure {

enum class HidppErrorKind : std::uint8_t {
    PolicyDenied,       // par (feature, função) fora da lista fechada; nada foi escrito
    FeatureNotPresent,  // o dispositivo não expõe a feature; nada foi escrito além da consulta Root
    InvalidArgument,    // parâmetros acima de 16 bytes
    Timeout,            // prazo total esgotado sem a resposta casada
    Io,                 // falha de escrita/leitura no hidraw
    DeviceErrorHidpp20, // resposta de erro HID++ 2.0 (0xFF); `code` = código do dispositivo
    DeviceErrorHidpp10, // resposta de erro HID++ 1.0 (0x8F); `code` = código do dispositivo
};

struct HidppError {
    HidppErrorKind kind;
    std::uint8_t code = 0;
    std::string detail;

    friend bool operator==(const HidppError&, const HidppError&) = default;
};

} // namespace ghub::lighting::infrastructure
