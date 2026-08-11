;; =============================================================================
;; GravityOS — Ring 3 test programı
;; =============================================================================
;; Kernel imajının içinde durur, çalıştırılmadan önce kullanıcı sayfasına
;; kopyalanır. Tüm adresleme RIP-göreli olduğu için hangi adrese kopyalanırsa
;; kopyalansın çalışır.
;;
;; Kernel'in hiçbir fonksiyonunu çağırmaz — tek iletişim yolu `syscall`.
;; =============================================================================

[BITS 64]

global user_prog_start
global user_prog_end

section .rodata

user_prog_start:
    ;; write(1, msg, msg_len)
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg]
    mov rdx, msg_len
    syscall

    ;; write(1, msg2, msg2_len)
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg2]
    mov rdx, msg2_len
    syscall

    ;; exit(0)
    mov rax, 60
    xor rdi, rdi
    syscall

.hang:
    jmp .hang

msg:      db "  [ring 3] Merhaba! Ben kullanici modunda calisiyorum.", 10
msg_len:  equ $ - msg

msg2:     db "  [ring 3] Kernel'e sadece syscall ile ulasabiliyorum.", 10
msg2_len: equ $ - msg2

user_prog_end:
