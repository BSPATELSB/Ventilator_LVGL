#include "audio_manager.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* State variables with clean default values */
static int g_master_volume = 80;      /* 0 to 100% */
static bool g_ecg_enabled = true;
static int g_ecg_volume = 80;         /* 0 to 100% */
static int g_alarm_volume = 90;       /* 0 to 100% */
static bool g_alarm_muted = false;
static bool g_touch_enabled = true;
static int g_touch_volume = 70;       /* 0 to 100% */

/* SDL Audio state */
static SDL_AudioDeviceID g_audio_dev = 0;
static SDL_AudioSpec g_obtained_spec;
static bool g_audio_inited = false;

/* PCM Buffers */
static Uint8 *g_ecg_buffer = NULL;
static Uint32 g_ecg_length = 0;
static Uint32 g_ecg_play_pos = 0;

static Uint8 *g_touch_buffer = NULL;
static Uint32 g_touch_length = 0;
static Uint32 g_touch_play_pos = 0;

static Uint8 *g_alarm_buffer = NULL;
static Uint32 g_alarm_length = 0;
static Uint32 g_alarm_play_pos = 0;

/* Synthesize a clean fallback PCM wave buffer (S16LSB, 44100Hz, Stereo) */
static Uint8 * generate_synth_beep(Uint32 freq_hz, Uint32 duration_ms, Uint32 * out_len)
{
    Uint32 sample_rate = 44100;
    Uint32 num_samples = (sample_rate * duration_ms) / 1000;
    Uint32 bytes_per_sample = 4; // 16-bit stereo = 2 bytes * 2 channels
    Uint32 total_bytes = num_samples * bytes_per_sample;

    Uint8 * buf = (Uint8 *)malloc(total_bytes);
    if (!buf) return NULL;

    int16_t * pcm = (int16_t *)buf;
    for (Uint32 i = 0; i < num_samples; i++) {
        float t = (float)i / (float)sample_rate;
        // Apply smooth envelope (fade in 5ms, fade out 10ms)
        float env = 1.0f;
        float dur_sec = (float)duration_ms / 1000.0f;
        if (t < 0.005f) env = t / 0.005f;
        else if (t > dur_sec - 0.010f) env = (dur_sec - t) / 0.010f;
        if (env < 0.0f) env = 0.0f;

        float sample_val = sinf(2.0f * (float)M_PI * (float)freq_hz * t) * env;
        int16_t val = (int16_t)(sample_val * 24000.0f);

        pcm[i * 2]     = val; // Left channel
        pcm[i * 2 + 1] = val; // Right channel
    }

    *out_len = total_bytes;
    return buf;
}

/* Audio device output callback */
static void audio_device_callback(void * userdata, Uint8 * stream, int len)
{
    (void)userdata;
    SDL_memset(stream, 0, len);

    // 1. ECG Beep
    if (g_ecg_enabled && g_master_volume > 0 && g_ecg_volume > 0 && g_ecg_buffer && g_ecg_play_pos < g_ecg_length) {
        Uint32 rem = g_ecg_length - g_ecg_play_pos;
        Uint32 chunk = (rem < (Uint32)len) ? rem : (Uint32)len;
        int mix_vol = (int)(((float)g_master_volume / 100.0f) * ((float)g_ecg_volume / 100.0f) * (float)SDL_MIX_MAXVOLUME);
        if (mix_vol > SDL_MIX_MAXVOLUME) mix_vol = SDL_MIX_MAXVOLUME;

        SDL_MixAudioFormat(stream, g_ecg_buffer + g_ecg_play_pos, g_obtained_spec.format, chunk, mix_vol);
        g_ecg_play_pos += chunk;
    }

    // 2. Touch Sound
    if (g_touch_enabled && g_master_volume > 0 && g_touch_volume > 0 && g_touch_buffer && g_touch_play_pos < g_touch_length) {
        Uint32 rem = g_touch_length - g_touch_play_pos;
        Uint32 chunk = (rem < (Uint32)len) ? rem : (Uint32)len;
        int mix_vol = (int)(((float)g_master_volume / 100.0f) * ((float)g_touch_volume / 100.0f) * (float)SDL_MIX_MAXVOLUME);
        if (mix_vol > SDL_MIX_MAXVOLUME) mix_vol = SDL_MIX_MAXVOLUME;

        SDL_MixAudioFormat(stream, g_touch_buffer + g_touch_play_pos, g_obtained_spec.format, chunk, mix_vol);
        g_touch_play_pos += chunk;
    }

    // 3. Alarm Sound
    if (!g_alarm_muted && g_master_volume > 0 && g_alarm_volume > 0 && g_alarm_buffer && g_alarm_play_pos < g_alarm_length) {
        Uint32 rem = g_alarm_length - g_alarm_play_pos;
        Uint32 chunk = (rem < (Uint32)len) ? rem : (Uint32)len;
        int mix_vol = (int)(((float)g_master_volume / 100.0f) * ((float)g_alarm_volume / 100.0f) * (float)SDL_MIX_MAXVOLUME);
        if (mix_vol > SDL_MIX_MAXVOLUME) mix_vol = SDL_MIX_MAXVOLUME;

        SDL_MixAudioFormat(stream, g_alarm_buffer + g_alarm_play_pos, g_obtained_spec.format, chunk, mix_vol);
        g_alarm_play_pos += chunk;
    }
}

