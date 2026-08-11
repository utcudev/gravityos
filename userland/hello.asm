;; =============================================================================
;; GravityOS — Kullanıcı programı: hello
;; =============================================================================
;; Kernel'in bir parçası DEĞİL. Ayrı bir ELF olarak derlenir, disk imajına
;; kopyalanır ve `run HELLO.ELF` ile ring 3'te çalıştırılır.
;;
;; Kernel'e tek erişim yolu `syscall`.
;; =============================================================================

[BITS 64]

global _start

section .text
_start:
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

    ;; exit(7) — kernel'in çıkış kodunu doğru aldığını görelim
    mov rax, 60
    mov rdi, 7
    syscall

.hang:
    jmp .hang

section .rodata
msg:      db "  [hello.elf] Diskten yuklendim ve ring 3'te calisiyorum.", 10
msg_len:  equ $ - msg

msg2:     db "  [hello.elf] Kendi isletim sisteminde kendi programin. :)", 10
msg2_len: equ $ - msg2
