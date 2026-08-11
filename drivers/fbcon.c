/* =============================================================================
 * GravityOS — Framebuffer Console (fbcon)
 * ============================================================================= */

#include "fbcon.h"
#include "font.h"

/* Sabit tavan: heap'e bağımlı olmasın diye statik grid.
   1024x768'lik alanda 128x76 hücre yeter. */
#define FBCON_MAX_COLS 160
#define FBCON_MAX_ROWS 96

static uint32_t con_x = 0, con_y = 0;      /* Konsol alanının sol üst köşesi */
static uint32_t con_cols = 0, con_rows = 0;
static uint32_t con_bg = 0;
static uint32_t con_fg = FBC_GREY;

static uint32_t cur_col = 0, cur_row = 0;
static int con_ready = 0;

/* Ekran içeriği — kaydırma sırasında yeniden çizmek için tutulur */
static char     cell_ch[FBCON_MAX_ROWS][FBCON_MAX_COLS];
static uint32_t cell_fg[FBCON_MAX_ROWS][FBCON_MAX_COLS];

int fbcon_ready(void) { return con_ready; }

static void draw_cell(uint32_t row, uint32_t col)
{
    uint32_t px = con_x + col * FBCON_CELL_W;
    uint32_t py = con_y + row * FBCON_CELL_H;
    char c = cell_ch[row][col];

    /* Hücrenin tamamını (satır arası dahil) arkaplanla temizle */
    fb_draw_rect(px, py, FBCON_CELL_W, FBCON_CELL_H, con_bg);

    if (c != 0 && c != ' ') {
        font_draw_char(px, py, c, cell_fg[row][col], con_bg);
    }
}

static void redraw_all(void)
{
    for (uint32_t r = 0; r < con_rows; r++) {
        for (uint32_t c = 0; c < con_cols; c++) {
            draw_cell(r, c);
        }
    }
}

void fbcon_init(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_color)
{
    con_x = x;
    con_y = y;
    con_bg = bg_color;

    con_cols = w / FBCON_CELL_W;
    con_rows = h / FBCON_CELL_H;
    if (con_cols > FBCON_MAX_COLS) con_cols = FBCON_MAX_COLS;
    if (con_rows > FBCON_MAX_ROWS) con_rows = FBCON_MAX_ROWS;

    con_ready = 1;
    fbcon_clear();
}

void fbcon_set_color(uint32_t fg_color)
{
    con_fg = fg_color;
}

void fbcon_clear(void)
{
    if (!con_ready) return;

    for (uint32_t r = 0; r < con_rows; r++) {
        for (uint32_t c = 0; c < con_cols; c++) {
            cell_ch[r][c] = ' ';
            cell_fg[r][c] = con_fg;
        }
    }
    cur_col = 0;
    cur_row = 0;

    fb_draw_rect(con_x, con_y, con_cols * FBCON_CELL_W, con_rows * FBCON_CELL_H, con_bg);
}

/* Bir satır yukarı kaydır */
static void scroll_up(void)
{
    for (uint32_t r = 0; r + 1 < con_rows; r++) {
        for (uint32_t c = 0; c < con_cols; c++) {
            cell_ch[r][c] = cell_ch[r + 1][c];
            cell_fg[r][c] = cell_fg[r + 1][c];
        }
    }
    for (uint32_t c = 0; c < con_cols; c++) {
        cell_ch[con_rows - 1][c] = ' ';
        cell_fg[con_rows - 1][c] = con_fg;
    }

    cur_row = con_rows - 1;
    redraw_all();
}

static void newline(void)
{
    cur_col = 0;
    if (cur_row + 1 < con_rows) {
        cur_row++;
    } else {
        scroll_up();
    }
}

void fbcon_putchar(char c)
{
    if (!con_ready) return;

    if (c == '\n') {
        newline();
        return;
    }

    if (c == '\r') {
        cur_col = 0;
        return;
    }

    if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
        } else if (cur_row > 0) {
            cur_row--;
            cur_col = con_cols - 1;
        }
        cell_ch[cur_row][cur_col] = ' ';
        draw_cell(cur_row, cur_col);
        return;
    }

    if (c == '\t') {
        for (int i = 0; i < 4; i++) fbcon_putchar(' ');
        return;
    }

    /* Yazdırılamayan karakterleri atla */
    if (c < 32 || (uint8_t)c > 126) return;

    if (cur_col >= con_cols) newline();

    cell_ch[cur_row][cur_col] = c;
    cell_fg[cur_row][cur_col] = con_fg;
    draw_cell(cur_row, cur_col);
    cur_col++;
}
