/* =============================================================================
 * GravityOS — gsh (GravityOS Shell)
 * =============================================================================
 * Basit komut satırı arayüzü
 * Komutlar: help, clear, echo, uptime, gravity, reboot
 * ============================================================================= */

#include "shell.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../drivers/fbcon.h"
#include "../drivers/keyboard.h"
#include "../kernel/pmm.h"
#include "../kernel/process.h"
#include "../kernel/usermode.h"
#include "../drivers/fb.h"
#include "../drivers/ata.h"
#include "../drivers/timer.h"
#include "../cpu/ports.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS    16

/* ==========================================================================
 * Komut işleme fonksiyonları
 * ========================================================================== */

static void cmd_help(void)
{
    kprintf("\n");
    kprintf("  GravityOS Shell Commands:\n");
    kprintf("  ========================\n");
    kprintf("  help       - Show this help message\n");
    kprintf("  clear      - Clear the screen\n");
    kprintf("  echo <msg> - Print a message\n");
    kprintf("  fetch      - System summary with logo\n");
    kprintf("  usermode   - Run a test program in ring 3\n");
    kprintf("  disk       - Show ATA disk info and dump sector 0\n");
    kprintf("  mem        - Show physical memory usage\n");
    kprintf("  uptime     - Show system uptime\n");
    kprintf("  gravity    - Show GravityOS info\n");
    kprintf("  reboot     - Reboot the system\n");
    kprintf("  halt       - Halt the CPU\n");
    kprintf("\n");
}

static void cmd_clear(void)
{
    fbcon_clear();
}

static void cmd_mem(void)
{
    uint64_t total = pmm_get_total_memory();
    uint64_t freem = pmm_get_free_memory();

    kprintf("Physical memory: %lu MB total, %lu MB free, %lu KB used\n",
            total / (1024 * 1024), freem / (1024 * 1024),
            (total - freem) / 1024);
}

static void cmd_echo(const char *args)
{
    if (args) {
        kprintf("%s\n", args);
    } else {
        kprintf("\n");
    }
}

static void cmd_uptime(void)
{
    uint64_t ticks = timer_get_ticks();
    uint64_t seconds = ticks / 100; /* 100 Hz timer */
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;

    kprintf("Uptime: %lu hours, %lu minutes, %lu seconds\n",
            hours, minutes % 60, seconds % 60);
}

static void cmd_disk(void)
{
    if (!ata_present()) {
        kprintf("gsh: no ATA disk detected\n");
        return;
    }

    kprintf("Model:   %s\n", ata_model());
    kprintf("Sectors: %u (%u MB)\n", ata_sector_count(), ata_sector_count() / 2048);

    static uint8_t sector[ATA_SECTOR_SIZE];
    if (ata_read_sectors(0, 1, sector) != 0) {
        kprintf("gsh: read failed\n");
        return;
    }

    kprintf("LBA 0, first 64 bytes:\n");
    for (int row = 0; row < 4; row++) {
        kprintf("  %04x  ", row * 16);
        for (int i = 0; i < 16; i++) {
            kprintf("%02x ", sector[row * 16 + i]);
        }
        kprintf(" |");
        for (int i = 0; i < 16; i++) {
            char c = (char)sector[row * 16 + i];
            kprintf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        kprintf("|\n");
    }
}

static void cmd_usermode(void)
{
    kprintf("Starting ring 3 test program...\n");

    uint32_t pid = process_create(usermode_run_test);
    if (pid == 0) {
        kprintf("gsh: could not create user mode process\n");
        return;
    }

    /* Program ayrı bir süreç olarak çalışıyor; scheduler ona geçtiğinde
       çıktısı buraya düşecek. Birkaç tick bekleyip prompt'u geri veriyoruz. */
    sleep_ms(200);
}

/* CPU marka adını CPUID (0x80000002-0x80000004) ile oku */
static void cpu_brand(char *out, int size)
{
    uint32_t regs[4];
    uint32_t max_ext;

    __asm__ volatile("cpuid" : "=a"(max_ext), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                             : "a"(0x80000000));

    if (max_ext < 0x80000004 || size < 49) {
        strcpy(out, "Unknown x86_64 CPU");
        return;
    }

    int pos = 0;
    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        __asm__ volatile("cpuid" : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                                 : "a"(leaf));
        for (int r = 0; r < 4; r++) {
            for (int b = 0; b < 4; b++) {
                out[pos++] = (char)((regs[r] >> (b * 8)) & 0xFF);
            }
        }
    }
    out[pos] = '\0';

    /* Baştaki boşlukları kırp */
    int start = 0;
    while (out[start] == ' ') start++;
    if (start) {
        int i = 0;
        while (out[start + i]) { out[i] = out[start + i]; i++; }
        out[i] = '\0';
    }
}

/* fastfetch tarzı sistem özeti */
static void cmd_fetch(void)
{
    static const char *logo[] = {
        "   ______                 ",
        "  / ____/_______ __   __  ",
        " / / __/ ___/ _ \\\\ \\ / /  ",
        "/ /_/ / /  /  __/ \\ V /   ",
        "\\____/_/   \\___/  \\_/     ",
        "                          ",
        "                          ",
        "                          ",
        "                          ",
        "                          ",
    };

    char brand[64];
    cpu_brand(brand, sizeof(brand));

    uint64_t seconds = timer_get_ticks() / 100;
    uint64_t total   = pmm_get_total_memory() / (1024 * 1024);
    uint64_t used_kb = (pmm_get_total_memory() - pmm_get_free_memory()) / 1024;

    char info[10][80];
    snprintf(info[0], sizeof(info[0]), "root@gravityos");
    snprintf(info[1], sizeof(info[1]), "--------------");
    snprintf(info[2], sizeof(info[2]), "OS:         GravityOS v0.1.0 x86_64");
    snprintf(info[3], sizeof(info[3]), "Kernel:     gravity-monolithic (higher-half)");
    snprintf(info[4], sizeof(info[4]), "Bootloader: Limine");
    snprintf(info[5], sizeof(info[5]), "Uptime:     %lu min %lu sec", seconds / 60, seconds % 60);
    snprintf(info[6], sizeof(info[6]), "Shell:      gsh");
    snprintf(info[7], sizeof(info[7]), "Resolution: %ux%u", fb_get_width(), fb_get_height());
    snprintf(info[8], sizeof(info[8]), "CPU:        %s", brand);
    snprintf(info[9], sizeof(info[9]), "Memory:     %lu KB / %lu MB   (%d process)",
             used_kb, total, process_count_alive());

    kprintf("\n");
    for (int i = 0; i < 10; i++) {
        fbcon_set_color(FBC_CYAN);
        kprintf("%s", logo[i]);
        fbcon_set_color(i < 2 ? FBC_GREEN : FBC_GREY);
        kprintf("  %s\n", info[i]);
    }
    fbcon_set_color(FBC_GREY);
    kprintf("\n");
}

static void cmd_gravity(void)
{
    fbcon_set_color(FBC_CYAN);
    kprintf("\n");
    kprintf("   ______                 _ __        ____  _____\n");
    kprintf("  / ____/_______ __   __ (_) /___  __/ __ \\/ ___/\n");
    kprintf(" / / __/ ___/ _ \\\\ \\ / // / __/ / / / / / /\\__ \\ \n");
    kprintf("/ /_/ / /  /  __/ \\ V // / /_/ /_/ / /_/ /___/ / \n");
    kprintf("\\____/_/   \\___/  \\_//_/\\__/\\__, /\\____//____/  \n");
    kprintf("                           /____/                \n");
    fbcon_set_color(FBC_YELLOW);
    kprintf("\n  GravityOS v0.1.0 — A 64-bit Operating System\n");
    kprintf("  Architecture: x86_64 (Long Mode)\n");
    kprintf("  Built with love, Assembly & C\n\n");
    fbcon_set_color(FBC_GREY);
}

static void cmd_reboot(void)
{
    kprintf("Rebooting...\n");
    /* Keyboard controller ile yeniden başlat */
    uint8_t status;
    do {
        status = inb(0x64);
        if (status & 1) {
            inb(0x60); /* Bekleyen byte'ı oku */
        }
    } while (status & 2);
    outb(0x64, 0xFE); /* Reset komutu */

    /* Başarısız olursa triple fault */
    __asm__ volatile("cli");
    __asm__ volatile("lidt (%%rax)" : : "a"(0));
    __asm__ volatile("int $3");
}

static void cmd_halt(void)
{
    kprintf("System halted.\n");
    __asm__ volatile("cli; hlt");
}

/* ==========================================================================
 * readline — Satır oku (echo + backspace desteği)
 * ========================================================================== */
static int readline(char *buf, int max_len)
{
    int pos = 0;
    while (pos < max_len - 1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            buf[pos] = '\0';
            kputchar('\n');
            return pos;
        }

        if (c == '\b') {
            if (pos > 0) {
                pos--;
                kputchar('\b');
            }
            continue;
        }

        /* Tab'ı yoksay */
        if (c == '\t') continue;

        /* Yazdırılabilir karakter */
        if (c >= 32 && c < 127) {
            buf[pos++] = c;
            kputchar(c);
        }
    }

    buf[pos] = '\0';
    return pos;
}

/* ==========================================================================
 * process_command — Komutu ayrıştır ve çalıştır
 * ========================================================================== */
static void process_command(char *cmd_line)
{
    /* Baştaki boşlukları atla */
    while (*cmd_line == ' ') cmd_line++;

    /* Boş komut */
    if (*cmd_line == '\0') return;

    /* Komutu ve argümanları ayır */
    char *cmd = cmd_line;
    char *args = NULL;

    /* İlk boşlukta böl */
    char *space = strchr(cmd_line, ' ');
    if (space) {
        *space = '\0';
        args = space + 1;
        /* Argümandaki baştaki boşlukları atla */
        while (*args == ' ') args++;
        if (*args == '\0') args = NULL;
    }

    /* Komut eşleştirme */
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        cmd_clear();
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(cmd, "disk") == 0) {
        cmd_disk();
    } else if (strcmp(cmd, "usermode") == 0 || strcmp(cmd, "ring3") == 0) {
        cmd_usermode();
    } else if (strcmp(cmd, "fetch") == 0 || strcmp(cmd, "neofetch") == 0) {
        cmd_fetch();
    } else if (strcmp(cmd, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(cmd, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(cmd, "gravity") == 0) {
        cmd_gravity();
    } else if (strcmp(cmd, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(cmd, "halt") == 0) {
        cmd_halt();
    } else {
        fbcon_set_color(FBC_RED);
        kprintf("gsh: command not found: %s\n", cmd);
        fbcon_set_color(FBC_GREY);
    }
}

/* ==========================================================================
 * shell_run — Shell ana döngüsü
 * ========================================================================== */
void shell_run(void)
{
    char cmd_buf[MAX_CMD_LEN];

    /* Hoş geldin mesajı */
    kprintf("\n");
    fbcon_set_color(FBC_GREEN);
    kprintf("Welcome to GravityOS!\n");
    fbcon_set_color(FBC_GREY);
    kprintf("Type 'help' for available commands.\n\n");

    /* Ana döngü */
    while (1) {
        /* Prompt */
        fbcon_set_color(FBC_GREEN);
        kprintf("gravity");
        fbcon_set_color(FBC_CYAN);
        kprintf("@");
        fbcon_set_color(FBC_BLUE);
        kprintf("os");
        fbcon_set_color(FBC_GREY);
        kprintf("$ ");

        /* Komut oku */
        readline(cmd_buf, sizeof(cmd_buf));

        /* Komutu çalıştır */
        process_command(cmd_buf);
    }
}
