#!/usr/bin/env bash
# Grava o barramento USB de um dispositivo enquanto ele está na VM (PLANO-REV-GHUB §5, passos A4 e C-nn).
# Uso: sudo bash tools/capture/03-gravar-usb.sh <g703|g915|g733>[,<outro>...] <captura, ex.: c00> [segundos]
# Com [segundos], a gravação para sozinha (dumpcap -a duration); sem ele, só para com Ctrl+C.
# Vários dispositivos separados por vírgula gravam todos os barramentos envolvidos num só arquivo.
# Ctrl+C encerra. Nada fica permanente: o usbmon é carregado só até o próximo boot, o dumpcap roda só durante
# a gravação (sem grupo wireshark) e o arquivo final pertence ao usuário que chamou o sudo.
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "Rode com sudo." >&2; exit 1; }
[[ -n ${SUDO_USER:-} ]] || { echo "Rode com sudo a partir do seu usuário (SUDO_USER vazio)." >&2; exit 1; }

uso="Uso: sudo bash $0 <g703|g915|g733>[,<outro>...] <captura> [segundos]"
dispositivo=${1:-}
captura=${2:-}
segundos=${3:-}
[[ -n $dispositivo ]] || { echo "$uso" >&2; exit 1; }
[[ $captura =~ ^[a-z0-9-]+$ ]] || { echo "Nome da captura inválido (use letras minúsculas, números e '-')." >&2; exit 1; }
[[ -z $segundos || ( $segundos =~ ^[0-9]+$ && $segundos -ge 10 && $segundos -le 3600 ) ]] || { echo "Duração inválida (10 a 3600 segundos)." >&2; exit 1; }

interfaces=()
IFS=, read -ra nomes <<< "$dispositivo"
for nome in "${nomes[@]}"; do
    case $nome in
        g703) pid=c539 ;;   # receptor Lightspeed do G703
        g915) pid=c547 ;;   # receptor do G915
        g733) pid=0afe ;;   # G733 (receptor USB do headset)
        *) echo "$uso" >&2; exit 1 ;;
    esac
    dev=
    for d in /sys/bus/usb/devices/*; do
        [[ -f $d/idVendor && $(<"$d/idVendor") == 046d && $(<"$d/idProduct") == "$pid" ]] && { dev=$d; break; }
    done
    [[ -n $dev ]] || { echo "046d:$pid ($nome) não encontrado no host (já está na VM?). Conecte-o ao CachyOS antes de gravar." >&2; exit 1; }
    bus=$(<"$dev/busnum")
    echo "$nome: 046d:$pid no barramento $bus, endereço $(<"$dev/devnum")"
    [[ " ${interfaces[*]} " == *" usbmon$bus "* ]] || interfaces+=("usbmon$bus")
done
dispositivo=${dispositivo//,/-}

modprobe usbmon

home=$(getent passwd "$SUDO_USER" | cut -d: -f6)
dir=$home/ghub-capturas
install -d -m 0700 -o "$SUDO_USER" -g "$(id -gn "$SUDO_USER")" "$dir"
nome=$captura-$dispositivo-$(date +%Y%m%d-%H%M%S).pcapng
arquivo=$dir/$nome
# O dumpcap descarta CAP_DAC_OVERRIDE depois de abrir a interface: como root, não escreve na pasta 0700 do
# usuário. Grava numa pasta temporária do root e entrega o arquivo ao usuário no fim (inclusive no Ctrl+C).
tmp=$(mktemp -d)
entregar() {
    if [[ -s $tmp/$nome ]]; then
        install -m 0600 -o "$SUDO_USER" -g "$(id -gn "$SUDO_USER")" "$tmp/$nome" "$arquivo"
        echo "Salvo em $arquivo"
    else
        echo "Nada gravado." >&2
    fi
    rm -rf "$tmp"
}
trap entregar EXIT

echo "Gravando ${interfaces[*]}; destino $arquivo"
echo "Agora passe o(s) dispositivo(s) para a VM, um por vez. Anote cada ação no G HUB com a hora. Ctrl+C para encerrar."
args=()
for i in "${interfaces[@]}"; do args+=(-i "$i"); done
[[ -z $segundos ]] || args+=(-a "duration:$segundos")
dumpcap -q "${args[@]}" -w "$tmp/$nome"
