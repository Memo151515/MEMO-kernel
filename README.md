# MEMO

> An operating system kernel written from scratch.

MEMO is an operating system kernel written from scratch by Memo151515
using C and x86 assembly.

MEMO is developed independently and is not based on Linux or another
existing operating system kernel.

The goal of the project is to build the fundamental components required
for a complete operating system, starting from the lowest-level parts
of the system.

---

# Features

## CPU & Architecture

- x86 protected mode
- 32-bit kernel execution
- Ring 0 kernel mode
- Ring 3 userspace support
- Assembly boot code
- Custom linker script
- Freestanding C environment
- Cross-compilation support

## Memory Management

MEMO has its own basic memory management system.

Current implementation includes:

- Kernel heap
- `kmalloc()`
- Page directory
- Page tables
- Paging initialization
- Identity mapping
- User-accessible memory pages

## Global Descriptor Table

MEMO implements its own GDT with separate privilege levels.

Current segments include:

- Kernel code
- Kernel data
- User code
- User data
- TSS entry

## Interrupts

MEMO implements its own interrupt subsystem.

It currently provides:

- IDT
- ISRs
- IRQ handlers
- Hardware interrupt registration
- Keyboard IRQ
- Timer IRQ
- System call interrupt

## Timer

MEMO uses the programmable interval timer (PIT).

The current implementation configures the timer to approximately
100 Hz and maintains a kernel tick counter.

Timer interrupts are also used by the scheduler.

## Multitasking

MEMO currently contains a basic task scheduler and context-switching
system.

Implemented functionality includes:

- Task structures
- Kernel tasks
- Task creation
- Individual task stacks
- Scheduler
- Task list
- Context switching
- Timer-driven scheduling

## Userspace

MEMO currently supports an initial Ring 3 userspace environment.

The kernel is capable of:

1. Creating a userspace stack
2. Configuring user-mode segments
3. Configuring a TSS
4. Setting up a kernel stack for privilege transitions
5. Switching from Ring 0 to Ring 3
6. Executing a userspace program

## System Calls

MEMO provides a basic system call interface through:

    INT 0x80

The currently implemented syscall is:

    SYS_WRITE = 1

## Keyboard Driver

MEMO includes a basic PS/2 keyboard driver using IRQ 1.

Current functionality includes:

- Keyboard scancode handling
- Scancode-to-ASCII conversion
- Input buffering
- Enter
- Backspace
- Keyboard input
- Screen scrolling

## VGA Text Output

MEMO currently uses VGA text mode for display output.

The VGA text buffer is located at:

    0xB8000

The kernel directly writes characters to VGA memory and currently uses
an 80x25 text-mode display.

## Shell

MEMO includes a small command-line interface running directly inside
the kernel.

Available commands:

    help
    clear
    about
    uptime
    ls
    write <name> <text>
    cat <name>

Example:

    write hello Hello from MEMO
    cat hello

## Filesystem

MEMO currently includes a basic in-memory filesystem.

It provides:

- File creation
- File writing
- File reading
- File listing
- Dynamic file contents
- Multiple files
- Filename handling

Example:

    write hello Hello from MEMO
    cat hello
    ls

The current filesystem stores files in memory and does not yet provide
persistent disk storage.

---

# Project Structure

    MEMO/
    ├── boot/
    │   ├── boot.asm
    │   ├── gdt_flush.asm
    │   ├── idt_flush.asm
    │   ├── irq.asm
    │   ├── isr.asm
    │   ├── paging_enable.asm
    │   ├── switch_to.asm
    │   └── usermode_jump.asm
    │
    ├── kernel/
    │   ├── kernel.c
    │   ├── gdt.c
    │   ├── idt.c
    │   ├── irq.c
    │   ├── isr.c
    │   ├── keyboard.c
    │   ├── paging.c
    │   ├── kheap.c
    │   ├── timer.c
    │   ├── task.c
    │   ├── syscall.c
    │   ├── fs.c
    │   └── tss.c
    │
    ├── linker.ld
    ├── Makefile
    └── LICENSE

---

# Building

## Requirements

- `x86_64-elf-gcc`
- `x86_64-elf-ld`
- `x86_64-elf-objcopy`
- NASM
- GNU Make

## Clone

    git clone https://github.com/Memo151515/MEMO-kernel.git
    cd MEMO-kernel

## Build

    make

The build produces:

    memo.bin
    memo_flat.bin

## Clean

    make clean

---

# Current Architecture

| Component | Current implementation |
|-----------|------------------------|
| Architecture | x86 |
| Execution mode | 32-bit protected mode |
| Kernel mode | Ring 0 |
| Userspace | Ring 3 |
| Interrupts | IDT / ISR / IRQ |
| System calls | `INT 0x80` |
| Paging | 32-bit paging |
| Timer | PIT |
| Keyboard | PS/2 |
| Display | VGA text mode |
| Filesystem | In-memory |

---

# Current Status

## Implemented

