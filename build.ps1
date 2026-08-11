# ==============================================================================
# GravityOS — Windows Native Build Script
# ==============================================================================
# WSL gerektirmez. Zig'i cross-compiler, NASM'ı assembler, Limine'ı bootloader
# olarak kullanıp boot edilebilir bir ISO üretir.
#
#   .\build.ps1          -> derle + ISO üret
#   .\build.ps1 -Run     -> derle + ISO üret + QEMU'da başlat
#   .\build.ps1 -Run -Headless  -> pencere açmadan çalıştır, seri çıktı konsola
# ==============================================================================

param(
    [switch]$Run,
    [switch]$Headless
)

$ErrorActionPreference = "Stop"

function Fail($msg) {
    Write-Host "[HATA] $msg" -ForegroundColor Red
    exit 1
}

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " GravityOS Native Windows Builder" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

$cwd      = (Get-Location).Path
$toolsDir = Join-Path $cwd "tools"
$buildDir = Join-Path $cwd "build"

New-Item -ItemType Directory -Force -Path $toolsDir, $buildDir | Out-Null

# ------------------------------------------------------------------ 1. NASM
If (!(Test-Path "$toolsDir\nasm\nasm.exe")) {
    Write-Host "[1/5] NASM indiriliyor..." -ForegroundColor Yellow
    $nasmUrl = "https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip"
    Invoke-WebRequest -Uri $nasmUrl -OutFile "$toolsDir\nasm.zip"
    Expand-Archive -Path "$toolsDir\nasm.zip" -DestinationPath $toolsDir -Force
    Rename-Item -Path "$toolsDir\nasm-2.16.03" -NewName "nasm" -Force
    Remove-Item -Path "$toolsDir\nasm.zip" -Force
} else {
    Write-Host "[1/5] NASM mevcut." -ForegroundColor DarkGray
}

# ------------------------------------------------------------------- 2. Zig
If (!(Test-Path "$toolsDir\zig\zig.exe")) {
    Write-Host "[2/5] Zig indiriliyor (birkaç dakika sürebilir)..." -ForegroundColor Yellow
    $zigUrl = "https://ziglang.org/download/0.11.0/zig-windows-x86_64-0.11.0.zip"
    Invoke-WebRequest -Uri $zigUrl -OutFile "$toolsDir\zig.zip"
    Expand-Archive -Path "$toolsDir\zig.zip" -DestinationPath $toolsDir -Force
    Rename-Item -Path "$toolsDir\zig-windows-x86_64-0.11.0" -NewName "zig" -Force
    Remove-Item -Path "$toolsDir\zig.zip" -Force
} else {
    Write-Host "[2/5] Zig mevcut." -ForegroundColor DarkGray
}

$env:PATH = "$toolsDir\nasm;$toolsDir\zig;" + $env:PATH

# --------------------------------------------------------------- 3. Limine
$limineDir = "$toolsDir\limine-bin\limine-binary"
If (!(Test-Path "$limineDir\limine-bios.sys")) {
    Write-Host "[3/5] Limine bootloader indiriliyor..." -ForegroundColor Yellow
    $py = @"
import urllib.request, json, zipfile
req = urllib.request.urlopen('https://api.github.com/repos/limine-bootloader/limine/releases/latest')
data = json.loads(req.read())
for asset in data['assets']:
    if asset['name'] == 'limine-binary.zip':
        urllib.request.urlretrieve(asset['browser_download_url'], 'limine.zip')
        with zipfile.ZipFile('limine.zip', 'r') as z:
            z.extractall('tools/limine-bin')
        break
"@
    Set-Content -Path "$env:TEMP\dl_limine.py" -Value $py
    python "$env:TEMP\dl_limine.py"
    Remove-Item "$env:TEMP\dl_limine.py" -Force
    If (Test-Path "limine.zip") { Remove-Item "limine.zip" -Force }
} else {
    Write-Host "[3/5] Limine mevcut." -ForegroundColor DarkGray
}

If (!(Test-Path "$limineDir\limine-bios.sys")) { Fail "Limine indirilemedi." }

# --------------------------------------------------------------- 4. Kernel
Write-Host "[4/5] Kernel derleniyor..." -ForegroundColor Yellow

$c_files = @(
    "kernel/kernel.c",
    "kernel/pmm.c",
    "kernel/vmm.c",
    "kernel/heap.c",
    "kernel/process.c",
    "kernel/syscall.c",
    "kernel/usermode.c",
    "kernel/elf_loader.c",
    "kernel/vfs.c",
    "cpu/gdt.c",
    "cpu/idt.c",
    "cpu/isr.c",
    "drivers/serial.c",
    "drivers/timer.c",
    "drivers/keyboard.c",
    "drivers/mouse.c",
    "drivers/fb.c",
    "drivers/font.c",
    "drivers/fbcon.c",
    "drivers/fat32.c",
    "drivers/pci.c",
    "drivers/ata.c",
    "gui/window.c",
    "lib/string.c",
    "lib/stdio.c",
    "shell/shell.c"
)

$obj_files = @()

$asm_files = @(
    "cpu/isr_stub.asm",
    "cpu/syscall_entry.asm",
    "cpu/usermode_prog.asm"
)

