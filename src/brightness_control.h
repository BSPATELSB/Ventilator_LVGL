#ifndef BRIGHTNESS_CONTROL_H
#define BRIGHTNESS_CONTROL_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the dynamic brightness control backend & visual layer overlay.
 */
void brightness_control_init(void);

/**
 * @brief Set the brightness level dynamically (clamped between 10% and 100%).
 * @param level Target brightness percentage (10 - 100).
 */
void brightness_set_level(int level);

/**
 * @brief Get current brightness level (10 - 100).
 */
int brightness_get_level(void);

/**
 * @brief Increment brightness by specified step percentage.
 * @param step Percentage amount to increase (e.g. 5 or 10).
 * @return New brightness level.
 */
int brightness_increment(int step);

/**
 * @brief Decrement brightness by specified step percentage.
 * @param step Percentage amount to decrease (e.g. 5 or 10).
 * @return New brightness level.
 */
int brightness_decrement(int step);

/**
 * @brief Enable or disable auto-brightness adjustment based on ambient light sensor backend.
 * @param enable true to enable auto-brightness, false for manual mode.
 */
void brightness_set_auto(bool enable);

/**
 * @brief Check if auto-brightness is currently enabled.
 */
bool brightness_is_auto(void);

#ifdef __cplusplus
}
#endif

#endif /* BRIGHTNESS_CONTROL_H */
