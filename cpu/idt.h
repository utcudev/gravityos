/* =============================================================================
 * GravityOS — IDT (Interrupt Descriptor Table) Header
 * ============================================================================= */

#ifndef CPU_IDT_H
#define CPU_IDT_H

#include <stdint.h>

/* IDT entry sayısı */
#define IDT_ENTRIES 256

/* IDT Gate tipleri */
#define IDT_GATE_INTERRUPT 0x8E  /* Present, Ring 0, Interrupt Gate */
#define IDT_GATE_TRAP      0x8F  /* Present, Ring 0, Trap Gate */
#define IDT_GATE_USER_INT  0xEE  /* Present, Ring 3, Interrupt Gate (syscall) */

/* IDT Entry (64-bit) */
typedef struct __attribute__((packed)) {
    uint16_t offset_low;       /* Offset bits 0-15 */
    uint16_t selector;         /* GDT code segment selector */
    uint8_t  ist;              /* IST (bits 0-2), rest zero */
    uint8_t  type_attr;        /* Gate type and attributes */
    uint16_t offset_mid;       /* Offset bits 16-31 */
    uint32_t offset_high;      /* Offset bits 32-63 */
    uint32_t reserved;         /* Reserved, must be zero */
} idt_entry_t;

/* IDT Pointer (lidt ile yüklenecek) */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idt_ptr_t;

/* CPU durumu — interrupt handler'a aktarılan registerlar */
typedef struct __attribute__((packed)) {
    /* context_switch veya isr_stub tarafından push edilen */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    /* ISR numarası ve hata kodu */
    uint64_t int_no, err_code;

    /* CPU tarafından otomatik push edilen */
    uint64_t rip, cs, rflags, rsp, ss;
} cpu_state_t;

/* IRQ handler fonksiyon tipi */
typedef void (*irq_handler_t)(cpu_state_t *regs);

/* Fonksiyonlar */
void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

/* ISR fonksiyonları */
void isr_init(void);
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

/* PIC fonksiyonları */
void pic_remap(void);
void pic_send_eoi(uint8_t irq);

/* Assembly'de tanımlanan ISR stubleri */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* IRQ stubleri (IRQ 0-15 → INT 32-47) */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

#endif /* CPU_IDT_H */
