#pragma once

#include "shared_kernel/Result.hpp"

#include <complex>
#include <vector>

namespace ghub::audio_processing {

// FFT radix-2 iterativa (Cooley-Tukey, decimação no tempo), própria do projeto: INC-021 R3 / ADR-0030 (sem biblioteca
// externa). Transforma `data` no lugar; convenção direta X[k] = soma x[n] e^(-2*pi*i*k*n/N), sem normalização.
// Tamanho que não seja potência de dois (inclui 0) -> InvalidArgument e `data` fica intacto.
[[nodiscard]] shared_kernel::Result<void> fftInPlace(std::vector<std::complex<double>>& data);

} // namespace ghub::audio_processing
