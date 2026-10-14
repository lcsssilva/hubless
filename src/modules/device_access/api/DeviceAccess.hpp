#pragma once

// API pública do registro de acesso a dispositivos: o que outros módulos podem incluir para pedir um
// DeviceLease (DEVICE_ACCESS_POLICY.md). Os cabeçalhos de domínio abaixo são parte desta API.
#include "modules/device_access/domain/AccessRegistry.hpp"
#include "modules/device_access/domain/LeaseRequest.hpp"
#include "modules/device_access/domain/PhysicalInterfaceId.hpp"
