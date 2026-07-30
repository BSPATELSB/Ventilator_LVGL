#ifndef VENTILATOR_TIME_SCREEN_H
#define VENTILATOR_TIME_SCREEN_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Global time offset in seconds added to system time.
 */
extern time_t ventilator_time_offset;

/**
 * @brief Get the current time adjusted by ventilator_time_offset.
 * @param timer Pointer to time_t to store time, or NULL
 * @return Adjusted time_t value
 */
time_t ventilator_get_current_time(time_t * timer);

/**
 * @brief Create and render the Date, Time & Day adjustment screen (1280x800).
 */
void create_ventilator_time_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* VENTILATOR_TIME_SCREEN_H */
