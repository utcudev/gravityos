/* =============================================================================
 * GravityOS — PS/2 Mouse Driver
 * =============================================================================
 * IRQ 12 üzerinden PS/2 Fare okuması yapar.
 * Mouse koordinatlarını günceller ve tıklamaları algılar.
 * ============================================================================= */

#include "mouse.h"
#include "../cpu/ports.h"
#include "../cpu/idt.h"
#include "../drivers/fb.h"
#include "../lib/stdio.h"

int32_t mouse_x = 512;
int32_t mouse_y = 384;
uint8_t mouse_left_pressed = 0;
uint8_t mouse_right_pressed = 0;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

/* İmlecin altında kalan pikseller — imleç taşınırken geri yüklenir */
#define CURSOR_SIZE 5
static uint32_t mouse_saved_pixels[CURSOR_SIZE][CURSOR_SIZE];
static int  mouse_saved_valid = 0;
static int32_t old_mouse_x = 512;
static int32_t old_mouse_y = 384;

static inline void mouse_wait(uint8_t a_type)
{
    uint32_t _time_out = 100000;
    if (a_type == 0) {
        while (_time_out--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (_time_out--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static inline void mouse_write(uint8_t a_write)
{
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, a_write);
}

static uint8_t mouse_read(void)
{
    mouse_wait(0);
    return inb(0x60);
}

/* Basit Mouse İmleci (5x5 kırmızı artı).
   Çizmeden önce altındaki pikseller saklanır, imleç taşınınca geri konur —
   aksi halde imlecin geçtiği her yere kalıcı bir kare iz kalır. */
static void draw_mouse_cursor(int32_t x, int32_t y)
{
    /* Eski konumdaki içeriği geri yükle */
    if (mouse_saved_valid) {
        for (int i = 0; i < CURSOR_SIZE; i++) {
            for (int j = 0; j < CURSOR_SIZE; j++) {
                fb_putpixel(old_mouse_x + j, old_mouse_y + i, mouse_saved_pixels[i][j]);
            }
        }
    }

    /* Yeni konumun altındaki pikselleri sakla */
    for (int i = 0; i < CURSOR_SIZE; i++) {
        for (int j = 0; j < CURSOR_SIZE; j++) {
            mouse_saved_pixels[i][j] = fb_getpixel(x + j, y + i);
        }
    }
    mouse_saved_valid = 1;

    /* İmleci çiz */
    fb_draw_rect(x, y + 2, CURSOR_SIZE, 1, FB_COLOR(255, 0, 0));
    fb_draw_rect(x + 2, y, 1, CURSOR_SIZE, FB_COLOR(255, 0, 0));

    old_mouse_x = x;
    old_mouse_y = y;
}

static void mouse_irq_handler(cpu_state_t *regs)
{
    (void)regs;

    uint8_t status = inb(0x64);

    /* Bit 0: okunacak veri var mı, Bit 5: veri FARE'den mi geliyor.
       Klavye baytı ise DOKUNMA — okursak scancode kaybolur ve tuşlar yutulur.
       O baytı IRQ 1 handler'ı alacak. */
    if ((status & 0x21) != 0x21) return;

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
    case 0:
        /* İlk bayt daima 0x08 bitini taşır; taşımıyorsa senkron bozulmuştur */
        if (!(data & 0x08)) return;
        mouse_byte[0] = (int8_t)data;
        mouse_cycle = 1;
        return;

    case 1:
        mouse_byte[1] = (int8_t)data;
        mouse_cycle = 2;
        return;

    case 2:
        mouse_byte[2] = (int8_t)data;
        mouse_cycle = 0;
        break;

    default:
        mouse_cycle = 0;
        return;
    }

    /* X/Y taşma bitleri set ise paket güvenilmez — imleci fırlatmamak için at */
    if (mouse_byte[0] & 0xC0) return;

    mouse_left_pressed  = mouse_byte[0] & 0x01;
    mouse_right_pressed = mouse_byte[0] & 0x02;

    /* Hareket baytları 9-bit işaretli: işaret biti byte[0]'da taşınır */
    int32_t rel_x = (int32_t)(uint8_t)mouse_byte[1] - ((mouse_byte[0] & 0x10) ? 256 : 0);
    int32_t rel_y = (int32_t)(uint8_t)mouse_byte[2] - ((mouse_byte[0] & 0x20) ? 256 : 0);

    if (rel_x == 0 && rel_y == 0) return;

    /* PS/2 farede Y ekseni yukarı doğrudur, ekranımızda aşağı doğru */
    mouse_x += rel_x;
    mouse_y -= rel_y;

    /* Sınırları kontrol et (imleç tamamen ekran içinde kalsın) */
    int32_t max_x = (int32_t)fb_get_width()  - CURSOR_SIZE;
    int32_t max_y = (int32_t)fb_get_height() - CURSOR_SIZE;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x > max_x) mouse_x = max_x;
    if (mouse_y > max_y) mouse_y = max_y;

    draw_mouse_cursor(mouse_x, mouse_y);
}

void mouse_init(void)
{
    uint8_t status;

    /* Fare portunu aktifleştir */
    mouse_wait(1);
    outb(0x64, 0xA8);

    /* Compaq durum byte'ını al ve IRQ12'yi aktifleştir */
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);

    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    /* Default ayarları kullan ve Veri Raporlamayı Aç */
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();

    /* IRQ 12'yi kaydet */
    irq_install_handler(12, mouse_irq_handler);

    /* İmleci ilk kez ekranın ortasına çiz (fare hareket etmeden de görünsün) */
    mouse_x = (int32_t)fb_get_width() / 2;
    mouse_y = (int32_t)fb_get_height() / 2;
    draw_mouse_cursor(mouse_x, mouse_y);
}