void audio_manager_init(void)
{
    if (g_audio_inited) return;

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            fprintf(stderr, "[Audio] SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
        }
    }

    // Try loading Audio/Ecg_beep.wav
    SDL_AudioSpec wav_spec;
    if (SDL_LoadWAV("Audio/Ecg_beep.wav", &wav_spec, &g_ecg_buffer, &g_ecg_length) != NULL) {
        printf("[Audio] Loaded Audio/Ecg_beep.wav (%u bytes)\n", g_ecg_length);
    } else {
        printf("[Audio] Audio/Ecg_beep.wav not found or invalid (%s), synthesizing ECG beep fallback\n", SDL_GetError());
        g_ecg_buffer = generate_synth_beep(880, 150, &g_ecg_length); // 880Hz pitch, 150ms length
    }

    // Generate Touch Click (1200Hz, 30ms)
    g_touch_buffer = generate_synth_beep(1200, 30, &g_touch_length);

    // Generate Alarm Sound (1500Hz, 300ms)
    g_alarm_buffer = generate_synth_beep(1500, 300, &g_alarm_length);

    // Setup SDL Audio Device
    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = 44100;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = audio_device_callback;
    desired.userdata = NULL;

    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired, &g_obtained_spec, 0);
    if (g_audio_dev != 0) {
        SDL_PauseAudioDevice(g_audio_dev, 0); // Start audio output thread
        printf("[Audio] Audio device opened successfully (ID: %u, Freq: %d)\n", g_audio_dev, g_obtained_spec.freq);
    } else {
        fprintf(stderr, "[Audio] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    }

    g_audio_inited = true;
}

void audio_manager_deinit(void)
{
    if (!g_audio_inited) return;

    if (g_audio_dev != 0) {
        SDL_CloseAudioDevice(g_audio_dev);
        g_audio_dev = 0;
    }

    if (g_ecg_buffer) {
        // If loaded via SDL_LoadWAV or malloc
        free(g_ecg_buffer);
        g_ecg_buffer = NULL;
    }

    if (g_touch_buffer) {
        free(g_touch_buffer);
        g_touch_buffer = NULL;
    }

    if (g_alarm_buffer) {
        free(g_alarm_buffer);
        g_alarm_buffer = NULL;
    }

    g_audio_inited = false;
}

void audio_play_ecg_beep(void)
{
    if (!g_audio_inited) audio_manager_init();
    if (!g_ecg_enabled || g_master_volume <= 0 || g_ecg_volume <= 0) return;

    if (g_audio_dev != 0) {
        SDL_LockAudioDevice(g_audio_dev);
        g_ecg_play_pos = 0; // Rewind to start of ECG beep audio
        SDL_UnlockAudioDevice(g_audio_dev);
    }
}

void audio_play_touch_sound(void)
{
    if (!g_audio_inited) audio_manager_init();
    if (!g_touch_enabled || g_master_volume <= 0 || g_touch_volume <= 0) return;

    if (g_audio_dev != 0) {
        SDL_LockAudioDevice(g_audio_dev);
        g_touch_play_pos = 0;
        SDL_UnlockAudioDevice(g_audio_dev);
    }
}

void audio_play_alarm_sound(void)
{
    if (!g_audio_inited) audio_manager_init();
    if (g_alarm_muted || g_master_volume <= 0 || g_alarm_volume <= 0) return;

    if (g_audio_dev != 0) {
        SDL_LockAudioDevice(g_audio_dev);
        g_alarm_play_pos = 0;
        SDL_UnlockAudioDevice(g_audio_dev);
    }
}

/* Getters & Setters */
int audio_get_master_volume(void) { return g_master_volume; }
void audio_set_master_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_master_volume = vol;
}

bool audio_get_ecg_enabled(void) { return g_ecg_enabled; }
void audio_set_ecg_enabled(bool enabled) { g_ecg_enabled = enabled; }

int audio_get_ecg_volume(void) { return g_ecg_volume; }
void audio_set_ecg_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_ecg_volume = vol;
}

int audio_get_alarm_volume(void) { return g_alarm_volume; }
void audio_set_alarm_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_alarm_volume = vol;
}

bool audio_get_alarm_muted(void) { return g_alarm_muted; }
void audio_set_alarm_muted(bool muted) { g_alarm_muted = muted; }

bool audio_get_touch_enabled(void) { return g_touch_enabled; }
void audio_set_touch_enabled(bool enabled) { g_touch_enabled = enabled; }

int audio_get_touch_volume(void) { return g_touch_volume; }
void audio_set_touch_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_touch_volume = vol;
}
