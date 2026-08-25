# MEMO

**MEMO**, sıfırdan yazılmış, x86 mimarisi için Linux benzeri bir kernel'dir. Herhangi bir mevcut kernel kod tabanından (Linux, BSD vb.) kod alınmadan, GDT/IDT kurulumundan userspace'e kadar tüm katmanlar sıfırdan geliştirilmiştir.

A **Linux-like kernel for x86, written entirely from scratch**. No code was taken from any existing kernel codebase (Linux, BSD, etc.) — every layer, from GDT/IDT setup to userspace, was built from the ground up.

## Özellikler / Features

- Kendi GDT (Global Descriptor Table) ve IDT (Interrupt Descriptor Table) kurulumu
  Custom GDT (Global Descriptor Table) and IDT (Interrupt Descriptor Table) setup
- CPU exception handling (divide-by-zero, page fault, general protection fault vb.)
  CPU exception handling (divide-by-zero, page fault, general protection fault, etc.)
- IRQ/PIC tabanlı donanım kesme yönetimi
  IRQ/PIC-based hardware interrupt management
- PS/2 klavye sürücüsü
  PS/2 keyboard driver
- Basit bir kernel-mode shell (`help`, `clear`, `about`, `uptime`, `ls`, `write`, `cat`)
  A simple kernel-mode shell (`help`, `clear`, `about`, `uptime`, `ls`, `write`, `cat`)
- Paging (sanal bellek yönetimi, identity-mapped ilk 4MB)
  Paging (virtual memory management, first 4MB identity-mapped)
- Kernel heap (`kmalloc`)
  Kernel heap (`kmalloc`)
- PIT (Programmable Interval Timer) tabanlı preemptive multitasking
  PIT (Programmable Interval Timer)-based preemptive multitasking
- Syscall arayüzü (`int 0x80`)
  Syscall interface (`int 0x80`)
- Basit bellek içi (in-memory) dosya sistemi
  Simple in-memory file system
- TSS (Task State Segment) ve userspace (ring 3) desteği
  TSS (Task State Segment) and userspace (ring 3) support

## Gereksinimler / Requirements

- `x86_64-elf-gcc` ve `x86_64-elf-binutils` (cross-compiler)
- `nasm`
- `qemu` (test için / for testing)

## Derleme / Build

```bash
make clean
make
```

Bu, `memo.bin` (ELF) ve `memo_flat.bin` (flat binary, bootloader için) dosyalarını üretir.

This produces `memo.bin` (ELF) and `memo_flat.bin` (flat binary, for the bootloader).

## Çalıştırma / Running

MEMO, kendi bootloader'ı olan [MBoot](https://github.com/Memo151515/MEMO-mboot) ile birlikte çalıştırılacak şekilde tasarlanmıştır (GRUB veya başka bir GPL'li bootloader kullanılmaz). Test etmek için MBoot reposundaki talimatları izleyin.

MEMO is designed to run with its own bootloader, [MBoot](https://github.com/Memo151515/MEMO-mboot) (no GRUB or other GPL-licensed bootloader is used). To test it, follow the instructions in the MBoot repo.

## Lisans / License

Bu proje **MEMO Free Code License (MFCL)** ile lisanslanmıştır — kodu istediğiniz gibi değiştirebilir, kapalı kaynak türevler oluşturabilirsiniz; tek şart orijinal yazara atıf yapmaktır. Detaylar için `LICENSE` dosyasına bakın.

This project is licensed under the **MEMO Free Code License (MFCL)** — you may modify the code freely and create closed-source derivatives; the only requirement is crediting the original author. See the `LICENSE` file for details.
