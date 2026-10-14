# Hubless

**Configurador nativo de periféricos Logitech para Linux** — C++23, Qt 6/QML — voltado a distribuições baseadas em **Arch Linux** (Arch, CachyOS, EndeavourOS, Manjaro etc.). Desenvolvido e testado no CachyOS com G915 (teclado), G703 (mouse), G733 (headset) e Brio 100 (webcam).

> **Plataforma-alvo:** Arch Linux e derivadas. Os nomes de pacotes, o empacotamento planejado (PKGBUILD) e os testes assumem esse ambiente. Outras distribuições podem funcionar compilando do código-fonte, mas não são testadas nem suportadas.

O objetivo é substituir o Logitech G HUB **nas funcionalidades tecnicamente possíveis no Linux**. Paridade total não é prometida: cada recurso só é anunciado depois de comprovado no hardware.

> Projeto independente e **não oficial**. Não tem relação com a Logitech. “Logitech”, “G HUB”, “LIGHTSPEED” e os nomes dos produtos são marcas dos seus donos e aparecem aqui só para identificar os dispositivos compatíveis.

## Estado

Em desenvolvimento, sem versão publicada. Resumo do que já funciona no dispositivo real:

| Dispositivo | O que funciona hoje |
|---|---|
| G915 | Detecção; bateria (kernel `power_supply`); iluminação ao vivo de uma cor pelo app (HID++ próprio, volátil); prévia de efeitos de hardware (onda, ciclo, respiração, ondulação) enquanto o app segura o controle. Gravar efeito no teclado **não** funciona |
| G703 | Detecção; bateria; leitura de DPI, report rate, botões e LED via ratbagd (só leitura, sob demanda) |
| G733 | Detecção; bateria, sidetone 0–100 e luzes liga/desliga via [HeadsetControl](https://github.com/Sapd/HeadsetControl) |
| Brio 100 | Detecção; modos de vídeo; pré-visualização; leitura e ajuste dos 12 controles de imagem (V4L2) |

Princípios de segurança:

- Nada é gravado em dispositivo ao abrir o app nem ao reconectar; toda escrita acontece por clique do usuário.
- Comandos HID++ desconhecidos, firmware e perfis onboard não são gravados.
- O app não roda como root.

## Build e testes

Dependências (pacotes do Arch Linux, instaláveis com `pacman`):

- Execução: `qt6-base`, `qt6-declarative`, `libpipewire`
- Build e testes: `cmake`, `ninja`, `gcc`, `gtest`, `python`
- Opcionais: `headsetcontrol` (G733), `libratbag` (G703, ratbagd ativado sob demanda via D-Bus), `pipewire` (equalizador e redução de ruído), `noise-suppression-for-voice` (redução de ruído do microfone)

```sh
cmake --preset dev            # Debug (GCC); presets: dev, asan (ASan+UBSan), tsan (sem app Qt), release
cmake --build --preset dev
ctest --preset dev            # GoogleTest + teste de arquitetura + smoke do app (offscreen)
./build/dev/src/app/ghub-linux
```

## Instalação local (forma atual)

Instala só para o seu usuário, em `~/.local`, sem `sudo`:

```sh
cmake -S . -B build/local -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$HOME/.local -DGHUB_INSTALL_UDEV_RULES=OFF -DGHUB_BUILD_TESTS=OFF
cmake --build build/local
cmake --install build/local
```

Isso instala:

- o binário em `~/.local/bin/ghub-linux` (o `~/.local/bin` precisa estar no `PATH`, por causa do autostart);
- a entrada de menu “Hubless”;
- o ícone.

Para atualizar, rode os mesmos comandos de novo. Para remover: `xargs rm < build/local/install_manifest.txt`.

### Regra udev

O app abre a interface HID++ (interface 2) dos receptores LIGHTSPEED `046d:c547` (G915) e `046d:c539` (G703). Se outro pacote já dá esse acesso (por exemplo, o `openrgb`), nada precisa ser feito.

Caso contrário, copie a regra do projeto uma vez:

```sh
sudo cp packaging/70-ghub-linux.rules /etc/udev/rules.d/
```

Depois, reconecte o receptor. A regra dá acesso só ao usuário da sessão ativa e só à interface 2; as interfaces de teclas e movimentos ficam de fora. O G733 usa a regra do pacote `headsetcontrol`.

### Pacote (futuro)

`packaging/arch/PKGBUILD` (`hubless-git`) já gera um pacote do `pacman` com os mesmos arquivos e a regra udev. Ele fica para quando houver outros usuários (AUR).

Testes em hardware real são opcionais e precisam de `GHUB_HW_TESTS=1`. Os que escrevem no dispositivo exigem variáveis adicionais.

## Arquitetura

Monólito modular com DDD e Clean Architecture: cada módulo (`battery`, `camera`, `lighting`, `pointer`, `headset`, `device_inventory`, `device_access`) separa `domain`, `application`, `infrastructure` e `presentation`, e as dependências sempre apontam para dentro. O desenvolvimento é guiado por testes (TDD).

## Créditos

O protocolo de iluminação do G915 deriva do [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) (GPL-2.0-or-later, tag `release_1.0`): os arquivos `G915KeyCatalog.cpp`, `G915PerKeyFrames.cpp` e `G915EffectFrames.cpp`, em `src/modules/lighting/`, mantêm o cabeçalho original de autoria e licença e indicam a origem e o que foi alterado. Também usa ou consulta [HeadsetControl](https://github.com/Sapd/HeadsetControl), [libratbag](https://github.com/libratbag/libratbag) e [Solaar](https://github.com/pwr-Solaar/Solaar) (só como referência).

## Licença

[GPL-3.0-or-later](LICENSE).