- [x] Boot code
- [x] GDT
- [x] IDT
- [x] ISR handling
- [x] IRQ handling
- [x] Keyboard driver
- [x] VGA text output
- [x] Kernel heap
- [x] Paging
- [x] PIT timer
- [x] Task structures
- [x] Scheduler
- [x] Context switching
- [x] TSS
- [x] Ring 3 userspace
- [x] System calls
- [x] In-memory filesystem
- [x] Kernel shell
- [x] Basic file operations

## Planned

- [ ] Physical memory manager
- [ ] Improved virtual memory manager
- [ ] Process management
- [ ] Better process isolation
- [ ] Persistent filesystem
- [ ] VFS
- [ ] Disk drivers
- [ ] More system calls
- [ ] Userspace program loading
- [ ] Executable format support
- [ ] Shared libraries
- [ ] Device driver framework
- [ ] Networking
- [ ] USB support
- [ ] SMP / multicore support
- [ ] x86_64 long mode
- [ ] GUI
- [ ] More complete userspace environment

---

# Development

MEMO is being developed from the lowest level upward.

The project implements its own fundamental operating system components
instead of depending on an existing general-purpose kernel.

The long-term goal is to continue expanding MEMO into a complete,
modern operating system kernel.

---

# License

MEMO is licensed under the **MEMO Free Code License (MFCL)**.

See [`LICENSE`](LICENSE) for the complete license text.

Copyright (c) 2026 Memo151515.

---

# Author

**Memo151515**

GitHub:

https://github.com/Memo151515

Repository:

https://github.com/Memo151515/MEMO-kernel


---

# TÜRKÇE

# MEMO

> Sıfırdan yazılmış bir işletim sistemi çekirdeği.

MEMO, Memo151515 tarafından C ve x86 assembly kullanılarak sıfırdan
yazılan bir işletim sistemi çekirdeğidir.

MEMO bağımsız olarak geliştirilmektedir ve Linux veya başka bir mevcut
işletim sistemi çekirdeği üzerine kurulmamıştır.

Projenin amacı, bir işletim sistemi için gerekli temel bileşenleri en
alt seviyeden başlayarak kendisi gerçekleştirmektir.

---

# Özellikler

## CPU ve Mimari

- x86 protected mode
- 32-bit çekirdek çalışması
- Ring 0 çekirdek modu
- Ring 3 kullanıcı alanı desteği
- Assembly ile boot kodu
- Özel linker script
- Freestanding C ortamı
- Cross-compilation desteği

## Bellek Yönetimi

MEMO'nun kendi temel bellek yönetimi sistemi bulunmaktadır.

Mevcut sistem:

- Kernel heap
- `kmalloc()`
- Page directory
- Page table
- Paging başlatma
- Identity mapping
- Kullanıcı erişimine açık bellek sayfaları

## Global Descriptor Table

MEMO kendi GDT sistemini kullanır.

Mevcut segmentler:

- Kernel code
- Kernel data
- User code
- User data
- TSS

Bu yapı kernel ve kullanıcı alanı arasındaki privilege seviyelerinin
temelini oluşturur.

## Interrupt Sistemi

MEMO kendi interrupt altyapısını kullanır.

Mevcut özellikler:

- IDT
- ISR
- IRQ handler'ları
- Donanım interrupt kaydı
- Keyboard IRQ
- Timer IRQ
- System call interrupt

## Timer

MEMO programlanabilir interval timer (PIT) kullanır.

Mevcut sistem timer'ı yaklaşık 100 Hz frekansta çalıştırır ve kernel
tick sayacını tutar.

Timer interrupt'ları scheduler tarafından da kullanılır.

## Multitasking

MEMO'da temel bir task scheduler ve context switching sistemi bulunur.

Mevcut özellikler:

- Task yapıları
- Kernel task'ları
- Task oluşturma
- Task stack'leri
- Scheduler
- Task listesi
- Context switching
- Timer tabanlı scheduling

## Kullanıcı Alanı

MEMO'nun başlangıç seviyesinde Ring 3 userspace desteği bulunmaktadır.

Kernel:

1. Userspace stack oluşturabilir.
2. User-mode segmentlerini ayarlayabilir.
3. TSS yapılandırabilir.
4. Kernel stack'i hazırlayabilir.
5. Ring 0'dan Ring 3'e geçebilir.
6. Bir userspace programı çalıştırabilir.

## System Call

MEMO temel bir system call arayüzü sağlar:

    INT 0x80

Şu anda uygulanan system call:

    SYS_WRITE = 1

Userspace, kernel'den çıktı almak için bu arayüzü kullanabilir.

## Klavye Sürücüsü

MEMO, IRQ 1 üzerinden çalışan temel bir PS/2 klavye sürücüsüne sahiptir.

Mevcut özellikler:

- Keyboard scancode işleme
- Scancode → ASCII dönüşümü
- Input buffer
- Enter
- Backspace
- Klavye girişi
- Ekran kaydırma

## VGA Text Mode

