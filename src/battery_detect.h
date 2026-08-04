#ifndef BATTERY_DETECT_H
#define BATTERY_DETECT_H

#include <stdbool.h>
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int capacity;         /* 0 to 100 percentage */
    bool is_charging;     /* true if battery is actively charging */
    bool is_present;      /* true if system battery device found */
    char status_str[32];  /* "Charging", "Discharging", "Full", "Not charging", etc. */
} battery_info_t;

/**
 * @brief Initialize battery detection module
 */
void battery_detect_init(void);

/**
 * @brief Read actual battery status from Linux sysfs (/sys/class/power_supply/)
 * @param info Pointer to battery_info_t structure to populate
 * @return true if battery state was read successfully
 */
bool battery_detect_read(battery_info_t * info);

/**
 * @brief Helper to update an LVGL battery label with current icon, percentage, and color
 * @param label Pointer to lv_obj_t label created in LVGL UI
 */
void battery_update_label(lv_obj_t * label);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_DETECT_H */
