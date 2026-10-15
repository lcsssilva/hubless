#!/usr/bin/env bash
# Prepara o CachyOS para rodar o G HUB numa VM Windows (PLANO-REV-GHUB §5, passo A1).
# Rodar UMA vez, pelo usuário: sudo bash tools/capture/01-instalar-host.sh
# Não muda permissões de usbmon nem grupos do usuário (isso é por gravação: 03-gravar-usb.sh).
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "Rode com sudo." >&2; exit 1; }

pacman -S --needed qemu-desktop libvirt virt-manager virt-install edk2-ovmf swtpm wireshark-cli wireshark-qt

systemctl enable --now libvirtd.service

# Rede NAT da VM (o instalador do G HUB precisa de internet).
if ! virsh -c qemu:///system net-info default >/dev/null 2>&1; then
    virsh -c qemu:///system net-define /usr/share/libvirt/networks/default.xml
fi
virsh -c qemu:///system net-autostart default
virsh -c qemu:///system net-info default | grep -q '^Active:.*yes' || virsh -c qemu:///system net-start default

# O / é btrfs: sem copy-on-write no diretório das imagens, o qcow2 não fragmenta.
# O +C só vale para arquivos criados depois, por isso vem antes da 02-criar-vm.sh.
install -d -m 0711 /var/lib/libvirt/images
if [[ $(findmnt -no FSTYPE -T /var/lib/libvirt/images) == btrfs ]]; then
    chattr +C /var/lib/libvirt/images
fi

echo
echo "Pronto. Versões instaladas:"
pacman -Q qemu-desktop libvirt virt-manager edk2-ovmf swtpm wireshark-cli
ls -l /usr/bin/dumpcap