foreach ($file in $asm_files) {
    if (!(Test-Path $file)) { Fail "Kaynak dosya bulunamadı: $file" }
    $obj = Join-Path $buildDir ([System.IO.Path]::GetFileNameWithoutExtension($file) + ".o")
    Write-Host "      AS $file" -ForegroundColor DarkGray
    & nasm -f elf64 $file -o $obj
    if ($LASTEXITCODE -ne 0) { Fail "nasm $file derlenemedi." }
    $obj_files += $obj
}

# SSE/MMX kapalı olmalı: kernel FPU/SSE durumunu kurmuyor, derleyici bu
# komutları üretirse CPU boot sırasında exception atar.
$cflags = @(
    "-ffreestanding", "-fno-stack-protector", "-fno-pic", "-fno-pie",
    "-mno-red-zone", "-mcmodel=kernel",
    "-mno-mmx", "-mno-sse", "-mno-sse2", "-mno-avx", "-mno-80387",
    "-O2", "-Wall", "-Wextra",
    "-std=gnu11", "-target", "x86_64-freestanding"
)

foreach ($file in $c_files) {
    if (!(Test-Path $file)) { Fail "Kaynak dosya bulunamadı: $file" }
    $obj = Join-Path $buildDir ([System.IO.Path]::GetFileNameWithoutExtension($file) + ".o")
    $obj_files += $obj
    Write-Host "      CC $file" -ForegroundColor DarkGray
    & zig cc @cflags -c $file -o $obj
    if ($LASTEXITCODE -ne 0) { Fail "$file derlenemedi." }
}

Write-Host "      LD kernel.elf" -ForegroundColor DarkGray
& zig cc "-nostdlib" "-T" "linker.ld" "-Wl,-z,max-page-size=0x1000" @obj_files `
    "-o" "$buildDir\kernel.elf" "-target" "x86_64-freestanding"
if ($LASTEXITCODE -ne 0) { Fail "Kernel linklenemedi." }

# ------------------------------------------------------------------ 5. ISO
Write-Host "[5/5] ISO üretiliyor..." -ForegroundColor Yellow

$mkisofs = "$cwd\cdrtfe\cdrtfe-1.5.9\tools\cdrtools\mkisofs.exe"
if (!(Test-Path $mkisofs)) { Fail "mkisofs bulunamadı: $mkisofs" }
$env:PATH = "$cwd\cdrtfe\cdrtfe-1.5.9\tools\cygwin;" + $env:PATH

$isoRoot = Join-Path $cwd "iso_root"
If (Test-Path $isoRoot) { Remove-Item -Recurse -Force $isoRoot }
New-Item -ItemType Directory -Path "$isoRoot\boot\limine" -Force | Out-Null

Copy-Item "$buildDir\kernel.elf" "$isoRoot\boot\"
# Limine v8+ yapılandırma dosyası: limine.conf (eski adı limine.cfg).
# Bootloader'ın aradığı konumların hepsine koyuyoruz.
Copy-Item "$cwd\limine.conf" "$isoRoot\boot\limine\"
Copy-Item "$cwd\limine.conf" "$isoRoot\boot\"
Copy-Item "$cwd\limine.conf" "$isoRoot\"
Copy-Item "$limineDir\limine-bios.sys"     "$isoRoot\boot\limine\"
Copy-Item "$limineDir\limine-bios-cd.bin"  "$isoRoot\boot\limine\"
Copy-Item "$limineDir\limine-uefi-cd.bin"  "$isoRoot\boot\limine\"

$mkisofs_args = @(
    "-J", "-R", "-V", "GravityOS",
    "-b", "boot/limine/limine-bios-cd.bin",
    "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table",
    "-eltorito-alt-boot",
    "-eltorito-platform", "0xEF",
    "-b", "boot/limine/limine-uefi-cd.bin",
    "-no-emul-boot",
    "-o", "$cwd\gravityos.iso",
    $isoRoot
)
& $mkisofs @mkisofs_args
if ($LASTEXITCODE -ne 0) { Fail "mkisofs ISO üretemedi." }

# BIOS boot sector
$limine_exe = "$limineDir\limine-tool-windows-x86\limine.exe"
if (Test-Path $limine_exe) {
    & $limine_exe bios-install "$cwd\gravityos.iso" | Out-Null
} else {
    Write-Host "      UYARI: limine.exe yok, BIOS boot sector yazılmadı (UEFI yine çalışır)." -ForegroundColor Yellow
}

Write-Host "=============================================" -ForegroundColor Green
Write-Host " TAMAM: gravityos.iso hazır." -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green

# ------------------------------------------------------------- QEMU ile çalıştır
if ($Run) {
    $qemu = (Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue).Source
    if (-not $qemu) { $qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe" }
    if (!(Test-Path $qemu)) { Fail "QEMU bulunamadı. Kurulum: winget install SoftwareFreedomConservancy.QEMU" }

    # -boot d: veri diski de boot imzası taşıdığından CD'den açılmayı zorla
    $qargs = @("-cdrom", "$cwd\gravityos.iso", "-boot", "d",
               "-m", "512M", "-no-reboot", "-no-shutdown")

    # Sabit disk imajı varsa bağla
    if (Test-Path "$cwd\disk.img") {
        $qargs += @("-drive", "file=$cwd\disk.img,format=raw,if=ide,index=0,media=disk")
    }
    if ($Headless) {
        $qargs += @("-display", "none", "-serial", "stdio")
    } else {
        $qargs += @("-serial", "stdio")
    }

    Write-Host "QEMU başlatılıyor..." -ForegroundColor Cyan
    & $qemu @qargs
}
