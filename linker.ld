/* =============================================================================
 * GravityOS — Linker Script (Limine boot protocol)
 * =============================================================================
 * Kernel higher-half'ta (0xFFFFFFFF80000000) çalışır. Limine ELF'i doğrudan
 * yükler; fiziksel adresleri kendisi seçer, bu yüzden AT() kullanılmaz.
 *
 * Her program header (PT_LOAD) farklı izinlere sahip olduğundan bölümler
 * sayfa sınırında ayrılmak zorunda — aksi halde Limine "PHDRs with different
 * permissions sharing the same memory page" diyerek yüklemeyi reddeder.
 * ============================================================================= */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    text    PT_LOAD;
    rodata  PT_LOAD;
    data    PT_LOAD;
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    /* Limine istek yapıları — salt okunur bölümde kalmalı */
    .requests : {
        KEEP(*(.requests_start_marker))
        KEEP(*(.requests))
        KEEP(*(.requests.*))
        KEEP(*(.requests_end_marker))
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
