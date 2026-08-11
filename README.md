# GravityOS

Sıfırdan yazılmış 64-bit (x86_64) işletim sistemi. Limine ile boot eder, grafik
modda masaüstü çizer ve pencere içinde çalışan bir kabuk (gsh) sunar.

```
   ______                 _ __        ____  _____
  / ____/_______ __   __ (_) /___  __/ __ \/ ___/
 / / __/ ___/ _ \\ \ / // / __/ / / / / / /\__ \
/ /_/ / /  /  __/ \ V // / /_/ /_/ / /_/ /___/ /
\____/_/   \___/  \_//_/\__/\__, /\____//____/
                           /____/
```

## Mimari

| Bileşen | Durum |
|---------|-------|
| Mimari | x86_64 (Long Mode) |
| Bootloader | Limine (BIOS + UEFI, ISO'dan) |
| Diller | C (Zig'in clang'i ile derlenir) + NASM |
| Kernel | Monolitik, higher-half (0xFFFFFFFF80000000) |
| Ekran | Limine framebuffer, 1024x768x32 |
| Konsol | Framebuffer üzerinde 8x8 bitmap font (`drivers/fbcon.c`) |
| Çokluişlem | Preemptive round-robin, PIT IRQ0 (100 Hz) |

Kernel SSE/MMX olmadan derlenir (`-mno-sse -mno-mmx -mno-avx -mno-80387`).
FPU/SSE durumu kurulmadığı için derleyici bu komutları üretirse kernel boot
sırasında çöker.

## Derleme ve Çalıştırma (Windows)

WSL veya cross-compiler gerekmez. Betik NASM, Zig ve Limine'ı ilk çalıştırmada
kendisi indirir.

```powershell
.\build.ps1                  # derle + gravityos.iso üret
.\build.ps1 -Run             # derle + QEMU'da başlat
.\build.ps1 -Run -Headless   # penceresiz, seri çıktı konsola
```

PowerShell 5.1'de komutları `&&` ile değil `;` ile zincirle.

Otomatik boot testi — penceresiz çalıştırır, seri çıktıyı ve ekran görüntüsünü kaydeder:

```powershell
.\scripts\qemu_test.ps1 -Seconds 10 -Keys "help`nmem`n"
```

Çıktılar: `build\serial.log` ve `build\screen.png`.

Üretilen `gravityos.iso` VMware, VirtualBox veya QEMU'da CD/DVD olarak boot edilir.

### Gereksinimler

- Windows PowerShell 5.1+
- Python 3 (Limine indirme ve ekran görüntüsü dönüşümü için)
- QEMU (test için): `winget install SoftwareFreedomConservancy.QEMU`
- `cdrtfe/` altındaki mkisofs (depoda mevcut)

## Proje Yapısı

```
gravityos/
├── kernel/         # kmain, PMM, VMM, heap, scheduler, syscall, ELF loader
├── cpu/            # IDT, ISR/IRQ stub'ları, port I/O
├── drivers/        # serial, timer, keyboard, mouse, framebuffer, fbcon, PCI, fat32
├── gui/            # masaüstü ve pencere çizimi
├── lib/            # string, stdio (kprintf)
├── shell/          # gsh — komut satırı
├── scripts/        # qemu_test.ps1
├── attic/          # kullanım dışı eski kod ve arşiv (build'e dahil değil)
├── build.ps1       # build sistemi
├── limine.conf     # bootloader yapılandırması
└── linker.ld       # kernel linker script
```

## Shell Komutları

| Komut | Açıklama |
|-------|----------|
| `help` | Komut listesi |
| `clear` | Ekranı temizle |
| `echo <msg>` | Mesaj yazdır |
| `mem` | Fiziksel bellek kullanımı |
| `uptime` | Sistem çalışma süresi |
| `gravity` | GravityOS bilgisi |
| `reboot` | Sistemi yeniden başlat |
| `halt` | CPU'yu durdur |

## Durum

Çalışanlar:

- [x] Limine ile BIOS/UEFI boot, higher-half kernel
- [x] Framebuffer sürücüsü + kaydırmalı metin konsolu
- [x] Seri port (hata ayıklama çıktısı)
- [x] IDT + ISR + IRQ, exception'larda register dökümü
- [x] PIT timer (100 Hz), PCI taraması
- [x] PS/2 klavye ve fare — ikisi aynı tamponu paylaşıyor, sürücüler
      birbirinin baytını yutmuyor (status bit 5 ile ayrım)
- [x] Fare imleci — altındaki pikselleri saklayıp geri koyuyor, iz bırakmıyor
- [x] Physical Memory Manager (bitmap, ilk 1 MB rezerve)
- [x] Virtual Memory Manager (4 seviyeli sayfa tabloları, HHDM)
- [x] Kernel heap — serbest liste, blok birleştirme, çalışan `kfree`
- [x] Preemptive scheduler — masaüstü saati ayrı süreçte dönüyor
- [x] Masaüstü + pencere + görev çubuğu
- [x] gsh kabuğu pencere içinde, klavyeden komut alıyor

Henüz yok:

- [ ] Ring 3 (user mode) ve TSS
- [ ] `syscall`/`sysret` girişi — `kernel/syscall.c` yazılı ama tetiklenmiyor
- [ ] Disk sürücüsü (ATA PIO / AHCI) — bu olmadan dosya okunamıyor
- [ ] FAT32 — `drivers/fat32.c` iskelet, `fat32_init` hiçbir yerden çağrılmıyor
- [ ] ELF çalıştırma — `kernel/elf_loader.c` segmentleri haritalıyor, ring 3'e geçmiyor
- [ ] Pencere yöneticisi: taşıma, odak, çoklu pencere, fare tıklaması

Sıradaki adım: TSS + ring 3 geçişi, ardından `syscall` girişi, sonra ATA PIO + FAT32.
Disk okuma çalışmadan ELF yükleyicinin gidecek yeri yok.

### Bilinen sınırlar

- PMM bitmap'i 512 MB ile sınırlı; fazlası kullanılmaz (boot'ta uyarı basar).
- `kmalloc`/`kfree` kilitsiz; scheduler kesme bağlamında `kfree` çağırıyor.
  Tek çekirdekte sorun yok, ring 3 gelmeden önce kilit gerekecek.
- Tek tampon (double buffering yok): imleç metnin üstündeyken o bölgeye yazı
  basılırsa küçük iz kalabilir.
- `attic/` içindekiler derlenmiyor: eski 2 aşamalı bootloader, VGA text sürücüsü,
  Win32/PE yükleyici, sahte terminal uygulaması, eski Makefile ve debug scriptleri.

## Lisans

MIT
