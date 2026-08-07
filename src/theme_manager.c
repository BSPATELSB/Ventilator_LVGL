#include "theme_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THEME_CONFIG_FILE "/tmp/ventilator_theme.conf"

static theme_mode_t current_theme_mode = THEME_MODE_DARK;
static theme_palette_t current_palette;

static const theme_palette_t PALETTE_DARK = {
    .bg           = LV_COLOR_MAKE(0x04, 0x0B, 0x16),
    .card_bg      = LV_COLOR_MAKE(0x09, 0x18, 0x2B),
    .card_border  = LV_COLOR_MAKE(0x13, 0x2C, 0x4A),
    .panel_hdr    = LV_COLOR_MAKE(0x06, 0x15, 0x28),
    .btn_bg       = LV_COLOR_MAKE(0x09, 0x1D, 0x36),
    .btn_active   = LV_COLOR_MAKE(0x0A, 0x3B, 0x73),
    .item_hover   = LV_COLOR_MAKE(0x0E, 0x26, 0x45),
    .text_main    = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .text_muted   = LV_COLOR_MAKE(0x70, 0x97, 0xBA),
    .accent_blue  = LV_COLOR_MAKE(0x00, 0xA8, 0xFF),
    .accent_green = LV_COLOR_MAKE(0x00, 0xE6, 0x76),
    .accent_yellow= LV_COLOR_MAKE(0xFF, 0xD6, 0x00),
    .accent_red   = LV_COLOR_MAKE(0xD5, 0x00, 0x00)
};

static const theme_palette_t PALETTE_LIGHT = {
    .bg           = LV_COLOR_MAKE(0xEE, 0xF2, 0xF6),
    .card_bg      = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .card_border  = LV_COLOR_MAKE(0xCB, 0xD5, 0xE1),
    .panel_hdr    = LV_COLOR_MAKE(0xE2, 0xE8, 0xF0),
    .btn_bg       = LV_COLOR_MAKE(0xE2, 0xE8, 0xF0),
    .btn_active   = LV_COLOR_MAKE(0x00, 0x78, 0xD4),
    .item_hover   = LV_COLOR_MAKE(0xDC, 0xE6, 0xF1),
    .text_main    = LV_COLOR_MAKE(0x0F, 0x17, 0x2A),
    .text_muted   = LV_COLOR_MAKE(0x47, 0x55, 0x69),
    .accent_blue  = LV_COLOR_MAKE(0x00, 0x78, 0xD4),
    .accent_green = LV_COLOR_MAKE(0x05, 0x96, 0x69),
    .accent_yellow= LV_COLOR_MAKE(0xD9, 0x77, 0x06),
    .accent_red   = LV_COLOR_MAKE(0xDC, 0x26, 0x26)
};

static const theme_palette_t PALETTE_NIGHT_VISION = {
    .bg           = LV_COLOR_MAKE(0x08, 0x02, 0x02),
    .card_bg      = LV_COLOR_MAKE(0x16, 0x06, 0x06),
    .card_border  = LV_COLOR_MAKE(0x3D, 0x10, 0x10),
    .panel_hdr    = LV_COLOR_MAKE(0x22, 0x08, 0x08),
    .btn_bg       = LV_COLOR_MAKE(0x22, 0x0A, 0x0A),
    .btn_active   = LV_COLOR_MAKE(0x5E, 0x14, 0x14),
    .item_hover   = LV_COLOR_MAKE(0x33, 0x0E, 0x0E),
    .text_main    = LV_COLOR_MAKE(0xFF, 0x66, 0x66),
    .text_muted   = LV_COLOR_MAKE(0xB3, 0x4A, 0x4A),
    .accent_blue  = LV_COLOR_MAKE(0xFF, 0x33, 0x33),
    .accent_green = LV_COLOR_MAKE(0xFF, 0x88, 0x00),
    .accent_yellow= LV_COLOR_MAKE(0xFF, 0xBB, 0x00),
    .accent_red   = LV_COLOR_MAKE(0xFF, 0x00, 0x00)
};

static void update_current_palette(void)
{
    switch(current_theme_mode) {
        case THEME_MODE_LIGHT:
            current_palette = PALETTE_LIGHT;
            break;
        case THEME_MODE_NIGHT_VISION:
            current_palette = PALETTE_NIGHT_VISION;
            break;
        case THEME_MODE_DARK:
        default:
            current_palette = PALETTE_DARK;
            break;
    }
}

static void save_theme_config(void)
{
    FILE * fp = fopen(THEME_CONFIG_FILE, "w");
    if(fp) {
        fprintf(fp, "%d\n", (int)current_theme_mode);
        fclose(fp);
    }
}

static void load_theme_config(void)
{
    FILE * fp = fopen(THEME_CONFIG_FILE, "r");
    if(fp) {
        int mode = 0;
        if(fscanf(fp, "%d", &mode) == 1) {
            if(mode >= 0 && mode <= 2) {
                current_theme_mode = (theme_mode_t)mode;
            }
        }
        fclose(fp);
    }
}

void theme_manager_init(void)
{
    load_theme_config();
    update_current_palette();
}

void theme_set_mode(theme_mode_t mode)
{
    if(mode > THEME_MODE_NIGHT_VISION) mode = THEME_MODE_DARK;
    current_theme_mode = mode;
    update_current_palette();
    save_theme_config();
}

theme_mode_t theme_get_mode(void)
{
    return current_theme_mode;
}

const theme_palette_t * theme_get_palette(void)
{
    return &current_palette;
}

const char * theme_get_mode_name(theme_mode_t mode)
{
    switch(mode) {
        case THEME_MODE_LIGHT:
            return "Light Mode";
        case THEME_MODE_NIGHT_VISION:
            return "Night Vision";
        case THEME_MODE_DARK:
        default:
            return "Dark Medical";
    }
}
