/* =============================================================================
 * GravityOS — ELF64 (Linux) Binary Loader
 * ============================================================================= */

#include "elf.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "vmm.h"
#include "pmm.h"
#include "process.h"

void elf_load_and_run(uint8_t *file_buffer)
{
    elf64_header_t* header = (elf64_header_t*)file_buffer;

    if (header->magic != ELF_MAGIC) {
        kprintf("[ELF LOADER] Not a valid ELF file (Magic mismatch).\n");
        return;
    }

    if (header->bitness != 2 || header->machine != 0x3E) {
        kprintf("[ELF LOADER] Only 64-bit x86_64 ELF files are supported.\n");
        return;
    }

    kprintf("[ELF LOADER] Loading Linux Executable (ELF64)...\n");
    kprintf("[ELF LOADER] Entry Point: 0x%lx\n", header->entry);

    /* Program Header Tablosunu oku */
    elf64_phdr_t* phdr = (elf64_phdr_t*)(file_buffer + header->phoff);

    for (int i = 0; i < header->phnum; i++) {
        /* Sadece yüklenmesi gereken (PT_LOAD) bölümleri hafızaya al */
        if (phdr[i].p_type == PT_LOAD) {
            kprintf("[ELF LOADER] Mapping segment at vaddr 0x%lx (Size: %lu bytes)\n", 
                    phdr[i].p_vaddr, phdr[i].p_memsz);

            uint64_t v_addr = phdr[i].p_vaddr;
            uint64_t size = phdr[i].p_memsz;
            
            /* Sayfa hizalaması (Page alignment) yapalım */
            uint64_t v_addr_aligned = v_addr & ~(PMM_PAGE_SIZE - 1);
            
            for (uint64_t offset = 0; offset < size; offset += PMM_PAGE_SIZE) {
                void* phys = pmm_alloc_page();
                if (phys) {
                    process_track_allocation(process_get_current_pid(), phys);
                    vmm_map_page((uint64_t)phys, v_addr_aligned + offset, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
                }
            }

            /* Dosyadaki asıl baytları yeni bağladığımız sanal hafızaya kopyala */
            if (phdr[i].p_filesz > 0) {
                memcpy((void*)v_addr, file_buffer + phdr[i].p_offset, phdr[i].p_filesz);
            }
            
            /* Eğer memory size > file size ise, kalanı 0 ile doldur (.bss bölümü) */
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((void*)(v_addr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    kprintf("[ELF LOADER] Program mapped into memory. Ready to execute in User Mode.\n");

    /* TODO: Ring 3 (User Mode) geçişi ve Process oluşturma işlemleri */
}
