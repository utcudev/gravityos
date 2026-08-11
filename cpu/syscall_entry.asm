;; =============================================================================
;; GravityOS — SYSCALL giriş noktası
;; =============================================================================
;; `syscall` komutu çalıştığında CPU:
;;   RCX <- dönüş RIP, R11 <- RFLAGS, CS/SS <- STAR'daki kernel seçicileri
;; Stack DEĞİŞMEZ — hâlâ kullanıcının stack'i üstündeyiz. İlk iş kernel
;; stack'ine geçmek.
;;
;; Linux ABI: numara RAX, argümanlar RDI, RSI, RDX, R10, R8, R9
;; SysV C ABI: RDI, RSI, RDX, RCX, R8, R9
;; =============================================================================

[BITS 64]

extern syscall_handler
extern syscall_kernel_stack     ; uint64_t — kernel stack tepesi (C tarafında)

global syscall_entry

section .data
saved_user_rsp: dq 0

section .text
syscall_entry:
    mov [rel saved_user_rsp], rsp
    mov rsp, [rel syscall_kernel_stack]

    push rcx                    ; kullanıcının dönüş adresi
    push r11                    ; kullanıcının RFLAGS'i

    ;; Argümanları Linux ABI'sinden C ABI'sine kaydır.
    ;; Sıra önemli: hedef register'lar birbirini ezmesin.
    mov r9, r8                  ; arg5
    mov r8, r10                 ; arg4
    mov rcx, rdx                ; arg3
    mov rdx, rsi                ; arg2
    mov rsi, rdi                ; arg1
    mov rdi, rax                ; syscall numarası

    call syscall_handler        ; dönüş değeri RAX'te kalır

    pop r11
    pop rcx

    mov rsp, [rel saved_user_rsp]
    o64 sysret                  ; ring 3'e dön (CS/SS STAR'dan, RIP=RCX, RFLAGS=R11)
