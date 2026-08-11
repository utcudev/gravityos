#!/usr/bin/env python3
"""
GravityOS — FAT32 test diski üretici

Ham (raw) bir disk imajı oluşturur, FAT32 olarak biçimlendirir ve içine
scripts/diskfiles/ altındaki dosyaları kopyalar.

Kullanım:
    python scripts/make_disk.py [cikti.img]

Not: Uzun dosya adı (LFN) desteği yok — dosyalar 8.3 biçimine çevrilir.
"""

import os
import struct
import sys

SECTOR = 512
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 32
NUM_FATS = 2
IMAGE_MB = 64
ROOT_CLUSTER = 2

ATTR_ARCHIVE = 0x20
ATTR_VOLUME_ID = 0x08


def short_name(name):
    """Dosya adını 11 baytlık 8.3 biçimine çevir."""
    name = name.upper()
    if "." in name:
        base, ext = name.rsplit(".", 1)
    else:
        base, ext = name, ""
    base = "".join(c for c in base if c.isalnum() or c in "_-")[:8]
    ext = "".join(c for c in ext if c.isalnum())[:3]
    return (base.ljust(8) + ext.ljust(3)).encode("ascii")


def build_boot_sector(total_sectors, fat_size):
    bs = bytearray(SECTOR)
    bs[0:3] = b"\xEB\x58\x90"
    bs[3:11] = b"GRAVITY "
    struct.pack_into("<H", bs, 11, SECTOR)
    bs[13] = SECTORS_PER_CLUSTER
    struct.pack_into("<H", bs, 14, RESERVED_SECTORS)
    bs[16] = NUM_FATS
    struct.pack_into("<H", bs, 17, 0)          # FAT32'de kök dizin girdisi yok
    struct.pack_into("<H", bs, 19, 0)          # total_sectors16 kullanılmıyor
    bs[21] = 0xF8                              # sabit disk
    struct.pack_into("<H", bs, 22, 0)          # FAT32'de fat_size16 = 0
    struct.pack_into("<H", bs, 24, 32)         # sectors per track
    struct.pack_into("<H", bs, 26, 8)          # heads
    struct.pack_into("<I", bs, 28, 0)          # hidden sectors
    struct.pack_into("<I", bs, 32, total_sectors)
    struct.pack_into("<I", bs, 36, fat_size)
    struct.pack_into("<H", bs, 40, 0)          # ext flags
    struct.pack_into("<H", bs, 42, 0)          # fs version
    struct.pack_into("<I", bs, 44, ROOT_CLUSTER)
    struct.pack_into("<H", bs, 48, 1)          # FSInfo sektörü
    struct.pack_into("<H", bs, 50, 6)          # yedek boot sektörü
    bs[64] = 0x80                              # sürücü numarası
    bs[66] = 0x29                              # genişletilmiş imza
    struct.pack_into("<I", bs, 67, 0x47524156) # birim seri numarası
    bs[71:82] = b"GRAVITYOS  "
    bs[82:90] = b"FAT32   "
    bs[510] = 0x55
    bs[511] = 0xAA
    return bs


def build_fsinfo():
    fsi = bytearray(SECTOR)
    struct.pack_into("<I", fsi, 0, 0x41615252)
    struct.pack_into("<I", fsi, 484, 0x61417272)
    struct.pack_into("<I", fsi, 488, 0xFFFFFFFF)  # boş küme sayısı bilinmiyor
    struct.pack_into("<I", fsi, 492, 0xFFFFFFFF)
    fsi[510] = 0x55
    fsi[511] = 0xAA
    return fsi


def dir_entry(name11, attr, cluster, size):
    e = bytearray(32)
    e[0:11] = name11
    e[11] = attr
    struct.pack_into("<H", e, 14, 0x0000)      # oluşturma saati
    struct.pack_into("<H", e, 16, 0x5A21)      # oluşturma tarihi (2025-01-01)
    struct.pack_into("<H", e, 18, 0x5A21)
    struct.pack_into("<H", e, 20, (cluster >> 16) & 0xFFFF)
    struct.pack_into("<H", e, 22, 0x0000)
    struct.pack_into("<H", e, 24, 0x5A21)
    struct.pack_into("<H", e, 26, cluster & 0xFFFF)
    struct.pack_into("<I", e, 28, size)
    return e


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "disk.img"
    src_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "diskfiles")

    total_sectors = IMAGE_MB * 1024 * 1024 // SECTOR

    # FAT boyutunu kabaca hesapla: her küme için 4 bayt
    data_sectors = total_sectors - RESERVED_SECTORS
    clusters = data_sectors // SECTORS_PER_CLUSTER
    fat_size = (clusters * 4 + SECTOR - 1) // SECTOR
    # FAT'ler de veri alanından yer kaptığı için bir kez daha daralt
    data_sectors = total_sectors - RESERVED_SECTORS - NUM_FATS * fat_size
    clusters = data_sectors // SECTORS_PER_CLUSTER

    image = bytearray(total_sectors * SECTOR)
    image[0:SECTOR] = build_boot_sector(total_sectors, fat_size)
    image[SECTOR:2 * SECTOR] = build_fsinfo()
    # Yedek boot sektörü
    image[6 * SECTOR:7 * SECTOR] = image[0:SECTOR]

    fat = [0] * (fat_size * SECTOR // 4)
    fat[0] = 0x0FFFFFF8
    fat[1] = 0x0FFFFFFF
    fat[ROOT_CLUSTER] = 0x0FFFFFFF  # kök dizin tek küme

    first_data_sector = RESERVED_SECTORS + NUM_FATS * fat_size

    def cluster_offset(cluster):
        return (first_data_sector + (cluster - ROOT_CLUSTER) * SECTORS_PER_CLUSTER) * SECTOR

    entries = bytearray()
    entries += dir_entry(b"GRAVITYOS  ", ATTR_VOLUME_ID, 0, 0)

    next_cluster = ROOT_CLUSTER + 1
    files = []
    if os.path.isdir(src_dir):
        files = sorted(f for f in os.listdir(src_dir)
                       if os.path.isfile(os.path.join(src_dir, f)))

    for fname in files:
        data = open(os.path.join(src_dir, fname), "rb").read()
        n_clusters = max(1, (len(data) + SECTOR * SECTORS_PER_CLUSTER - 1)
                         // (SECTOR * SECTORS_PER_CLUSTER))
        start = next_cluster

        for i in range(n_clusters):
            c = start + i
            fat[c] = 0x0FFFFFFF if i == n_clusters - 1 else c + 1

        off = cluster_offset(start)
        image[off:off + len(data)] = data

        entries += dir_entry(short_name(fname), ATTR_ARCHIVE, start, len(data))
        next_cluster += n_clusters
        print(f"  + {fname:20s} {len(data):7d} bayt  -> kume {start}")

    # Kök dizini yaz
    root_off = cluster_offset(ROOT_CLUSTER)
    image[root_off:root_off + len(entries)] = entries

    # FAT'leri yaz (iki kopya)
    fat_bytes = b"".join(struct.pack("<I", v) for v in fat)
    for i in range(NUM_FATS):
        off = (RESERVED_SECTORS + i * fat_size) * SECTOR
        image[off:off + len(fat_bytes)] = fat_bytes

    with open(out_path, "wb") as f:
        f.write(image)

    print(f"{out_path}: {IMAGE_MB} MB, FAT32, {clusters} kume, "
          f"{len(files)} dosya, veri alani sektor {first_data_sector}")


if __name__ == "__main__":
    main()
