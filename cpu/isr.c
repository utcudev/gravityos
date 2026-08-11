/* =============================================================================
 * GravityOS — ISR (Interrupt Service Routines) Implementation
 * =============================================================================
 * CPU exception handler'ları ve IRQ yönetimi
 * ============================================================================= */

#include "idt.h"
#include "../cpu/ports.h"
#include "../lib/stdio.h"

/* PIC portları */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

/* IRQ handler tablosu */
static irq_handler_t irq_handlers[16] = { 0 };

/* CPU exception isimleri */
static const char *exception_names[] = {
    "Division By Zero",           /*  0 */
    "Debug",                      /*  1 */
    "Non Maskable Interrupt",     /*  2 */
    "Breakpoint",                 /*  3 */
    "Overflow",                   /*  4 */
    "Bound Range Exceeded",       /*  5 */
    "Invalid Opcode",             /*  6 */
    "Device Not Available",       /*  7 */
    "Double Fault",               /*  8 */
    "Coprocessor Segment Overrun",/*  9 */
    "Invalid TSS",                /* 10 */
    "Segment Not Present",        /* 11 */
    "Stack-Segment Fault",        /* 12 */
    "General Protection Fault",   /* 13 */
    "Page Fault",                 /* 14 */
    "Reserved",                   /* 15 */
    "x87 Floating-Point",        /* 16 */
    "Alignment Check",            /* 17 */
    "Machine Check",              /* 18 */
    "SIMD Floating-Point",       /* 19 */
    "Virtualization",             /* 20 */
    "Control Protection",         /* 21 */
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved",
    "Hypervisor Injection",       /* 28 */
    "VMM Communication",          /* 29 */
    "Security Exception",         /* 30 */
    "Reserved",                   /* 31 */
};

/* ==========================================================================
 * pic_remap — 8259 PIC'i yeniden eşle
 * IRQ 0-7  → INT 32-39  (Master PIC)
 * IRQ 8-15 → INT 40-47  (Slave PIC)
 * ========================================================================== */
void pic_remap(void)
{
    /* ICW1: Başlatma (cascade mode, ICW4 gerekli) */
    outb(PIC1_CMD,  0x11);
    io_wait();
    outb(PIC2_CMD,  0x11);
    io_wait();

    /* ICW2: Vektör offset */
    outb(PIC1_DATA, 0x20);    /* Master: IRQ 0 → INT 32 */
    io_wait();
    outb(PIC2_DATA, 0x28);    /* Slave:  IRQ 8 → INT 40 */
    io_wait();

    /* ICW3: Cascade bağlantısı */
    outb(PIC1_DATA, 0x04);    /* Master: Slave IRQ2'de */
    io_wait();
    outb(PIC2_DATA, 0x02);    /* Slave:  Cascade identity */
    io_wait();

    /* ICW4: 8086 modu */
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Tüm IRQ'ları maskele (başlangıçta hepsini kapat) */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ==========================================================================
 * pic_send_eoi — End of Interrupt sinyali gönder
 * ========================================================================== */
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);    /* Slave PIC'e EOI */
    }
    outb(PIC1_CMD, 0x20);        /* Master PIC'e EOI */
}

/* ==========================================================================
 * irq_install_handler — Belirli bir IRQ için handler ata
 * ========================================================================== */
void irq_install_handler(int irq, irq_handler_t handler)
{
    if (irq < 0 || irq > 15) return;

    irq_handlers[irq] = handler;

    /* Bu IRQ'nun maskesini kaldır */
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;

        /* Cascade IRQ2'yi de aç */
        value = inb(PIC1_DATA);
        outb(PIC1_DATA, value & ~(1 << 2));
    }

    value = inb(port);
    outb(port, value & ~(1 << irq));
}

/* ==========================================================================
 * irq_uninstall_handler — IRQ handler'ını kaldır
 * ========================================================================== */
void irq_uninstall_handler(int irq)
{
    if (irq < 0 || irq > 15) return;
    irq_handlers[irq] = 0;
}

/* ==========================================================================
 * isr_handler — CPU exception handler (C tarafı)
 * Assembly stub'ları tarafından çağrılır
 * ========================================================================== */
void isr_handler(cpu_state_t *regs)
{
    if (regs->int_no < 32) {
        /* CPU Exception */
        kprintf("\n\n");
        kprintf("=== KERNEL PANIC ===\n");
        kprintf("Exception: %s (#%d)\n", exception_names[regs->int_no], (int)regs->int_no);
        kprintf("Error Code: 0x%lx\n", regs->err_code);
        kprintf("\n--- Register Dump ---\n");
        kprintf("RAX=%016lx  RBX=%016lx\n", regs->rax, regs->rbx);
        kprintf("RCX=%016lx  RDX=%016lx\n", regs->rcx, regs->rdx);
        kprintf("RSI=%016lx  RDI=%016lx\n", regs->rsi, regs->rdi);
        kprintf("RBP=%016lx  RSP=%016lx\n", regs->rbp, regs->rsp);
        kprintf("R8 =%016lx  R9 =%016lx\n", regs->r8,  regs->r9);
        kprintf("R10=%016lx  R11=%016lx\n", regs->r10, regs->r11);
        kprintf("R12=%016lx  R13=%016lx\n", regs->r12, regs->r13);
        kprintf("R14=%016lx  R15=%016lx\n", regs->r14, regs->r15);
        kprintf("RIP=%016lx  CS =%016lx\n", regs->rip, regs->cs);
        kprintf("RFLAGS=%016lx\n", regs->rflags);

        /* Page Fault ek bilgisi */
        if (regs->int_no == 14) {
            uint64_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            kprintf("CR2 (Faulting Address): %016lx\n", cr2);
            kprintf("Cause: %s %s %s\n",
                (regs->err_code & 1) ? "Protection violation" : "Page not present",
                (regs->err_code & 2) ? "Write" : "Read",
                (regs->err_code & 4) ? "User mode" : "Kernel mode");
        }

        kprintf("\nSystem halted.\n");

        /* Sistemi durdur */
        __asm__ volatile("cli; hlt");
        for (;;) {}
    }
}

/* ==========================================================================
 * irq_handler — Donanım IRQ handler (C tarafı)
 * Assembly stub'ları tarafından çağrılır
 * ========================================================================== */
void irq_handler(cpu_state_t *regs)
{
    uint64_t irq_num = regs->int_no - 32;

    /* Kayıtlı handler varsa çağır */
    if (irq_num < 16 && irq_handlers[irq_num]) {
        irq_handlers[irq_num](regs);
    }

    /* EOI gönder */
    pic_send_eoi((uint8_t)irq_num);
}

/* ==========================================================================
 * isr_init — ISR alt sistemini başlat
 * ========================================================================== */
void isr_init(void)
{
    /* IDT'yi kur (bu ISR stub'larını da kaydeder) */
    idt_init();
}
