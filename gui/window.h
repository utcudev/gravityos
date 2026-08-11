/* =============================================================================
 * GravityOS — Window Manager Header
 * ============================================================================= */

#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H

#include <stdint.h>

/* Görev çubuğu yüksekliği (piksel) */
#define GUI_TASKBAR_H 40
/* Pencere başlık çubuğu yüksekliği (piksel) */
#define GUI_TITLEBAR_H 22

typedef struct {
    int x;
    int y;
    int width;
    int height;
    const char* title;
    uint32_t bg_color;
    uint32_t fg_color;
} gui_window_t;

void gui_init(void);
void gui_draw_window(gui_window_t* win);
void gui_draw_desktop(void);

#endif /* GUI_WINDOW_H */
