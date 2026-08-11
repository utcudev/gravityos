/* =============================================================================
 * GravityOS — Kullanıcı programı linker script
 * =============================================================================
 * Programlar 0x400000'e bağlanır. Kernel higher-half'ta (0xFFFF...) durduğu
 * için bu adres alanı tamamen kullanıcıya aittir.
 * ============================================================================= */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
