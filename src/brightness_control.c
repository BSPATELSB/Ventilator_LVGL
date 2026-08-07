#include "brightness_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <math.h>

#define BRIGHTNESS_CONFIG_FILE "/tmp/ventilator_brightness.conf"
#define BRIGHTNESS_MIN 10
#define BRIGHTNESS_MAX 100

static int current_brightness = 80;
static bool auto_brightness_active = true;
static lv_obj_t * brightness_overlay = NULL;
static lv_timer_t * auto_bright_timer = NULL;
static char sysfs_backlight_path[256] = "";
static int sysfs_max_brightness = 255;
static bool sysfs_available = false;

/* Scan Linux sysfs for display backlight control directory */
static void init_sysfs_backlight(void)
{
    DIR * dir = opendir("/sys/class/backlight");
    if(!dir) return;

    struct dirent * entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') continue;
        
        snprintf(sysfs_backlight_path, sizeof(sysfs_backlight_path), "/sys/class/backlight/%s", entry->d_name);
        
        /* Read max_brightness */
        char max_path[300];
        snprintf(max_path, sizeof(max_path), "%s/max_brightness", sysfs_backlight_path);
        FILE * fp = fopen(max_path, "r");
        if(fp) {
            if(fscanf(fp, "%d", &sysfs_max_brightness) == 1 && sysfs_max_brightness > 0) {
                sysfs_available = true;
            }
            fclose(fp);
        }
        if(sysfs_available) break;
    }
    closedir(dir);
}

/* Write hardware brightness to Linux backlight sysfs or command fallback */
static void apply_hardware_brightness(int level)
{
    if(sysfs_available && sysfs_backlight_path[0] != '\0') {
        char bright_path[300];
        snprintf(bright_path, sizeof(bright_path), "%s/brightness", sysfs_backlight_path);
        FILE * fp = fopen(bright_path, "w");
        if(fp) {
            int raw_val = (int)roundf((float)level * sysfs_max_brightness / 100.0f);
            if(raw_val < 1) raw_val = 1;
            fprintf(fp, "%d\n", raw_val);
            fclose(fp);
            return;
        }
    }

    /* System command fallback (e.g. brightnessctl or xrandr) */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "brightnessctl set %d%% > /dev/null 2>&1 &", level);
    (void)system(cmd);
}

/* Save current brightness level to config file */
static void save_brightness_config(void)
{
    FILE * fp = fopen(BRIGHTNESS_CONFIG_FILE, "w");
    if(fp) {
        fprintf(fp, "%d %d\n", current_brightness, auto_brightness_active ? 1 : 0);
        fclose(fp);
    }
}

/* Load stored brightness level from config file */
static void load_brightness_config(void)
{
    FILE * fp = fopen(BRIGHTNESS_CONFIG_FILE, "r");
    if(fp) {
        int val = 80, is_auto = 1;
        if(fscanf(fp, "%d %d", &val, &is_auto) >= 1) {
            if(val >= BRIGHTNESS_MIN && val <= BRIGHTNESS_MAX) {
                current_brightness = val;
            }
            auto_brightness_active = (is_auto != 0);
        }
        fclose(fp);
    }
}

/* Apply visual dimming overlay opacity on LVGL system layer */
static void apply_visual_brightness(int level)
{
    if(!brightness_overlay) {
        lv_obj_t * sys_layer = lv_layer_sys();
        if(!sys_layer) return;

        brightness_overlay = lv_obj_create(sys_layer);
        lv_obj_set_size(brightness_overlay, LV_PCT(100), LV_PCT(100));
        lv_obj_set_pos(brightness_overlay, 0, 0);
        
        /* Ensure overlay allows all touch events to pass right through */
        lv_obj_remove_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(brightness_overlay, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_set_style_bg_color(brightness_overlay, lv_color_black(), 0);
        lv_obj_set_style_border_width(brightness_overlay, 0, 0);
        lv_obj_set_style_pad_all(brightness_overlay, 0, 0);
        lv_obj_set_style_radius(brightness_overlay, 0, 0);
    }

    /* 100% -> opa = 0 (fully transparent, max bright)
     * 10% -> opa = 210 (darkened/dimmed) */
    float factor = (100.0f - (float)level) / 90.0f;
    if(factor < 0.0f) factor = 0.0f;
    if(factor > 1.0f) factor = 1.0f;

    uint8_t opa = (uint8_t)(factor * 215.0f);
    lv_obj_set_style_bg_opa(brightness_overlay, opa, 0);
}

/* Auto-brightness sensor simulation timer callback */
static void auto_bright_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(!auto_brightness_active) return;

    /* Simulate dynamic ambient room light fluctuation (realistic medical environment simulation) */
    static int target_lux = 75;
    static int phase = 0;
    phase = (phase + 1) % 8;
    if(phase == 0) {
        target_lux = 50 + (rand() % 45); /* 50% - 95% range */
    }

    if(current_brightness < target_lux) {
        brightness_increment(1);
    } else if(current_brightness > target_lux) {
        brightness_decrement(1);
    }
}

void brightness_control_init(void)
{
    init_sysfs_backlight();
    load_brightness_config();
    apply_visual_brightness(current_brightness);
    apply_hardware_brightness(current_brightness);

    if(auto_brightness_active && !auto_bright_timer) {
        auto_bright_timer = lv_timer_create(auto_bright_timer_cb, 2500, NULL);
    }
}

void brightness_set_level(int level)
{
    if(level < BRIGHTNESS_MIN) level = BRIGHTNESS_MIN;
    if(level > BRIGHTNESS_MAX) level = BRIGHTNESS_MAX;

    current_brightness = level;
    apply_visual_brightness(current_brightness);
    apply_hardware_brightness(current_brightness);
    save_brightness_config();
}

int brightness_get_level(void)
{
    return current_brightness;
}

int brightness_increment(int step)
{
    int new_level = current_brightness + step;
    brightness_set_level(new_level);
    return current_brightness;
}

int brightness_decrement(int step)
{
    int new_level = current_brightness - step;
    brightness_set_level(new_level);
    return current_brightness;
}

void brightness_set_auto(bool enable)
{
    auto_brightness_active = enable;
    if(auto_brightness_active) {
        if(!auto_bright_timer) {
            auto_bright_timer = lv_timer_create(auto_bright_timer_cb, 2500, NULL);
        } else {
            lv_timer_resume(auto_bright_timer);
        }
    } else {
        if(auto_bright_timer) {
            lv_timer_pause(auto_bright_timer);
        }
    }
    save_brightness_config();
}

bool brightness_is_auto(void)
{
    return auto_brightness_active;
}
