#include "custom_keyboard.h"
#include <stdio.h>
#include <string.h>

/* Aesthetic Medical UI Colors */
#define COLOR_KB_BG           lv_color_hex(0x061528)
#define COLOR_KB_BORDER       lv_color_hex(0x133D6B)
#define COLOR_KB_KEY_BG       lv_color_hex(0x0A203B)
#define COLOR_KB_KEY_BORDER   lv_color_hex(0x153A66)
#define COLOR_KB_KEY_TEXT     lv_color_hex(0xFFFFFF)
#define COLOR_KB_KEY_PRESSED  lv_color_hex(0x007CFF)
#define COLOR_KB_ACCENT_BLUE  lv_color_hex(0x00A8FF)
#define COLOR_KB_ACCENT_GREEN lv_color_hex(0x00E676)
#define COLOR_KB_TEXT_MUTED   lv_color_hex(0x7097BA)

/* Keyboard Data structure attached to user_data */
typedef struct {
    lv_obj_t * container;
    lv_obj_t * header_title;
    lv_obj_t * btn_mode_abc;
    lv_obj_t * btn_mode_123;
    lv_obj_t * btn_mode_sym;
    lv_obj_t * btnmatrix;
    lv_obj_t * target_ta;
    custom_kb_mode_t mode;
    bool temp_upper;
    custom_kb_done_cb_t done_cb;
} custom_keyboard_t;

/* Layout 0: Lowercase */
static const char * kb_map_lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    LV_SYMBOL_UP, "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "?123", "#+=", " ", ".", LV_SYMBOL_DOWN, LV_SYMBOL_OK, ""
};

static const lv_buttonmatrix_ctrl_t kb_ctrl_lower[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 2, 6, 1, 2, 3
};

/* Layout 1: Uppercase */
static const char * kb_map_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    LV_SYMBOL_UP, "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "?123", "#+=", " ", ".", LV_SYMBOL_DOWN, LV_SYMBOL_OK, ""
};

static const lv_buttonmatrix_ctrl_t kb_ctrl_upper[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 2, 6, 1, 2, 3
};

/* Layout 2: Numbers */
static const char * kb_map_numbers[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "/", ":", ";", "(", ")", "$", "&", "@", "\"", "\n",
    "#+=", ".", ",", "?", "!", "'", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", " ", "_", LV_SYMBOL_DOWN, LV_SYMBOL_OK, ""
};

static const lv_buttonmatrix_ctrl_t kb_ctrl_numbers[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 2,
    2, 7, 1, 2, 3
};

/* Layout 3: Symbols */
static const char * kb_map_symbols[] = {
    "[", "]", "{", "}", "#", "%", "^", "*", "+", "=", "\n",
    "_", "\\", "|", "~", "<", ">", "=", "/", "?", "!", "\n",
    "?123", ".", ",", "@", "&", "-", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", " ", "/", LV_SYMBOL_DOWN, LV_SYMBOL_OK, ""
};

static const lv_buttonmatrix_ctrl_t kb_ctrl_symbols[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 2,
    2, 7, 1, 2, 3
};

/* Forward declarations */
static void apply_keyboard_map(custom_keyboard_t * kb_data);
static void btnmatrix_event_cb(lv_event_t * e);
static void mode_btn_cb(lv_event_t * e);