MEMO şu anda görüntü için VGA text mode kullanmaktadır.

VGA text buffer:

    0xB8000

Kernel doğrudan VGA belleğine yazı yazabilir ve mevcut sistem 80x25
text mode kullanmaktadır.

## Shell

MEMO'nun kernel içerisinde çalışan basit bir komut satırı arayüzü vardır.

Mevcut komutlar:

    help
    clear
    about
    uptime
    ls
    write <name> <text>
    cat <name>

Örnek:

    write hello Hello from MEMO
    cat hello

## Dosya Sistemi

MEMO'da temel bir RAM tabanlı dosya sistemi bulunmaktadır.

Desteklenen işlemler:

- Dosya oluşturma
- Dosya yazma
- Dosya okuma
- Dosya listeleme
- Dinamik dosya içeriği
- Birden fazla dosya
- Dosya adı yönetimi

Örnek:

    write hello Hello from MEMO
    cat hello
    ls

Mevcut dosya sistemi dosyaları RAM üzerinde tutmaktadır ve henüz
kalıcı disk depolaması kullanmamaktadır.

---

# Proje Yapısı

    MEMO/
    ├── boot/
    │   ├── boot.asm
    │   ├── gdt_flush.asm
    │   ├── idt_flush.asm
    │   ├── irq.asm
    │   ├── isr.asm
    │   ├── paging_enable.asm
    │   ├── switch_to.asm
    │   └── usermode_jump.asm
    │
    ├── kernel/
    │   ├── kernel.c
    │   ├── gdt.c
    │   ├── idt.c
    │   ├── irq.c
    │   ├── isr.c
    │   ├── keyboard.c
    │   ├── paging.c
    │   ├── kheap.c
    │   ├── timer.c
    │   ├── task.c
    │   ├── syscall.c
    │   ├── fs.c
    │   └── tss.c
    │
    ├── linker.ld
    ├── Makefile
    └── LICENSE

---

# Derleme

## Gereksinimler

- `x86_64-elf-gcc`
- `x86_64-elf-ld`
- `x86_64-elf-objcopy`
- NASM
- GNU Make

## Klonlama

    git clone https://github.com/Memo151515/MEMO-kernel.git
    cd MEMO-kernel

## Derleme

    make

Derleme sonucunda:

    memo.bin
    memo_flat.bin

oluşturulur.

## Temizleme

    make clean

---

# Mevcut Mimari

| Bileşen | Mevcut yapı |
|---------|-------------|
| Mimari | x86 |
| Çalışma modu | 32-bit protected mode |
| Kernel modu | Ring 0 |
| Userspace | Ring 3 |
| Interrupt | IDT / ISR / IRQ |
| System call | `INT 0x80` |
| Paging | 32-bit paging |
| Timer | PIT |
| Klavye | PS/2 |
| Görüntü | VGA text mode |
| Dosya sistemi | RAM üzerinde |

---

# Mevcut Durum

## Tamamlananlar

- [x] Boot kodu
- [x] GDT
- [x] IDT
- [x] ISR sistemi
- [x] IRQ sistemi
- [x] Klavye sürücüsü
- [x] VGA text output
- [x] Kernel heap
- [x] Paging
- [x] PIT timer
- [x] Task yapıları
- [x] Scheduler
- [x] Context switching
- [x] TSS
- [x] Ring 3 userspace
- [x] System call
- [x] RAM tabanlı dosya sistemi
- [x] Kernel shell
- [x] Temel dosya işlemleri

## Planlananlar

- [ ] Physical memory manager
- [ ] Gelişmiş virtual memory manager
- [ ] Process yönetimi
- [ ] Daha gelişmiş process izolasyonu
- [ ] Kalıcı dosya sistemi
- [ ] VFS
- [ ] Disk sürücüleri
- [ ] Daha fazla system call
- [ ] Userspace program yükleme
- [ ] Executable format desteği
- [ ] Shared library desteği
- [ ] Device driver framework
- [ ] Networking
- [ ] USB desteği
- [ ] SMP / çok çekirdek desteği
- [ ] x86_64 long mode
- [ ] GUI
- [ ] Daha gelişmiş userspace

---

# Geliştirme

MEMO en alt seviyeden başlayarak geliştirilmektedir.

Proje, mevcut bir genel amaçlı işletim sistemi çekirdeğine dayanmak yerine
işletim sisteminin temel bileşenlerini kendisi gerçekleştirmektedir.

Uzun vadeli hedef, MEMO'yu tamamen çalışan ve modern bir işletim sistemi
çekirdeğine dönüştürmektir.

---

# Lisans

MEMO, **MEMO Free Code License (MFCL)** altında lisanslanmaktadır.

Lisansın tamamı için [`LICENSE`](LICENSE) dosyasına bakabilirsiniz.

Copyright (c) 2026 Memo151515.

---

# Geliştirici

**Memo151515**

GitHub:

https://github.com/Memo151515

Repository:

https://github.com/Memo151515/MEMO-kernel
