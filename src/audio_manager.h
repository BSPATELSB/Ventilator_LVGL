#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @brief Initialize the Audio Subsystem and pre-load ECG beep & sound assets.
 */
void audio_manager_init(void);

/**
 * @brief De-initialize audio subsystem resources.
 */
void audio_manager_deinit(void);

/**
 * @brief Play the realistic ECG beep sound effect (Audio/Ecg_beep.wav / Audio/Ecg_beep.mp3).
 *        Respects master volume and ECG beep toggle/volume settings.
 */
void audio_play_ecg_beep(void);

/**
 * @brief Play touch / key click sound effect.
 */
void audio_play_touch_sound(void);

/**
 * @brief Play high-priority alarm notification sound.
 */
void audio_play_alarm_sound(void);

/* Volume & Enable Getters / Setters */
int  audio_get_master_volume(void);
void audio_set_master_volume(int vol);

bool audio_get_ecg_enabled(void);
void audio_set_ecg_enabled(bool enabled);
int  audio_get_ecg_volume(void);
void audio_set_ecg_volume(int vol);

int  audio_get_alarm_volume(void);
void audio_set_alarm_volume(int vol);
bool audio_get_alarm_muted(void);
void audio_set_alarm_muted(bool muted);

bool audio_get_touch_enabled(void);
void audio_set_touch_enabled(bool enabled);
int  audio_get_touch_volume(void);
void audio_set_touch_volume(int vol);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_MANAGER_H */
