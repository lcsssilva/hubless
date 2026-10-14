#pragma once

#include "modules/camera/domain/CameraControl.hpp"
#include "modules/camera/infrastructure/V4l2Api.hpp"
#include "shared_kernel/Result.hpp"

#include <linux/videodev2.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ghub::camera::v4l2 {

// Leitura de um controle V4L2 já consultado (QUERY_EXT_CTRL), compartilhada pelo backend de leitura e pelo
// acesso de escrita. Só ioctls de consulta: QUERYMENU e G_CTRL.

// Texto de `-errno`.
std::string errnoText(int negativeErrno);

// BackendUnavailable com "<o que>: <texto do errno>".
shared_kernel::DomainError unavailable(std::string_view what, int negativeErrno);

// MalformedData: o driver excedeu um limite de itens.
shared_kernel::DomainError limitExceeded(std::string_view what);

// Texto de um buffer fixo do kernel (pode não ter NUL final).
std::string fixedText(std::span<const std::uint8_t> buffer);

// Texto de um campo `char[N]` do kernel (pode não ter NUL final).
std::string fixedText(std::span<const char> buffer);

// Um passo de enumeração: true = item preenchido; false = fim da lista (EINVAL); erro = qualquer outro errno.
shared_kernel::Result<bool> enumerate(V4l2Api& api, int fd, unsigned long request, std::string_view name,
                                      void* argument);

// Tipo de controle que o produto entende; vazio para os demais.
std::optional<ControlKind> supportedKind(std::uint32_t type);

// Monta o controle: faixa, flags, itens de menu e valor atual (ausente se o driver não deixa ler; ENODEV é erro).
shared_kernel::Result<CameraControl> buildControl(V4l2Api& api, int fd, const v4l2_query_ext_ctrl& control,
                                                  ControlKind kind);

} // namespace ghub::camera::v4l2
