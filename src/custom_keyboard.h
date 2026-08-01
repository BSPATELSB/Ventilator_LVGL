#ifndef CUSTOM_KEYBOARD_H
#define CUSTOM_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

typedef enum {
    CUSTOM_KB_MODE_LOWER = 0,
    CUSTOM_KB_MODE_UPPER,
    CUSTOM_KB_MODE_NUMBERS,
    CUSTOM_KB_MODE_SYMBOLS
} custom_kb_mode_t;

typedef void (*custom_kb_done_cb_t)(lv_obj_t * kb, lv_obj_t * ta);

/**
 * @brief Create a high-aesthetic, flexible medical UI virtual keyboard.
 * @param parent Parent container or NULL to place on active screen/layer
 * @return lv_obj_t* Pointer to keyboard object
 */
lv_obj_t * custom_keyboard_create(lv_obj_t * parent);

/**
 * @brief Bind keyboard to a specific text area input.
 */
void custom_keyboard_set_textarea(lv_obj_t * kb, lv_obj_t * ta);

/**
 * @brief Switch keyboard mode (Lowercase, Uppercase, Numbers, Symbols).
 */
void custom_keyboard_set_mode(lv_obj_t * kb, custom_kb_mode_t mode);

/**
 * @brief Show or hide the keyboard smoothly.
 */
void custom_keyboard_show(lv_obj_t * kb, bool show);

/**
 * @brief Register custom callback function invoked when user presses Done / Enter.
 */
void custom_keyboard_set_done_cb(lv_obj_t * kb, custom_kb_done_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_KEYBOARD_H */