static void apply_keyboard_map(custom_keyboard_t * kb_data)
{
    if (!kb_data || !kb_data->btnmatrix) return;

    const char ** map = NULL;
    const lv_buttonmatrix_ctrl_t * ctrl = NULL;
    const char * title_str = "KEYBOARD";

    switch (kb_data->mode) {
        case CUSTOM_KB_MODE_LOWER:
            map = kb_map_lower;
            ctrl = kb_ctrl_lower;
            title_str = "KEYBOARD • LOWERCASE";
            break;
        case CUSTOM_KB_MODE_UPPER:
            map = kb_map_upper;
            ctrl = kb_ctrl_upper;
            title_str = "KEYBOARD • UPPERCASE";
            break;
        case CUSTOM_KB_MODE_NUMBERS:
            map = kb_map_numbers;
            ctrl = kb_ctrl_numbers;
            title_str = "KEYBOARD • NUMERIC & SYMBOLS";
            break;
        case CUSTOM_KB_MODE_SYMBOLS:
            map = kb_map_symbols;
            ctrl = kb_ctrl_symbols;
            title_str = "KEYBOARD • EXTENDED SYMBOLS";
            break;
    }

    lv_buttonmatrix_set_map(kb_data->btnmatrix, map);
    
    /* Calculate matrix button count */
    uint32_t i = 0;
    uint32_t btn_count = 0;
    while (map[i][0] != '\0') {
        if (strcmp(map[i], "\n") != 0) {
            btn_count++;
        }
        i++;
    }

    for (uint32_t b = 0; b < btn_count; b++) {
        lv_buttonmatrix_set_button_width(kb_data->btnmatrix, b, ctrl[b]);
    }

    if (kb_data->header_title) {
        lv_label_set_text(kb_data->header_title, title_str);
    }

    /* Highlight active tab button */
    if (kb_data->btn_mode_abc && kb_data->btn_mode_123 && kb_data->btn_mode_sym) {
        lv_obj_set_style_bg_color(kb_data->btn_mode_abc, (kb_data->mode == CUSTOM_KB_MODE_LOWER || kb_data->mode == CUSTOM_KB_MODE_UPPER) ? COLOR_KB_ACCENT_BLUE : COLOR_KB_KEY_BG, 0);
        lv_obj_set_style_bg_color(kb_data->btn_mode_123, (kb_data->mode == CUSTOM_KB_MODE_NUMBERS) ? COLOR_KB_ACCENT_BLUE : COLOR_KB_KEY_BG, 0);
        lv_obj_set_style_bg_color(kb_data->btn_mode_sym, (kb_data->mode == CUSTOM_KB_MODE_SYMBOLS) ? COLOR_KB_ACCENT_BLUE : COLOR_KB_KEY_BG, 0);
    }
}

static void btnmatrix_event_cb(lv_event_t * e)
{
    lv_obj_t * btnm = lv_event_get_target(e);
    custom_keyboard_t * kb_data = (custom_keyboard_t *)lv_event_get_user_data(e);
    if (!kb_data) return;

    uint32_t btn_id = lv_buttonmatrix_get_selected_button(btnm);
    if (btn_id == LV_BUTTONMATRIX_BUTTON_NONE) return;

    const char * txt = lv_buttonmatrix_get_button_text(btnm, btn_id);
    if (!txt) return;

    /* Handle Special Control Keys */
    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        if (kb_data->target_ta) {
            lv_textarea_delete_char(kb_data->target_ta);
        }
    } else if (strcmp(txt, LV_SYMBOL_UP) == 0) {
        if (kb_data->mode == CUSTOM_KB_MODE_LOWER) {
            kb_data->mode = CUSTOM_KB_MODE_UPPER;
            kb_data->temp_upper = true;
        } else if (kb_data->mode == CUSTOM_KB_MODE_UPPER) {
            kb_data->mode = CUSTOM_KB_MODE_LOWER;
            kb_data->temp_upper = false;
        }
        apply_keyboard_map(kb_data);
    } else if (strcmp(txt, "?123") == 0) {
        kb_data->mode = CUSTOM_KB_MODE_NUMBERS;
        kb_data->temp_upper = false;
        apply_keyboard_map(kb_data);
    } else if (strcmp(txt, "#+=") == 0) {
        kb_data->mode = CUSTOM_KB_MODE_SYMBOLS;
        kb_data->temp_upper = false;
        apply_keyboard_map(kb_data);
    } else if (strcmp(txt, "ABC") == 0) {
        kb_data->mode = CUSTOM_KB_MODE_LOWER;
        kb_data->temp_upper = false;
        apply_keyboard_map(kb_data);
    } else if (strcmp(txt, LV_SYMBOL_DOWN) == 0) {
        custom_keyboard_show(kb_data->container, false);
    } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
        if (kb_data->done_cb) {
            kb_data->done_cb(kb_data->container, kb_data->target_ta);
        }
        custom_keyboard_show(kb_data->container, false);
    } else {
        /* Normal Character Key Press */
        if (kb_data->target_ta) {
            lv_textarea_add_text(kb_data->target_ta, txt);
        }
        /* If in temporary upper mode, auto-switch back to lowercase after typing */
        if (kb_data->temp_upper && kb_data->mode == CUSTOM_KB_MODE_UPPER) {
            kb_data->mode = CUSTOM_KB_MODE_LOWER;
            kb_data->temp_upper = false;
            apply_keyboard_map(kb_data);
        }
    }
}

