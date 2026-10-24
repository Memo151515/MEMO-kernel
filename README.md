# MNG/MEMO — MEMO Kernel 1.2.0

**MEMO**, sıfırdan yazılmış, Linux benzeri bir kernel'dir; üzerine kurulacak userland **MNG** ("MNG is Not GNU") ile birlikte sistemin adı **MNG/MEMO** olur (GNU/Linux gibi).
**MEMO** is a from-scratch, Linux-like kernel. Together with the **MNG** userland ("MNG is Not GNU") the system is called **MNG/MEMO**.

## Durum / Status

| Alan / Area | Durum / State |
|---|---|
| x86_64 (higher-half, long mode) ve i386 | çalışıyor / working (`make ARCH=...`) |
| MBoot v1.1 (BIOS), VBE framebuffer, E820 | çalışıyor / working |
| Süreç başına bellek alanı, ELF yükleyici / per-process address spaces, ELF loader | **x86_64**, statik ELF (static / static-PIE) |
| Linux x86_64 syscall uyumluluğu / Linux syscall ABI | temel set: glibc'li statik programlar çalışıyor / basic set, static glibc programs run |
| ext2 / ext3 / **ext4** | **salt okunur / read-only** (extents, 64-bit, htree); yazma + journal yok / no write, no journal |
| Ağ / Networking | **Intel e1000** + ARP, IPv4, ICMP (ping), UDP, DHCP (x86_64) |
| Sürücüler / Drivers | PIT, PIC, PS/2, UART, RTC, PCI, ATA PIO, e1000, VBE/VGA konsol |
| Klavye / Keyboard | us uk de fr es it tr dvorak colemak (`loadkeys`) |
| Shell | ~60 komut / commands |

### Henüz yok / Not yet (yol haritası / roadmap)
- **TCP, DNS**, soketler; **WiFi** (802.11 + firmware + WPA2), RTL8139/RTL8169/virtio-net sürücüleri
- **Dinamik linkleme** (ld.so / `PT_INTERP`), `fork`/`execve`/`wait4`/`clone`/threads, pipe, signals → busybox, Wine, glibc'li çoğu program
- **Çekirdek modülü uyumluluk katmanı (LinuxKPI benzeri)**: Linux kernel API'sini taklit eden bir katman — NVIDIA'nın kapalı `nvidia.ko` sürücüsü için gerekli, çok büyük bir iş
- **ext4 yazma**, VFS katmanı, `/proc` `/sys` `/dev`
- **Gerçek PC**: şu an yalnızca eski BIOS/CSM boot (MBoot). UEFI önyükleme (GOP framebuffer) + ACPI + AHCI/NVMe + USB (klavye/fare) gerekli. QEMU'da test edildi, **gerçek donanımda denenmedi**.
- **Wayland/X11 ve masaüstü (Hyprland, XFCE, KDE Plasma)**: DRM/KMS, evdev, Mesa + GPU sürücüsü, unix soketleri, mmap/poll/epoll/eventfd, shm, D-Bus gerekir

## Derleme / Building
```sh
make defconfig            # veya: make menuconfig
make                      # i386
make ARCH=x86_64          # 64-bit (süreçler, ağ ve Linux uyumluluk burada)
make CROSS=               # cross-compiler yerine sistem gcc'si
make rootfs               # build/rootfs.ext4: ext4 imaj + örnek glibc programı
make ARCH=x86_64 image run   # ../MEMO-mboot ile disk imajı üret, QEMU'da başlat (ağ + 2. disk dahil)
```
Gerekenler / needs: `gcc` (ya da `x86_64-elf-gcc`), `binutils`, `nasm`, `qemu`, `e2fsprogs` (rootfs için), `libc6-dev` (statik glibc).

Örnek oturum / example session:
```
root@memo:/# ls /disk/bin
root@memo:/# exec /disk/bin/hello a b      # glibc ile derlenmiş Linux programı
root@memo:/# ping 10.0.2.2                 # QEMU kullanıcı ağı
root@memo:/# loadkeys tr
```

## Klasörler / Layout
```
arch/x86 arch/i386 arch/x86_64   mimari kodu / architecture code
kernel/        main, irq, scheduler, mm/pmm, syscall, proc/ (vm, elf, Linux syscall'ları), net/ (IPv4 yığını)
mm/ lib/ fs/   heap, string/printf, ramfs, ext2/3/4 (salt okunur)
drivers/       console, serial, keyboard, pic, pit, rtc, pci, block/ata, net/e1000
shell/         shell + cmd_*.c
include/memo/  başlıklar / headers (version.h: MNG/MEMO 1.2.0)
tools/ configs/ scripts/
```
Yeni sürücü: `device_initcall()` / `late_initcall()` + `driver_register()`. Yeni komut: `SHELL_CMD(...)`.

## Lisans / License
MEMO Free Code License (MFCL) — bkz. `LICENSE`.
