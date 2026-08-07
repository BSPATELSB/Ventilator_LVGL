#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    THEME_MODE_DARK = 0,        /* Dark Medical (Default) */
    THEME_MODE_LIGHT = 1,       /* Light Daytime Mode */
    THEME_MODE_NIGHT_VISION = 2 /* Night Vision (Red/Dark) */
} theme_mode_t;

typedef struct {
    lv_color_t bg;
    lv_color_t card_bg;
    lv_color_t card_border;
    lv_color_t panel_hdr;
    lv_color_t btn_bg;
    lv_color_t btn_active;
    lv_color_t item_hover;
    lv_color_t text_main;
    lv_color_t text_muted;
    lv_color_t accent_blue;
    lv_color_t accent_green;
    lv_color_t accent_yellow;
    lv_color_t accent_red;
} theme_palette_t;

/**
 * @brief Initialize theme manager and load saved theme configuration.
 */
void theme_manager_init(void);

/**
 * @brief Set current theme mode (Dark, Light, or Night Vision).
 * @param mode Target theme mode.
 */
void theme_set_mode(theme_mode_t mode);

/**
 * @brief Get current active theme mode.
 */
theme_mode_t theme_get_mode(void);

/**
 * @brief Get current theme color palette structure pointer.
 */
const theme_palette_t * theme_get_palette(void);

/**
 * @brief Get human-readable name of current theme.
 */
const char * theme_get_mode_name(theme_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* THEME_MANAGER_H */
