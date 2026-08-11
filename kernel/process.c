/* =============================================================================
 * GravityOS — Multitasking & Process Scheduler Implementation
 * ============================================================================= */

#include "process.h"
#include "heap.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "pmm.h"

#define MAX_PROCESSES 64
static process_t processes[MAX_PROCESSES];
static int current_process_index = -1;
static uint32_t next_pid = 1;

/* Limine'in kurduğu GDT'deki 64-bit kod/veri seçicileri. Sabit varsaymak
   yerine çalışma anında okunur — yanlış seçiciyle iretq anında çöker. */
static uint64_t kernel_cs = 0x28;
static uint64_t kernel_ss = 0x30;

void process_init(void)
{
    memset(processes, 0, sizeof(processes));

    uint16_t cs, ss;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));
    kernel_cs = cs;
    kernel_ss = ss;

    /* İşletim sisteminin şu anki (kernel) durumu "Process 0" olarak kaydedilir */
    processes[0].pid = 0;
    processes[0].state = PROC_STATE_RUNNING;
    processes[0].alloc_count = 0;
    current_process_index = 0;

    kprintf("[SCHEDULER] Multitasking Engine Initialized.\n");
}

uint32_t process_create(void (*entry_point)(void))
{
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_DEAD) {
            processes[i].pid = next_pid++;
            /* Slotu SLEEPING olarak ayır: zamanlayıcı yalnızca READY süreçleri
               seçer, böylece kayıtlar doldurulmadan bu sürece geçilemez.
               Aksi halde timer tam bu arada tetiklenirse iretq sıfırlanmış
               bir bağlamla çalışır ve #GP ile sistem çöker. */
            processes[i].state = PROC_STATE_SLEEPING;
            processes[i].alloc_count = 0;
            
            /* Her süreç için 16KB'lık özel bir Kernel Stack tahsis ediyoruz */
            void* new_stack = kmalloc(PROC_STACK_SIZE);
            if (!new_stack) {
                processes[i].state = PROC_STATE_DEAD; /* ayırmayı geri al */
                return 0;
            }

            processes[i].stack_base = (uint64_t)new_stack;
            /* Stack yukarıdan aşağı büyür; tepesi 16 bayt hizalı olmalı */
            processes[i].kernel_stack = ((uint64_t)new_stack + PROC_STACK_SIZE) & ~0xFULL;

            /* Başlangıç Kayıtlarını (Registers) Ayarla */
            memset(&processes[i].context, 0, sizeof(cpu_state_t));
            processes[i].context.rip = (uint64_t)entry_point;
            processes[i].context.rsp = processes[i].kernel_stack;
            processes[i].context.cs = kernel_cs;
            processes[i].context.ss = kernel_ss;
            processes[i].context.rflags = 0x202; /* Interrupts Enabled */

            /* Her şey hazır — ancak şimdi çalıştırılabilir yap */
            processes[i].state = PROC_STATE_READY;

            kprintf("[SCHEDULER] Created Process PID: %d\n", processes[i].pid);
            return processes[i].pid;
        }
    }
    return 0; /* Boş yer yok */
}

/* 
 * Bu fonksiyon Timer Interrupt (IRQ0) tarafından saniyede 100 kere çağrılır.
 * Şu an çalışan sürecin durumunu kaydeder ve sıradaki READY süreci çalıştırır.
 */
void process_schedule(cpu_state_t* current_context)
{
    if (current_process_index == -1) return;

    /* Şu anki süreci kaydet */
    memcpy(&processes[current_process_index].context, current_context, sizeof(cpu_state_t));
    if (processes[current_process_index].state == PROC_STATE_RUNNING) {
        processes[current_process_index].state = PROC_STATE_READY;
    }

    /* Sıradaki READY süreci bul (Round-Robin algoritması) */
    int next_index = (current_process_index + 1) % MAX_PROCESSES;
    int found = 0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[next_index].state == PROC_STATE_READY) {
            found = 1;
            break;
        }
        next_index = (next_index + 1) % MAX_PROCESSES;
    }

    if (!found) {
        next_index = 0; /* Eğer çalışacak başka bir şey yoksa Kernel'a dön */
    }

    /* Yeni sürece geç (Context Switch) */
    current_process_index = next_index;
    processes[current_process_index].state = PROC_STATE_RUNNING;

    /* Seçilen sürecin durumunu CPU'ya yükle */
    memcpy(current_context, &processes[current_process_index].context, sizeof(cpu_state_t));

    /* Ölmüş süreçlerin stack'lerini geri ver. Süreç kendi stack'i üzerindeyken
       serbest bırakılamaz; bu yüzden temizlik buraya, artık o stack'ten
       çıkıldıktan sonraya ertelenir. */
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (i != current_process_index &&
            processes[i].state == PROC_STATE_DEAD &&
            processes[i].stack_base != 0) {
            kfree((void *)processes[i].stack_base);
            processes[i].stack_base = 0;
            processes[i].kernel_stack = 0;
        }
    }
}

int process_count_alive(void)
{
    int n = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_DEAD) n++;
    }
    return n;
}

uint32_t process_get_current_pid(void)
{
    if (current_process_index == -1) return 0;
    return processes[current_process_index].pid;
}

void process_track_allocation(uint32_t pid, void* phys_addr)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != PROC_STATE_DEAD) {
            if (processes[i].alloc_count < MAX_ALLOCATIONS_PER_PROC) {
                processes[i].allocations[processes[i].alloc_count++] = phys_addr;
            }
            break;
        }
    }
}

void process_exit(uint32_t pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != PROC_STATE_DEAD) {
            /* Çöp Toplayıcı (Garbage Collector) Devrede!
               Bu programın aldığı tüm RAM'leri sisteme iade et */
            for (int j = 0; j < processes[i].alloc_count; j++) {
                pmm_free_page(processes[i].allocations[j]);
            }
            processes[i].alloc_count = 0;
            processes[i].state = PROC_STATE_DEAD;
            kprintf("[SCHEDULER] Process PID %d Exited. Memory FREED.\n", pid);
            
            /* Süreç kendini sonlandırdıysa scheduler'ın onu devre dışı
               bırakmasını bekle. Buraya syscall içinden (IF kapalı)
               gelinebildiği için önce kesmeleri açmak şart — aksi halde
               timer hiç gelmez ve `hlt` sonsuza kadar asılı kalır. */
            if (current_process_index == i) {
                while (1) __asm__ volatile("sti; hlt");
            }
            break;
        }
    }
}
