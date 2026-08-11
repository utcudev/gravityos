/* =============================================================================
 * GravityOS — Window Manager Implementation
 * ============================================================================= */

#include "window.h"
#include "../drivers/fb.h"
#include "../drivers/font.h"
#include "../lib/string.h"

void gui_draw_desktop(void)
{
    uint32_t w = fb_get_width();
    uint32_t h = fb_get_height();

    /* Arkaplanı modern bir koyu mavi-gri renk ile doldur */
    fb_draw_rect(0, 0, w, h, FB_COLOR(40, 44, 52));

    /* Alt tarafa bir görev çubuğu çiz (Taskbar) */
    fb_draw_rect(0, h - GUI_TASKBAR_H, w, GUI_TASKBAR_H, FB_COLOR(33, 37, 43));

    /* Görev çubuğu üzerine basit bir menü ikonu/yazısı */
    font_draw_string(10, h - 25, " [ GRAVITY OS ] ",
                     FB_COLOR(255, 255, 255), FB_COLOR(33, 37, 43));
}

void gui_draw_window(gui_window_t* win)
{
    /* Pencere gövdesini çiz */
    fb_draw_rect(win->x, win->y, win->width, win->height, win->bg_color);
    
    /* Pencere kenarlığı */
    for (int x = win->x; x < win->x + win->width; x++) {
        fb_draw_rect(x, win->y, 1, 1, FB_COLOR(255, 255, 255)); /* Üst Çizgi */
        fb_draw_rect(x, win->y + win->height, 1, 1, FB_COLOR(0, 0, 0)); /* Alt Çizgi */
    }
    for (int y = win->y; y < win->y + win->height; y++) {
        fb_draw_rect(win->x, y, 1, 1, FB_COLOR(255, 255, 255)); /* Sol Çizgi */
        fb_draw_rect(win->x + win->width, y, 1, 1, FB_COLOR(0, 0, 0)); /* Sağ Çizgi */
    }
    
    /* Başlık Çubuğu (Title Bar) */
    fb_draw_rect(win->x + 2, win->y + 2, win->width - 4, GUI_TITLEBAR_H - 2,
                 FB_COLOR(0, 120, 215));

    /* Başlık Yazısı */
    font_draw_string(win->x + 10, win->y + 9, win->title,
                     FB_COLOR(255, 255, 255), FB_COLOR(0, 120, 215));
}

void gui_init(void)
{
    gui_draw_desktop();
}