static void mode_btn_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    custom_keyboard_t * kb_data = (custom_keyboard_t *)lv_event_get_user_data(e);
    if (!kb_data) return;

    if (btn == kb_data->btn_mode_abc) {
        kb_data->mode = CUSTOM_KB_MODE_LOWER;
    } else if (btn == kb_data->btn_mode_123) {
        kb_data->mode = CUSTOM_KB_MODE_NUMBERS;
    } else if (btn == kb_data->btn_mode_sym) {
        kb_data->mode = CUSTOM_KB_MODE_SYMBOLS;
    }
    kb_data->temp_upper = false;
    apply_keyboard_map(kb_data);
}

static void close_btn_cb(lv_event_t * e)
{
    custom_keyboard_t * kb_data = (custom_keyboard_t *)lv_event_get_user_data(e);
    if (kb_data && kb_data->container) {
        custom_keyboard_show(kb_data->container, false);
    }
}

lv_obj_t * custom_keyboard_create(lv_obj_t * parent)
{
    if (!parent) {
        parent = lv_screen_active();
    }

    custom_keyboard_t * kb_data = (custom_keyboard_t *)lv_malloc(sizeof(custom_keyboard_t));
    memset(kb_data, 0, sizeof(custom_keyboard_t));

    /* Main Container (Bottom Drawer Keyboard Panel) */
    lv_obj_t * container = lv_obj_create(parent);
    kb_data->container = container;
    lv_obj_set_size(container, 1000, 310);
    lv_obj_align(container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(container, COLOR_KB_BG, 0);
    lv_obj_set_style_border_color(container, COLOR_KB_BORDER, 0);
    lv_obj_set_style_border_width(container, 2, 0);
    lv_obj_set_style_radius(container, 16, 0);
    lv_obj_set_style_pad_all(container, 8, 0);
    lv_obj_set_style_shadow_color(container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(container, 30, 0);
    lv_obj_set_style_shadow_opa(container, LV_OPA_70, 0);

    /* 1. Header Bar */
    lv_obj_t * hdr = lv_obj_create(container);
    lv_obj_set_size(hdr, 980, 38);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x040D1A), 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 10, 0);
    lv_obj_set_style_pad_hor(hdr, 8, 0);
    lv_obj_set_style_pad_ver(hdr, 0, 0);

    /* Title Label */
    kb_data->header_title = lv_label_create(hdr);
    lv_label_set_text(kb_data->header_title, "KEYBOARD • LOWERCASE");
    lv_obj_set_style_text_font(kb_data->header_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(kb_data->header_title, COLOR_KB_ACCENT_BLUE, 0);
    lv_obj_align(kb_data->header_title, LV_ALIGN_LEFT_MID, 8, 0);

    /* Mode switcher quick tabs */
    kb_data->btn_mode_abc = lv_button_create(hdr);
    lv_obj_set_size(kb_data->btn_mode_abc, 55, 26);
    lv_obj_align(kb_data->btn_mode_abc, LV_ALIGN_RIGHT_MID, -170, 0);
    lv_obj_set_style_bg_color(kb_data->btn_mode_abc, COLOR_KB_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(kb_data->btn_mode_abc, 6, 0);
    lv_obj_set_style_pad_all(kb_data->btn_mode_abc, 0, 0);
    lv_obj_t * lbl_abc = lv_label_create(kb_data->btn_mode_abc);
    lv_label_set_text(lbl_abc, "ABC");
    lv_obj_set_style_text_font(lbl_abc, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_abc);
    lv_obj_add_event_cb(kb_data->btn_mode_abc, mode_btn_cb, LV_EVENT_CLICKED, kb_data);

    kb_data->btn_mode_123 = lv_button_create(hdr);
    lv_obj_set_size(kb_data->btn_mode_123, 55, 26);
    lv_obj_align(kb_data->btn_mode_123, LV_ALIGN_RIGHT_MID, -110, 0);
    lv_obj_set_style_bg_color(kb_data->btn_mode_123, COLOR_KB_KEY_BG, 0);
    lv_obj_set_style_radius(kb_data->btn_mode_123, 6, 0);
    lv_obj_set_style_pad_all(kb_data->btn_mode_123, 0, 0);
    lv_obj_t * lbl_123 = lv_label_create(kb_data->btn_mode_123);
    lv_label_set_text(lbl_123, "123");
    lv_obj_set_style_text_font(lbl_123, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_123);
    lv_obj_add_event_cb(kb_data->btn_mode_123, mode_btn_cb, LV_EVENT_CLICKED, kb_data);

    kb_data->btn_mode_sym = lv_button_create(hdr);
    lv_obj_set_size(kb_data->btn_mode_sym, 55, 26);
    lv_obj_align(kb_data->btn_mode_sym, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_obj_set_style_bg_color(kb_data->btn_mode_sym, COLOR_KB_KEY_BG, 0);
    lv_obj_set_style_radius(kb_data->btn_mode_sym, 6, 0);
    lv_obj_set_style_pad_all(kb_data->btn_mode_sym, 0, 0);
    lv_obj_t * lbl_sym = lv_label_create(kb_data->btn_mode_sym);
    lv_label_set_text(lbl_sym, "#+=");
    lv_obj_set_style_text_font(lbl_sym, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_sym);
    lv_obj_add_event_cb(kb_data->btn_mode_sym, mode_btn_cb, LV_EVENT_CLICKED, kb_data);

    /* Close Button */
    lv_obj_t * btn_close = lv_button_create(hdr);
    lv_obj_set_size(btn_close, 36, 26);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x401018), 0);
    lv_obj_set_style_radius(btn_close, 6, 0);
    lv_obj_set_style_pad_all(btn_close, 0, 0);
    lv_obj_t * lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "X");
    lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0xFF5252), 0);
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(btn_close, close_btn_cb, LV_EVENT_CLICKED, kb_data);

    /* 2. Button Matrix Keys */
    lv_obj_t * btnm = lv_buttonmatrix_create(container);
    kb_data->btnmatrix = btnm;
    lv_obj_set_size(btnm, 980, 245);
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Button matrix aesthetics */
    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnm, 0, 0);
    lv_obj_set_style_pad_all(btnm, 4, 0);
    lv_obj_set_style_pad_gap(btnm, 6, 0);

    /* Key item styling */
    lv_obj_set_style_bg_color(btnm, COLOR_KB_KEY_BG, LV_PART_ITEMS);
    lv_obj_set_style_border_color(btnm, COLOR_KB_KEY_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 8, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, COLOR_KB_KEY_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(btnm, COLOR_KB_KEY_PRESSED, LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_obj_add_event_cb(btnm, btnmatrix_event_cb, LV_EVENT_VALUE_CHANGED, kb_data);

    /* Save kb_data pointer to container */
    lv_obj_set_user_data(container, kb_data);

    /* Apply initial Lowercase map */
    kb_data->mode = CUSTOM_KB_MODE_LOWER;
    apply_keyboard_map(kb_data);

    /* Start hidden by default */
    custom_keyboard_show(container, false);

    return container;
}

void custom_keyboard_set_textarea(lv_obj_t * kb, lv_obj_t * ta)
{
    if (!kb) return;
    custom_keyboard_t * kb_data = (custom_keyboard_t *)lv_obj_get_user_data(kb);
    if (kb_data) {
        kb_data->target_ta = ta;
    }
}

void custom_keyboard_set_mode(lv_obj_t * kb, custom_kb_mode_t mode)
{
    if (!kb) return;
    custom_keyboard_t * kb_data = (custom_keyboard_t *)lv_obj_get_user_data(kb);
    if (kb_data) {
        kb_data->mode = mode;
        kb_data->temp_upper = false;
        apply_keyboard_map(kb_data);
    }
}

void custom_keyboard_show(lv_obj_t * kb, bool show)
{
    if (!kb) return;
    if (show) {
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(kb);
    } else {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

void custom_keyboard_set_done_cb(lv_obj_t * kb, custom_kb_done_cb_t cb)
{
    if (!kb) return;
    custom_keyboard_t * kb_data = (custom_keyboard_t *)lv_obj_get_user_data(kb);
    if (kb_data) {
        kb_data->done_cb = cb;
    }
}
