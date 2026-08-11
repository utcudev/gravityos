/* =============================================================================
 * GravityOS — ELF64 Loader Header
 * ============================================================================= */

#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include <stdint.h>

#define ELF_MAGIC 0x464C457F /* 0x7F 'E' 'L' 'F' */

typedef struct {
    uint32_t magic;
    uint8_t  bitness;     /* 1 = 32-bit, 2 = 64-bit */
    uint8_t  endianness;
    uint8_t  header_version;
    uint8_t  abi;
    uint64_t padding;
    uint16_t type;        /* 2 = Executable */
    uint16_t machine;     /* 0x3E = x86_64 */
    uint32_t version;
    uint64_t entry;       /* Entry point virtual address */
    uint64_t phoff;       /* Program header offset */
    uint64_t shoff;       /* Section header offset */
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf64_header_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD 1

/* ELF'i kullanıcı alanına yükler, giriş noktasını entry_out'a yazar.
   Başarıda 0, hatada -1. Programı başlatmaz. */
int elf_load(uint8_t *file_buffer, uint64_t *entry_out);

/* Yükler ve doğrudan ring 3'te başlatır (geri dönmez) */
void elf_load_and_run(uint8_t *file_buffer);

#endif /* KERNEL_ELF_H */
