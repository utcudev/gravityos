/* =============================================================================
 * GravityOS — IDT (Interrupt Descriptor Table) Implementation
 * ============================================================================= */

#include "idt.h"
#include "gdt.h"
#include "../cpu/ports.h"
#include "../lib/string.h"

/* IDT tablosu ve pointer'ı */
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

/* ==========================================================================
 * idt_set_gate — Bir IDT entry'si ayarla
 * ========================================================================== */
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags)
{
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].reserved    = 0;
}

/* ==========================================================================
 * idt_init — IDT'yi kur ve yükle
 * ========================================================================== */
void idt_init(void)
{
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    /* IDT'yi sıfırla */
    memset(&idt, 0, sizeof(idt));

    /* PIC'i yeniden eşle */
    pic_remap();

    /* CPU exception'ları (ISR 0-31) */
    idt_set_gate(0,  (uint64_t)isr0,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(1,  (uint64_t)isr1,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(2,  (uint64_t)isr2,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(3,  (uint64_t)isr3,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(4,  (uint64_t)isr4,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(5,  (uint64_t)isr5,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(6,  (uint64_t)isr6,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(7,  (uint64_t)isr7,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(8,  (uint64_t)isr8,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(9,  (uint64_t)isr9,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(10, (uint64_t)isr10, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(11, (uint64_t)isr11, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(12, (uint64_t)isr12, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(13, (uint64_t)isr13, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(14, (uint64_t)isr14, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(15, (uint64_t)isr15, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(16, (uint64_t)isr16, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(17, (uint64_t)isr17, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(18, (uint64_t)isr18, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(19, (uint64_t)isr19, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(20, (uint64_t)isr20, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(21, (uint64_t)isr21, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(22, (uint64_t)isr22, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(23, (uint64_t)isr23, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(24, (uint64_t)isr24, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(25, (uint64_t)isr25, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(26, (uint64_t)isr26, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(27, (uint64_t)isr27, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(28, (uint64_t)isr28, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(29, (uint64_t)isr29, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(30, (uint64_t)isr30, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(31, (uint64_t)isr31, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);

    /* IRQ'lar (32-47) */
    idt_set_gate(32, (uint64_t)irq0,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(33, (uint64_t)irq1,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(34, (uint64_t)irq2,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(35, (uint64_t)irq3,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(36, (uint64_t)irq4,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(37, (uint64_t)irq5,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(38, (uint64_t)irq6,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(39, (uint64_t)irq7,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(40, (uint64_t)irq8,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(41, (uint64_t)irq9,  GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(42, (uint64_t)irq10, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(43, (uint64_t)irq11, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(44, (uint64_t)irq12, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(45, (uint64_t)irq13, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(46, (uint64_t)irq14, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(47, (uint64_t)irq15, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);

    /* IDT'yi yükle */
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    /* Interrupt'ları etkinleştir */
    __asm__ volatile("sti");
}
