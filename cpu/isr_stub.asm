;; =============================================================================
;; GravityOS — ISR Assembly Stubs
;; =============================================================================
;; Her interrupt için register kaydetme/geri yükleme stub'ları
;; CPU exception'ları (0-31) ve IRQ'lar (32-47)
;; =============================================================================

[BITS 64]

;; C tarafındaki handler fonksiyonları
extern isr_handler
extern irq_handler

;; =============================================================================
;; Ortak makrolar
;; =============================================================================

;; Hata kodu OLMAYAN ISR stub'ı (sahte hata kodu push eder)
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0               ; Sahte hata kodu
    push qword %1              ; Interrupt numarası
    jmp isr_common_stub
%endmacro

;; Hata kodu OLAN ISR stub'ı (CPU zaten push etti)
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    ; Hata kodu zaten stack'te
    push qword %1              ; Interrupt numarası
    jmp isr_common_stub
%endmacro

;; IRQ stub'ı
%macro IRQ 2
global irq%1
irq%1:
    push qword 0               ; Sahte hata kodu
    push qword %2              ; Interrupt numarası (32 + IRQ no)
    jmp irq_common_stub
%endmacro

;; =============================================================================
;; CPU Exception Stub'ları (ISR 0-31)
;; =============================================================================

ISR_NOERRCODE 0    ; Division By Zero
ISR_NOERRCODE 1    ; Debug
ISR_NOERRCODE 2    ; Non Maskable Interrupt
ISR_NOERRCODE 3    ; Breakpoint
ISR_NOERRCODE 4    ; Overflow
ISR_NOERRCODE 5    ; Bound Range Exceeded
ISR_NOERRCODE 6    ; Invalid Opcode
ISR_NOERRCODE 7    ; Device Not Available
ISR_ERRCODE   8    ; Double Fault
ISR_NOERRCODE 9    ; Coprocessor Segment Overrun
ISR_ERRCODE   10   ; Invalid TSS
ISR_ERRCODE   11   ; Segment Not Present
ISR_ERRCODE   12   ; Stack-Segment Fault
ISR_ERRCODE   13   ; General Protection Fault
ISR_ERRCODE   14   ; Page Fault
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; x87 Floating-Point Exception
ISR_ERRCODE   17   ; Alignment Check
ISR_NOERRCODE 18   ; Machine Check
ISR_NOERRCODE 19   ; SIMD Floating-Point Exception
ISR_NOERRCODE 20   ; Virtualization Exception
ISR_ERRCODE   21   ; Control Protection Exception
ISR_NOERRCODE 22   ; Reserved
ISR_NOERRCODE 23   ; Reserved
ISR_NOERRCODE 24   ; Reserved
ISR_NOERRCODE 25   ; Reserved
ISR_NOERRCODE 26   ; Reserved
ISR_NOERRCODE 27   ; Reserved
ISR_NOERRCODE 28   ; Hypervisor Injection Exception
ISR_NOERRCODE 29   ; VMM Communication Exception
ISR_ERRCODE   30   ; Security Exception
ISR_NOERRCODE 31   ; Reserved

;; =============================================================================
;; IRQ Stub'ları (IRQ 0-15 → INT 32-47)
;; =============================================================================

IRQ 0,  32    ; PIT Timer
IRQ 1,  33    ; Keyboard
IRQ 2,  34    ; Cascade
IRQ 3,  35    ; COM2
IRQ 4,  36    ; COM1
IRQ 5,  37    ; LPT2
IRQ 6,  38    ; Floppy Disk
IRQ 7,  39    ; LPT1 / Spurious
IRQ 8,  40    ; CMOS Real Time Clock
IRQ 9,  41    ; ACPI
IRQ 10, 42    ; Open
IRQ 11, 43    ; Open
IRQ 12, 44    ; PS/2 Mouse
IRQ 13, 45    ; FPU / Coprocessor
IRQ 14, 46    ; Primary ATA
IRQ 15, 47    ; Secondary ATA

;; =============================================================================
;; Ortak ISR Stub — Registerları kaydet, C handler'ı çağır, geri yükle
;; =============================================================================
isr_common_stub:
    ;; Tüm genel amaçlı registerları kaydet
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ;; C handler'ı çağır — RDI = stack pointer (cpu_state_t*)
    mov rdi, rsp
    
    ;; GDT'de 64-bit Data Segment'ini (kendi GDT = 0x10) yükle
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    call isr_handler

    ;; Registerları geri yükle
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ;; int_no ve err_code'u temizle
    add rsp, 16

    ;; Interrupt'tan dön
    iretq

;; =============================================================================
;; Ortak IRQ Stub — Registerları kaydet, C handler'ı çağır, geri yükle
;; =============================================================================
irq_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    
    ;; GDT'de 64-bit Data Segment'ini (kendi GDT = 0x10) yükle
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq
