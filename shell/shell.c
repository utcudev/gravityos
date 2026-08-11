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
