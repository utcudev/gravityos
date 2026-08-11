/* =============================================================================
 * GravityOS — ELF64 yükleyici
 * =============================================================================
 * Diskten okunmuş bir ELF64 çalıştırılabilirini kullanıcı adres alanına
 * yerleştirip ring 3'te başlatır. Dinamik bağlama yok — statik ELF bekler.
 * ============================================================================= */

#include "elf.h"
#include "usermode.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

/* ELF'i kullanıcı adres alanına yerleştirir ve giriş noktasını döner.
   Başarıda 0. Çağıran, dosya tamponunu serbest bıraktıktan sonra
   usermode_enter ile programı başlatabilir. */
int elf_load(uint8_t *file_buffer, uint64_t *entry_out)
{
    elf64_header_t *header = (elf64_header_t *)file_buffer;

    if (header->magic != ELF_MAGIC) {
        kprintf("[ELF] Not an ELF file (bad magic).\n");
        return -1;
    }

    if (header->bitness != 2 || header->machine != 0x3E) {
        kprintf("[ELF] Only 64-bit x86_64 executables are supported.\n");
        return -1;
    }

    if (header->phnum == 0) {
        kprintf("[ELF] No program headers.\n");
        return -1;
    }

    kprintf("[ELF] Entry point 0x%lx, %u program headers\n",
            header->entry, header->phnum);

    /* Önceki programdan kalan haritalamalar bu programın segmentlerini
       bozmasın diye kullanıcı adres alanını temizle */
    usermode_reset_address_space();

    elf64_phdr_t *phdr = (elf64_phdr_t *)(file_buffer + header->phoff);

    for (int i = 0; i < header->phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        uint64_t vaddr = phdr[i].p_vaddr;
        uint64_t memsz = phdr[i].p_memsz;

        /* Kernel alanına yükleme girişimini reddet */
        if (vaddr >= 0xFFFF800000000000ULL) {
            kprintf("[ELF] Refusing to load segment into kernel space (0x%lx)\n", vaddr);
            return -1;
        }

        kprintf("[ELF] Segment %d -> 0x%lx (%lu bytes in file, %lu in memory)\n",
                i, vaddr, phdr[i].p_filesz, memsz);

        if (usermode_map_range(vaddr, memsz) != 0) return -1;

        /* Dosyadaki baytları kopyala; kalan kısım (.bss) zaten sıfırlı */
        if (phdr[i].p_filesz > 0) {
            memcpy((void *)vaddr, file_buffer + phdr[i].p_offset, phdr[i].p_filesz);
        }
    }

    /* Kullanıcı stack'i */
    if (usermode_map_range(USER_STACK_VIRT, USER_STACK_SIZE) != 0) return -1;

    *entry_out = header->entry;
    return 0;
}

void elf_load_and_run(uint8_t *file_buffer)
{
    uint64_t entry;
    if (elf_load(file_buffer, &entry) != 0) return;

    usermode_enter(entry, USER_STACK_VIRT + USER_STACK_SIZE - 16);
}
