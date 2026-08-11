/* =============================================================================
 * GravityOS — Kernel Entry Point (Native Boot)
 * ============================================================================= */

#include "kernel.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include <stdbool.h>
#include "../drivers/fb.h"
#include "../drivers/font.h"
#include "../drivers/fbcon.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/pci.h"
#include "../gui/window.h"
#include "../cpu/idt.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "process.h"
#include "limine.h"
#include "../shell/shell.h"

extern uint64_t __bss_start;
extern uint64_t __bss_end;

__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(1);

__attribute__((used, section(".requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

uint64_t hhdm_offset = 0xFFFF800000000000;

static boot_info_t limine_boot_info = {0};

/* ==========================================================================
 * clock_process — Görev çubuğunda çalışma süresini gösteren ikinci süreç.
 * Shell ile aynı anda çalışır; scheduler'ın gerçekten geçiş yaptığının
 * gözle görülür kanıtı budur.
 * ========================================================================== */
static void clock_process(void)
{
    char buf[32];
    uint64_t last_shown = (uint64_t)-1;

    for (;;) {
        uint64_t seconds = timer_get_ticks() / 100;

        if (seconds != last_shown) {
            last_shown = seconds;
            snprintf(buf, sizeof(buf), "up %lu:%02lu:%02lu",
                     seconds / 3600, (seconds / 60) % 60, seconds % 60);

            uint32_t x = fb_get_width() - 140;
            uint32_t y = fb_get_height() - 25;
            fb_draw_rect(x, y, 130, 10, FB_COLOR(33, 37, 43));
            font_draw_string(x, y, buf, FB_COLOR(200, 200, 200), FB_COLOR(33, 37, 43));
        }

        __asm__ volatile("hlt"); /* Bir sonraki tick'e kadar CPU'yu bırak */
    }
}

void kmain(void)
{
    serial_init();

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        kpanic("Limine base revision not supported!");
    }

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        kpanic("Limine did not provide a framebuffer!");
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    limine_boot_info.magic = BOOT_MAGIC;
    limine_boot_info.fb_addr = (uint64_t)fb->address;
    limine_boot_info.fb_width = fb->width;
    limine_boot_info.fb_height = fb->height;
    limine_boot_info.fb_pitch = fb->pitch;
    limine_boot_info.fb_bpp = fb->bpp;

    uint64_t total_memory = 0;
    if (memmap_request.response != NULL) {
        for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
            total_memory += memmap_request.response->entries[i]->length;
        }
    } else {
        total_memory = 128 * 1024 * 1024; /* Fallback */
    }
    limine_boot_info.memory_size = total_memory;

    if (hhdm_request.response != NULL) {
        hhdm_offset = hhdm_request.response->offset;
    }

    /* Hafıza Yönetimi Başlatma (PMM, VMM & HEAP) */
    if (memmap_request.response != NULL) {
        pmm_init(memmap_request.response);
    } else {
        kpanic("Limine did not provide a memory map!");
    }
    vmm_init();
    heap_init();

    /* Kesmeleri kur (Page Fault gibi exception'ları baştan yakalayabilelim) */
    isr_init();
    process_init(); /* Scheduler'ı hazırla — timer IRQ'su buna bağlı */

    /* VESA Framebuffer ve masaüstü */
    fb_init(&limine_boot_info);
    gui_init();

    /* Terminal penceresini çiz ve konsolu içine yerleştir */
    uint32_t scr_w = fb_get_width();
    uint32_t scr_h = fb_get_height();

    gui_window_t term;
    term.x = 40;
    term.y = 40;
    term.width  = (int)scr_w - 80;
    term.height = (int)scr_h - GUI_TASKBAR_H - 80;
    term.title = "gsh - GravityOS Shell";
    term.bg_color = FB_COLOR(24, 26, 30);
    term.fg_color = FB_COLOR(200, 200, 200);
    gui_draw_window(&term);

    fbcon_init(term.x + 8,
               term.y + GUI_TITLEBAR_H + 6,
               term.width - 16,
               term.height - GUI_TITLEBAR_H - 14,
               term.bg_color);

    /* Sürücüler — konsol hazır olduğu için mesajları ekranda görünür */
    timer_init(100);
    keyboard_init();
    mouse_init();
    pci_init();

    /* Saat sürecini başlat: scheduler gerçekten iki süreç arasında geçiş
       yaptığını görev çubuğundaki sayaç ile kanıtlar */
    process_create(clock_process);

    /* Kernel'in kendisi (process 0) shell olarak devam eder */
    shell_run();

    /* shell_run normalde dönmez */
    while (1) {
        __asm__ volatile("hlt");
    }
}

void kpanic(const char *fmt, ...)
{
    __asm__ volatile("cli");
    while (1) {
        __asm__ volatile("hlt");
    }
}
