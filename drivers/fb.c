/* =============================================================================
 * GravityOS — Framebuffer Driver (Custom Bootloader)
 * ============================================================================= */

#include "fb.h"
#include "../kernel/vmm.h"
#include "../kernel/pmm.h"

static uint32_t *fb_buffer = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t  fb_bpp = 0;

void fb_init(boot_info_t *info)
{
    if (!info) return;

    fb_buffer = (uint32_t *)(uintptr_t)info->fb_addr;
    fb_width  = info->fb_width;
    fb_height = info->fb_height;
    fb_pitch  = info->fb_pitch;
    fb_bpp    = info->fb_bpp;
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_width || y >= fb_height || !fb_buffer) return;

    /* Pitch is usually in bytes */
    uint32_t offset = (y * fb_pitch / 4) + x;
    fb_buffer[offset] = color;
}

uint32_t fb_getpixel(uint32_t x, uint32_t y)
{
    if (x >= fb_width || y >= fb_height || !fb_buffer) return 0;

    uint32_t offset = (y * fb_pitch / 4) + x;
    return fb_buffer[offset];
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            fb_putpixel(x + j, y + i, color);
        }
    }
}

void fb_clear(uint32_t color)
{
    fb_draw_rect(0, 0, fb_width, fb_height, color);
}

uint32_t fb_get_width(void)  { return fb_width; }
uint32_t fb_get_height(void) { return fb_height; }
