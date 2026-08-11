/* =============================================================================
 * GravityOS — PS/2 Keyboard Driver
 * =============================================================================
 * Scancode Set 1 → ASCII çevirisi
 * Shift, Caps Lock, Ctrl desteği
 * Circular input buffer
 * ============================================================================= */

#include "keyboard.h"
#include "mouse.h"
#include "../cpu/idt.h"
#include "../cpu/ports.h"

/* Klavye durumu */
static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int caps_lock = 0;

/* Circular buffer */
static char kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_buffer_head = 0;
static volatile int kb_buffer_tail = 0;

/* US QWERTY Scancode → ASCII tablosu (Scancode Set 1) */
static const char scancode_to_ascii[] = {
     0,   0,  '1', '2', '3', '4', '5', '6',   /* 0x00 - 0x07 */
    '7', '8', '9', '0', '-', '=', '\b', '\t',  /* 0x08 - 0x0F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   /* 0x10 - 0x17 */
    'o', 'p', '[', ']', '\n',  0,  'a', 's',   /* 0x18 - 0x1F */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 0x20 - 0x27 */
   '\'', '`',  0, '\\', 'z', 'x', 'c', 'v',   /* 0x28 - 0x2F */
    'b', 'n', 'm', ',', '.', '/',  0,  '*',    /* 0x30 - 0x37 */
     0,  ' ',  0,   0,   0,   0,   0,   0,     /* 0x38 - 0x3F */
     0,   0,   0,   0,   0,   0,   0,  '7',    /* 0x40 - 0x47 */
    '8', '9', '-', '4', '5', '6', '+', '1',    /* 0x48 - 0x4F */
    '2', '3', '0', '.',  0,   0,   0,   0,     /* 0x50 - 0x57 */
};

/* Shift basılıyken karakterler */
static const char scancode_to_ascii_shift[] = {
     0,   0,  '!', '@', '#', '$', '%', '^',    /* 0x00 - 0x07 */
    '&', '*', '(', ')', '_', '+', '\b', '\t',  /* 0x08 - 0x0F */
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',   /* 0x10 - 0x17 */
    'O', 'P', '{', '}', '\n',  0,  'A', 'S',   /* 0x18 - 0x1F */
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   /* 0x20 - 0x27 */
    '"', '~',  0,  '|', 'Z', 'X', 'C', 'V',   /* 0x28 - 0x2F */
    'B', 'N', 'M', '<', '>', '?',  0,  '*',    /* 0x30 - 0x37 */
     0,  ' ',  0,   0,   0,   0,   0,   0,     /* 0x38 - 0x3F */
};

/* ==========================================================================
 * buffer_put — Buffer'a karakter ekle
 * ========================================================================== */
static void buffer_put(char c)
{
    int next = (kb_buffer_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_buffer_tail) { /* Buffer dolu değilse */
        kb_buffer[kb_buffer_head] = c;
        kb_buffer_head = next;
    }
}

/* ==========================================================================
 * keyboard_handle_byte — Tek bir scancode'u işle
 * =========================================================================
 * PS/2 denetleyicisinde klavye ve fare TEK bir çıkış tamponunu paylaşır.
 * Baytı hangi kesmenin okuduğu değil, kime ait olduğu önemlidir; bu yüzden
 * çözümleme buraya ayrıldı. Hem IRQ 1 hem IRQ 12 buraya yönlendirebiliyor.
 * ========================================================================== */
void keyboard_handle_byte(uint8_t scancode)
{
    /* Key release (bit 7 set) */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) { /* Left/Right Shift */
            shift_pressed = 0;
        } else if (released == 0x1D) { /* Ctrl */
            ctrl_pressed = 0;
        }
        return;
    }

    /* Özel tuşlar */
    switch (scancode) {
    case 0x2A: /* Left Shift */
    case 0x36: /* Right Shift */
        shift_pressed = 1;
        return;

    case 0x1D: /* Left Ctrl */
        ctrl_pressed = 1;
        return;

    case 0x3A: /* Caps Lock toggle */
        caps_lock = !caps_lock;
        return;

    case 0x38: /* Alt — şimdilik yoksay */
        return;
    }

    /* Scancode'u ASCII'ye çevir */
    if (scancode >= sizeof(scancode_to_ascii)) return;

    char c;
    if (shift_pressed) {
        if (scancode < sizeof(scancode_to_ascii_shift)) {
            c = scancode_to_ascii_shift[scancode];
        } else {
            c = 0;
        }
    } else {
        c = scancode_to_ascii[scancode];
    }

    if (c == 0) return;

    /* Caps Lock — sadece harfleri etkiler */
    if (caps_lock && !shift_pressed) {
        if (c >= 'a' && c <= 'z') c -= 32;
    } else if (caps_lock && shift_pressed) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }

    /* Ctrl+C, Ctrl+L vb. */
    if (ctrl_pressed) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 1; /* Ctrl+A = 1, Ctrl+C = 3, vb. */
        }
    }

    buffer_put(c);
}

/* ==========================================================================
 * keyboard_init — Klavye sürücüsünü başlat
 * ========================================================================== */
/* ==========================================================================
 * keyboard_irq_handler — IRQ 1 (klavye kesmesi)
 * =========================================================================
 * Tamponu tamamen boşaltır. Fareye ait baytları da okuyup fare sürücüsüne
 * verir: bir bayt okunmadan tamponun başında beklerse arkasındaki klavye
 * baytları asla gelmez ve klavye kalıcı olarak ölür.
 * ========================================================================== */
static void keyboard_irq_handler(cpu_state_t *regs)
{
    (void)regs;

    for (int guard = 0; guard < 32; guard++) {
        uint8_t status = inb(KEYBOARD_STATUS_PORT);
        if ((status & 0x01) == 0) return;   /* tampon boş */

        uint8_t data = inb(KEYBOARD_DATA_PORT);

        if (status & 0x20) {
            mouse_handle_byte(data);        /* fareye ait */
        } else {
            keyboard_handle_byte(data);
        }
    }
}

void keyboard_init(void)
{
    /* Açılışta tamponda bekleyen artık baytları temizle */
    for (int i = 0; i < 32; i++) {
        if (!(inb(KEYBOARD_STATUS_PORT) & 0x01)) break;
        inb(KEYBOARD_DATA_PORT);
    }

    /* IRQ 1 handler'ını kaydet */
    irq_install_handler(1, keyboard_irq_handler);
}

/* ==========================================================================
 * keyboard_has_input — Buffer'da karakter var mı?
 * ========================================================================== */
int keyboard_has_input(void)
{
    return kb_buffer_head != kb_buffer_tail;
}

/* ==========================================================================
 * keyboard_getchar — Bir karakter oku (bloklayıcı)
 * ========================================================================== */
char keyboard_getchar(void)
{
    /* Karakter gelene kadar bekle */
    while (!keyboard_has_input()) {
        __asm__ volatile("hlt"); /* CPU'yu uyut, interrupt bekle */
    }

    char c = kb_buffer[kb_buffer_tail];
    kb_buffer_tail = (kb_buffer_tail + 1) % KB_BUFFER_SIZE;
    return c;
}
