/* =============================================================================
 * GravityOS — Multitasking & Process Scheduler
 * =============================================================================
 * İşletim sisteminin kalbi olan çoklu işlem (Multitasking) motorudur.
 * Süreçleri (Process) oluşturur ve aralarında geçiş (Context Switch) yapar.
 * ============================================================================= */

#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>
#include "../cpu/idt.h"

/* Süreç Durumları */
typedef enum {
    PROC_STATE_DEAD = 0,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_SLEEPING
} process_state_t;

#define MAX_ALLOCATIONS_PER_PROC 128

/* Her sürecin kernel stack boyutu */
#define PROC_STACK_SIZE 16384

/* Process Control Block (PCB) */
typedef struct {
    uint32_t pid;
    process_state_t state;
    cpu_state_t context;
    uint64_t page_directory; /* CR3 - Sanal Bellek Haritası */
    uint64_t kernel_stack;   /* Stack'in tepesi (rsp başlangıcı) */
    uint64_t stack_base;     /* kmalloc'tan dönen adres — kfree için */
    
    /* Garbage Collection için tahsis edilen RAM sayfalarının listesi */
    void* allocations[MAX_ALLOCATIONS_PER_PROC];
    int alloc_count;
} process_t;

void process_init(void);
uint32_t process_create(void (*entry_point)(void));
void process_schedule(cpu_state_t* current_context);
void process_exit(uint32_t pid);
void process_track_allocation(uint32_t pid, void* phys_addr);
uint32_t process_get_current_pid(void);

#endif /* KERNEL_PROCESS_H */
