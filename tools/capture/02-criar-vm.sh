#!/usr/bin/env bash
# Cria a VM Windows 11 da captura (PLANO-REV-GHUB §5, passo A2), sem dispositivo USB repassado.
# Uso: sudo bash tools/capture/02-criar-vm.sh /caminho/Win11.iso
# Disco SATA e placa e1000e: o Windows já tem driver para os dois (sem virtio-win).
# Firmware: o edk2-ovmf do Arch (202608) só tem OVMF com Secure Boot SEM chaves gravadas
# (descritor 50-edk2-ovmf-x86_64-secure-4m.json não tem "enrolled-keys"); pedir chaves falha.
set -euo pipefail

readonly VM=ghub-captura
iso=${1:-}

[[ $EUID -eq 0 ]] || { echo "Rode com sudo." >&2; exit 1; }
[[ -f $iso ]] || { echo "Uso: sudo bash $0 /caminho/Win11.iso" >&2; exit 1; }
if virsh -c qemu:///system dominfo "$VM" >/dev/null 2>&1; then
    echo "A VM $VM já existe; nada foi feito." >&2
    exit 1
fi

# O libvirt (usuário qemu) precisa ler a ISO; copiá-la evita abrir permissão na pasta pessoal.
dest=/var/lib/libvirt/images/$(basename "$iso")
[[ -f $dest ]] || install -m 0644 "$iso" "$dest"

virt-install \
    --connect qemu:///system \
    --name "$VM" \
    --osinfo win11 \
    --memory 8192 \
    --vcpus 4 \
    --cpu host-passthrough \
    --boot firmware=efi,firmware.feature0.name=secure-boot,firmware.feature0.enabled=yes,firmware.feature1.name=enrolled-keys,firmware.feature1.enabled=no \
    --tpm model=tpm-crb,backend.type=emulator,backend.version=2.0 \
    --disk path=/var/lib/libvirt/images/$VM.qcow2,size=64,format=qcow2,bus=sata \
    --cdrom "$dest" \
    --network network=default,model=e1000e \
    --controller type=usb,model=qemu-xhci \
    --graphics spice \
    --video qxl \
    --sound none \
    --noautoconsole

echo
echo "VM criada. Abra o virt-manager para seguir a instalação do Windows."
