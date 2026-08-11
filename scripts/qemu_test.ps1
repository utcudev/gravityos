# ==============================================================================
# GravityOS — Otomatik QEMU boot testi
# ==============================================================================
# ISO'yu penceresiz başlatır, seri port çıktısını ve ekran görüntüsünü kaydeder.
#
#   .\scripts\qemu_test.ps1 [-Iso gravityos.iso] [-Seconds 14] [-Keys "help`n"]
#
# Çıktılar: build\serial.log ve build\screen.png
# ==============================================================================

param(
    [string]$Iso = "gravityos.iso",
    [int]$Seconds = 14,
    [string]$Keys = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$isoPath = if ([System.IO.Path]::IsPathRooted($Iso)) { $Iso } else { Join-Path $root $Iso }
if (!(Test-Path $isoPath)) { throw "ISO bulunamadı: $isoPath" }

$buildDir = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$serialLog = Join-Path $buildDir "serial.log"
$ppm       = Join-Path $buildDir "screen.ppm"
$png       = Join-Path $buildDir "screen.png"
foreach ($f in @($serialLog, $ppm, $png)) { if (Test-Path $f) { Remove-Item $f -Force } }

$qemu = (Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue).Source
if (-not $qemu) { $qemu = Join-Path $env:ProgramFiles "qemu\qemu-system-x86_64.exe" }
if (!(Test-Path $qemu)) { throw "QEMU bulunamadı. winget install SoftwareFreedomConservancy.QEMU" }

# Monitör için boş bir TCP portu seç
$port = Get-Random -Minimum 45000 -Maximum 49000

$qargs = @(
    "-cdrom", $isoPath,
    "-boot", "d",          # disk imajı da boot imzası taşıdığı için CD'yi zorla
    "-m", "512M",
    "-no-reboot",
    "-display", "none",
    "-serial", "file:$serialLog",
    "-monitor", "tcp:127.0.0.1:$port,server,nowait"
)

# Sabit disk imajı varsa birincil IDE kanalına bağla
$diskImg = Join-Path $root "disk.img"
if (Test-Path $diskImg) {
    $qargs += @("-drive", "file=$diskImg,format=raw,if=ide,index=0,media=disk")
}

# Ağ: QEMU user-mode (slirp). Ağ geçidi 10.0.2.2, misafir 10.0.2.15.
# Trafiği build\net.pcap dosyasına da yazıyoruz — hata ayıklamada şart.
$qargs += @(
    "-netdev", "user,id=n0",
    "-device", "e1000,netdev=n0",
    "-object", "filter-dump,id=dump0,netdev=n0,file=$buildDir\net.pcap"
)

$proc = Start-Process -FilePath $qemu -ArgumentList $qargs -PassThru -NoNewWindow
Start-Sleep -Seconds $Seconds

# Monitör üzerinden tuş gönder ve ekran görüntüsü al
try {
    $client = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $port)
    $writer = New-Object System.IO.StreamWriter($client.GetStream())
    $writer.AutoFlush = $true
    Start-Sleep -Milliseconds 400

    if ($Keys) {
        foreach ($ch in $Keys.ToCharArray()) {
            $k = switch ($ch) {
                "`n" { "ret" }
                " "  { "spc" }
                "-"  { "minus" }
                "."  { "dot" }
                default { if ($ch -match '[a-z0-9]') { [string]$ch } else { $null } }
            }
            if ($k) { $writer.WriteLine("sendkey $k"); Start-Sleep -Milliseconds 60 }
        }
        Start-Sleep -Seconds 2
    }

    $writer.WriteLine("screendump $ppm")
    Start-Sleep -Seconds 3
    $client.Close()
} catch {
    Write-Host "Monitör bağlantısı kurulamadı: $_" -ForegroundColor Yellow
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# PPM -> PNG (python ile, harici bağımlılık yok)
if (Test-Path $ppm) {
    $py = @"
import struct, zlib
f = open(r'$ppm', 'rb')
def tok():
    t = b''
    while True:
        c = f.read(1)
        if c.isspace():
            if t: return t
        else: t += c
tok(); w = int(tok()); h = int(tok()); tok()
data = f.read()
raw = b''.join(b'\x00' + data[y*w*3:(y+1)*w*3] for y in range(h))
def chunk(t, d):
    c = t + d
    return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw, 6))
       + chunk(b'IEND', b''))
open(r'$png', 'wb').write(png)
print('screenshot:', w, 'x', h)
"@
    Set-Content -Path "$env:TEMP\gos_ppm2png.py" -Value $py
    python "$env:TEMP\gos_ppm2png.py"
    Remove-Item "$env:TEMP\gos_ppm2png.py" -Force
    Remove-Item $ppm -Force
}

Write-Host "--- SERIAL ---" -ForegroundColor Cyan
if (Test-Path $serialLog) { Get-Content $serialLog -Raw } else { Write-Host "(seri çıktı yok)" }
Write-Host "--- PNG: $png ---" -ForegroundColor Cyan
