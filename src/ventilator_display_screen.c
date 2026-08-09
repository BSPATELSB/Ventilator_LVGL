#include "ventilator_display_screen.h"
#include "ventilator_settings_screen.h"
#include "ventilator_main_screen.h"
#include "ventilator_time_screen.h"
#include "battery_detect.h"
#include "brightness_control.h"
#include "theme_manager.h"
#include "audio_manager.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Visual Theme Colors matching Ventilator UI Design */
#define COLOR_DASHBOARD_BG      (theme_get_palette()->bg)
#define COLOR_CARD_BG           (theme_get_palette()->card_bg)
#define COLOR_CARD_BORDER       (theme_get_palette()->card_border)
#define COLOR_ACCENT_BLUE       (theme_get_palette()->accent_blue)
#define COLOR_ACCENT_GREEN      (theme_get_palette()->accent_green)
#define COLOR_ACCENT_YELLOW     (theme_get_palette()->accent_yellow)
#define COLOR_ACCENT_RED        (theme_get_palette()->accent_red)
#define COLOR_TEXT_MAIN         (theme_get_palette()->text_main)
#define COLOR_TEXT_MUTED        (theme_get_palette()->text_muted)
#define COLOR_BTN_NAV_BG        (theme_get_palette()->btn_bg)
#define COLOR_BTN_NAV_ACTIVE    (theme_get_palette()->btn_active)
#define COLOR_PANEL_HEADER      (theme_get_palette()->panel_hdr)
#define COLOR_ITEM_HOVER        (theme_get_palette()->item_hover)

/* Display Settings State Variables */
static int brightness_val = 80;            /* 0 - 100% */
static bool auto_brightness_enabled = true;
static bool night_dimming_enabled = true;
static int screen_timeout_idx = 2;          /* 0: Never, 1: 1 Min, 2: 5 Min, 3: 15 Min, 4: 30 Min */
static int screensaver_mode_idx = 1;        /* 0: Disabled, 1: Clock & Status, 2: Blank Screen */
static int sleep_dim_level = 20;            /* 10 - 50% */
static int theme_mode_idx = 0;              /* 0: Dark Medical, 1: Light Mode, 2: Night Vision */
static int waveform_palette_idx = 0;       /* 0: Standard, 1: High Contrast Neon, 2: Monochromatic Blue */
static int ui_scale_idx = 0;                /* 0: 100% Standard, 1: 115% Medium, 2: 130% Large Text */
static int orientation_idx = 0;             /* 0: Landscape Standard, 1: Landscape Inverted */
static bool touch_sound_enabled = true;
static int fps_mode_idx = 0;                /* 0: 60 FPS Smooth, 1: 30 FPS Power Saver */

/* UI Object Handles */
static lv_timer_t * clock_timer = NULL;
static lv_timer_t * toast_timer = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * bat_lbl = NULL;
static lv_obj_t * main_screen_obj = NULL;

static lv_obj_t * slider_brightness = NULL;
static lv_obj_t * lbl_brightness_val = NULL;
static lv_obj_t * sw_auto_bright = NULL;
static lv_obj_t * sw_night_dim = NULL;
static lv_obj_t * preset_btns[4] = {NULL};

static lv_obj_t * dd_timeout = NULL;
static lv_obj_t * btn_saver_modes[3] = {NULL};
static lv_obj_t * slider_sleep_dim = NULL;
static lv_obj_t * lbl_sleep_dim_val = NULL;

static lv_obj_t * theme_cards[3] = {NULL};
static lv_obj_t * dd_palette = NULL;

static lv_obj_t * dd_scale = NULL;
static lv_obj_t * dd_orientation = NULL;
static lv_obj_t * sw_touch_sound = NULL;
static lv_obj_t * dd_fps = NULL;

static lv_obj_t * toast_banner = NULL;
static lv_obj_t * lbl_toast_msg = NULL;

/* External Helper */
extern void disable_scroll_recursive(lv_obj_t * obj);

/* Clock Timer Callback */
static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(lbl_clock) {
        time_t raw_time = ventilator_get_current_time(NULL);
        struct tm * time_info = localtime(&raw_time);
        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%d %b %Y\n%I:%M %p", time_info);
        lv_label_set_text(lbl_clock, clock_buf);
    }
    if(bat_lbl) {
        battery_update_label(bat_lbl);
    }
}

/* Toast Hide Callback */
static void toast_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(toast_banner) {
        lv_obj_add_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
}

/* Display Toast Notification Banner */
static void show_toast_message(const char * msg, bool is_error)
{
    if(!toast_banner || !lbl_toast_msg) return;

    lv_label_set_text(lbl_toast_msg, msg);
    lv_obj_set_style_bg_color(toast_banner, is_error ? lv_color_hex(0x7A0D0D) : lv_color_hex(0x0D6436), 0);
    lv_obj_set_style_border_color(toast_banner, is_error ? COLOR_ACCENT_RED : COLOR_ACCENT_GREEN, 0);
    lv_obj_remove_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);

    if(toast_timer) {
        lv_timer_delete(toast_timer);
    }
    toast_timer = lv_timer_create(toast_timer_cb, 3000, NULL);
}

/* Navigation Callbacks */
static void back_to_settings_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    create_ventilator_settings_screen();
}

static void home_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    create_ventilator_main_screen();
}

static void pat_badge_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    extern void create_ventilator_patient_screen(void);
    create_ventilator_patient_screen();
}

static void date_box_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    create_ventilator_time_screen();
}

/* Update Preset Button Visuals */
static void update_preset_buttons_ui(void)
{
    int preset_vals[4] = {25, 50, 75, 100};
    for(int i = 0; i < 4; i++) {
        if(preset_btns[i]) {
            if(brightness_val == preset_vals[i]) {
                lv_obj_set_style_bg_color(preset_btns[i], COLOR_BTN_NAV_ACTIVE, 0);
                lv_obj_set_style_border_color(preset_btns[i], COLOR_ACCENT_BLUE, 0);
            } else {
                lv_obj_set_style_bg_color(preset_btns[i], COLOR_BTN_NAV_BG, 0);
                lv_obj_set_style_border_color(preset_btns[i], COLOR_CARD_BORDER, 0);
            }
        }
    }
}

/* Brightness Slider Callback (Dynamic Real-time Update) */
static void brightness_slider_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    brightness_set_level(val);
    brightness_val = brightness_get_level();
    if(lbl_brightness_val) {
        lv_label_set_text_fmt(lbl_brightness_val, "%d%%", brightness_val);
    }
    update_preset_buttons_ui();
}

/* Brightness Decrement Button Callback */
static void brightness_dec_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    int val = brightness_decrement(5);
    brightness_val = val;
    if(slider_brightness) {
        lv_slider_set_value(slider_brightness, brightness_val, LV_ANIM_ON);
    }
    if(lbl_brightness_val) {
        lv_label_set_text_fmt(lbl_brightness_val, "%d%%", brightness_val);
    }
    update_preset_buttons_ui();
}

/* Brightness Increment Button Callback */
static void brightness_inc_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    int val = brightness_increment(5);
    brightness_val = val;
    if(slider_brightness) {
        lv_slider_set_value(slider_brightness, brightness_val, LV_ANIM_ON);
    }
    if(lbl_brightness_val) {
        lv_label_set_text_fmt(lbl_brightness_val, "%d%%", brightness_val);
    }
    update_preset_buttons_ui();
}

/* Preset Button Callback */
static void preset_btn_cb(lv_event_t * e)
{
    int val = (int)(intptr_t)lv_event_get_user_data(e);
    brightness_set_level(val);
    brightness_val = brightness_get_level();
    if(slider_brightness) {
        lv_slider_set_value(slider_brightness, brightness_val, LV_ANIM_ON);
    }
    if(lbl_brightness_val) {
        lv_label_set_text_fmt(lbl_brightness_val, "%d%%", brightness_val);
    }
    update_preset_buttons_ui();
    show_toast_message("Brightness preset selected.", false);
}

/* Auto Brightness Switch Callback */
static void sw_auto_bright_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    auto_brightness_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    brightness_set_auto(auto_brightness_enabled);
    if(auto_brightness_enabled) {
        show_toast_message("Auto-brightness enabled (Ambient Light Sensor).", false);
    } else {
        show_toast_message("Auto-brightness disabled. Manual brightness active.", false);
    }
}

/* Night Dimming Switch Callback */
static void sw_night_dim_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    night_dimming_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if(night_dimming_enabled) {
        show_toast_message("Night Auto-Dimming enabled (10:00 PM - 06:00 AM).", false);
    } else {
        show_toast_message("Night Auto-Dimming disabled.", false);
    }
}

/* Screen Timeout Dropdown Callback */
static void dd_timeout_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    screen_timeout_idx = lv_dropdown_get_selected(dd);
    const char * options[] = {"Never (Always On)", "1 Minute", "5 Minutes", "15 Minutes", "30 Minutes"};
    char buf[128];
    snprintf(buf, sizeof(buf), "Screen timeout set to %s.", options[screen_timeout_idx]);
    show_toast_message(buf, false);
}

/* Update Saver Mode Buttons Visuals */
static void update_saver_buttons_ui(void)
{
    for(int i = 0; i < 3; i++) {
        if(btn_saver_modes[i]) {
            if(screensaver_mode_idx == i) {
                lv_obj_set_style_bg_color(btn_saver_modes[i], COLOR_BTN_NAV_ACTIVE, 0);
                lv_obj_set_style_border_color(btn_saver_modes[i], COLOR_ACCENT_BLUE, 0);
            } else {
                lv_obj_set_style_bg_color(btn_saver_modes[i], COLOR_BTN_NAV_BG, 0);
                lv_obj_set_style_border_color(btn_saver_modes[i], COLOR_CARD_BORDER, 0);
            }
        }
    }
}

/* Screensaver Mode Callback */
static void btn_saver_mode_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    screensaver_mode_idx = idx;
    update_saver_buttons_ui();
    const char * names[] = {"Disabled", "Clock & Status", "Blank Screen"};
    char buf[128];
    snprintf(buf, sizeof(buf), "Screensaver mode set to %s.", names[screensaver_mode_idx]);
    show_toast_message(buf, false);
}

/* Sleep Dimming Level Slider Callback */
static void slider_sleep_dim_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    sleep_dim_level = lv_slider_get_value(slider);
    if(lbl_sleep_dim_val) {
        lv_label_set_text_fmt(lbl_sleep_dim_val, "%d%%", sleep_dim_level);
    }
}

/* Update Theme Cards Visuals */
static void update_theme_cards_ui(void)
{
    for(int i = 0; i < 3; i++) {
        if(theme_cards[i]) {
            if(theme_mode_idx == i) {
                lv_obj_set_style_border_color(theme_cards[i], COLOR_ACCENT_BLUE, 0);
                lv_obj_set_style_border_width(theme_cards[i], 2, 0);
                lv_obj_set_style_bg_color(theme_cards[i], COLOR_BTN_NAV_ACTIVE, 0);
            } else {
                lv_obj_set_style_border_color(theme_cards[i], COLOR_CARD_BORDER, 0);
                lv_obj_set_style_border_width(theme_cards[i], 1, 0);
                lv_obj_set_style_bg_color(theme_cards[i], COLOR_CARD_BG, 0);
            }
        }
    }
}

/* Theme Card Click Callback */
static void theme_card_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    theme_mode_idx = idx;
    theme_set_mode((theme_mode_t)theme_mode_idx);
    update_theme_cards_ui();

    char buf[128];
    snprintf(buf, sizeof(buf), "Color theme set to %s.", theme_get_mode_name((theme_mode_t)theme_mode_idx));
    show_toast_message(buf, false);

    /* Cleanly re-render display screen with new theme palette */
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    create_ventilator_display_screen();
}

/* Waveform Palette Dropdown Callback */
static void dd_palette_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    waveform_palette_idx = lv_dropdown_get_selected(dd);
    const char * palettes[] = {"Standard (Cyan/Yellow/Pink)", "High Contrast Neon", "Monochromatic Blue"};
    char buf[128];
    snprintf(buf, sizeof(buf), "Waveform color palette set to %s.", palettes[waveform_palette_idx]);
    show_toast_message(buf, false);
}

/* UI Scale Dropdown Callback */
static void dd_scale_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    ui_scale_idx = lv_dropdown_get_selected(dd);
    const char * scales[] = {"100% Standard", "115% Medium", "130% Large Text"};
    char buf[128];
    snprintf(buf, sizeof(buf), "UI display scale set to %s.", scales[ui_scale_idx]);
    show_toast_message(buf, false);
}

/* Screen Orientation Dropdown Callback */
static void dd_orientation_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    orientation_idx = lv_dropdown_get_selected(dd);
    const char * orientations[] = {"Landscape Standard (0°)", "Landscape Inverted (180°)"};
    char buf[128];
    snprintf(buf, sizeof(buf), "Screen orientation set to %s.", orientations[orientation_idx]);
    show_toast_message(buf, false);
}

/* Touch Sound Switch Callback */
static void sw_touch_sound_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    touch_sound_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    audio_set_touch_enabled(touch_sound_enabled);
    if(touch_sound_enabled) {
        show_toast_message("Touch sound & click audio feedback enabled.", false);
        audio_play_touch_sound();
    } else {
        show_toast_message("Touch sound feedback muted.", false);
    }
}

/* FPS Mode Dropdown Callback */
static void dd_fps_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    fps_mode_idx = lv_dropdown_get_selected(dd);
    const char * modes[] = {"60 FPS (High Smoothness)", "30 FPS (Power Saver)"};
    char buf[128];
    snprintf(buf, sizeof(buf), "Display refresh rate set to %s.", modes[fps_mode_idx]);
    show_toast_message(buf, false);
}

/* Touch Calibration Button Callback */
static void btn_calibrate_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    show_toast_message("Touch screen calibration process initiated...", false);
}

/* Save & Apply Settings Callback */
static void btn_save_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    show_toast_message("Display settings saved and applied successfully!", false);
}

/* Reset Defaults Callback */
static void btn_reset_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    brightness_set_level(80);
    brightness_set_auto(true);
    brightness_val = brightness_get_level();
    auto_brightness_enabled = brightness_is_auto();
    night_dimming_enabled = true;
    screen_timeout_idx = 2;
    screensaver_mode_idx = 1;
    sleep_dim_level = 20;
    theme_mode_idx = 0;
    waveform_palette_idx = 0;
    ui_scale_idx = 0;
    orientation_idx = 0;
    touch_sound_enabled = true;
    fps_mode_idx = 0;

    if(slider_brightness) lv_slider_set_value(slider_brightness, brightness_val, LV_ANIM_ON);
    if(lbl_brightness_val) lv_label_set_text_fmt(lbl_brightness_val, "%d%%", brightness_val);
    if(sw_auto_bright) {
        if(auto_brightness_enabled) lv_obj_add_state(sw_auto_bright, LV_STATE_CHECKED);
        else lv_obj_remove_state(sw_auto_bright, LV_STATE_CHECKED);
    }
    if(sw_night_dim) {
        if(night_dimming_enabled) lv_obj_add_state(sw_night_dim, LV_STATE_CHECKED);
        else lv_obj_remove_state(sw_night_dim, LV_STATE_CHECKED);
    }
    update_preset_buttons_ui();

    if(dd_timeout) lv_dropdown_set_selected(dd_timeout, screen_timeout_idx);
    update_saver_buttons_ui();
    if(slider_sleep_dim) lv_slider_set_value(slider_sleep_dim, sleep_dim_level, LV_ANIM_ON);
    if(lbl_sleep_dim_val) lv_label_set_text_fmt(lbl_sleep_dim_val, "%d%%", sleep_dim_level);

    update_theme_cards_ui();
    if(dd_palette) lv_dropdown_set_selected(dd_palette, waveform_palette_idx);
    if(dd_scale) lv_dropdown_set_selected(dd_scale, ui_scale_idx);
    if(dd_orientation) lv_dropdown_set_selected(dd_orientation, orientation_idx);
    if(sw_touch_sound) {
        if(touch_sound_enabled) lv_obj_add_state(sw_touch_sound, LV_STATE_CHECKED);
        else lv_obj_remove_state(sw_touch_sound, LV_STATE_CHECKED);
    }
    if(dd_fps) lv_dropdown_set_selected(dd_fps, fps_mode_idx);

    show_toast_message("Display settings reset to factory defaults.", false);
}

/**
 * @brief Create and render the Display Settings screen (1280x800).
 */
void create_ventilator_display_screen(void)
{
    /* Clean up old clock & toast timers */
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }

    /* Base Screen Setup */
    main_screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen_obj, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(main_screen_obj, LV_OPA_COVER, 0);

    /* ==================================================================== */
    /* 1. TOP HEADER BAR                                                    */
    /* ==================================================================== */
    lv_obj_t * top_bar = lv_obj_create(main_screen_obj);
    lv_obj_set_size(top_bar, 1280, 55);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, COLOR_PANEL_HEADER, 0);
    lv_obj_set_style_border_color(top_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_hor(top_bar, 12, 0);

    /* DISPLAY SETTINGS Badge (Left) */
    lv_obj_t * mode_box = lv_obj_create(top_bar);
    lv_obj_set_size(mode_box, 250, 42);
    lv_obj_align(mode_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(mode_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(mode_box, 0, 0);
    lv_obj_set_style_radius(mode_box, 6, 0);
    lv_obj_set_style_pad_all(mode_box, 4, 0);

    lv_obj_t * icon_box = lv_obj_create(mode_box);
    lv_obj_set_size(icon_box, 34, 34);
    lv_obj_align(icon_box, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(icon_box, lv_color_hex(0x007CFF), 0);
    lv_obj_set_style_radius(icon_box, 6, 0);
    lv_obj_set_style_border_width(icon_box, 0, 0);

    lv_obj_t * icon_lbl = lv_label_create(icon_box);
    lv_label_set_text(icon_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(icon_lbl, lv_color_white(), 0);
    lv_obj_center(icon_lbl);

    lv_obj_t * mode_title = lv_label_create(mode_box);
    lv_label_set_text(mode_title, "DISPLAY SETTINGS");
    lv_obj_set_style_text_font(mode_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mode_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(mode_title, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * mode_sub = lv_label_create(mode_box);
    lv_label_set_text(mode_sub, "Brightness, theme & layout");
    lv_obj_set_style_text_font(mode_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_sub, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(mode_sub, LV_ALIGN_LEFT_MID, 46, 10);

    /* Patient ID Badge (Middle Left) */
    lv_obj_t * pat_box = lv_obj_create(top_bar);
    lv_obj_set_size(pat_box, 230, 42);
    lv_obj_align(pat_box, LV_ALIGN_LEFT_MID, 262, 0);
    lv_obj_set_style_bg_color(pat_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(pat_box, 0, 0);
    lv_obj_set_style_radius(pat_box, 6, 0);
    lv_obj_add_flag(pat_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pat_box, pat_badge_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * pat_lbl = lv_label_create(pat_box);
    lv_label_set_text(pat_lbl, LV_SYMBOL_DIRECTORY " John Doe\n#7097ba ID: 12345678 | Male | 45 yrs | 70 kg#");
    lv_label_set_recolor(pat_lbl, true);
    lv_obj_set_style_text_font(pat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pat_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(pat_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* Critical Alarms Banner (Middle) */
    lv_obj_t * alarm_box = lv_obj_create(top_bar);
    lv_obj_set_size(alarm_box, 320, 42);
    lv_obj_align(alarm_box, LV_ALIGN_CENTER, 80, 0);
    lv_obj_set_style_bg_color(alarm_box, lv_color_hex(0x5A0C0C), 0);
    lv_obj_set_style_border_width(alarm_box, 0, 0);
    lv_obj_set_style_radius(alarm_box, 6, 0);

    lv_obj_t * alarm_lbl = lv_label_create(alarm_box);
    lv_label_set_text(alarm_lbl, LV_SYMBOL_BELL "  2 CRITICAL ALARMS\n   Require immediate attention");
    lv_obj_set_style_text_font(alarm_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(alarm_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * alarm_arrow = lv_label_create(alarm_box);
    lv_label_set_text(alarm_arrow, ">");
    lv_obj_set_style_text_font(alarm_arrow, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_arrow, COLOR_TEXT_MAIN, 0);
    lv_obj_align(alarm_arrow, LV_ALIGN_RIGHT_MID, -10, 0);

    /* Date & Time Clock Box (Middle Right) */
    lv_obj_t * date_box = lv_obj_create(top_bar);
    lv_obj_set_size(date_box, 150, 42);
    lv_obj_align(date_box, LV_ALIGN_RIGHT_MID, -130, 0);
    lv_obj_set_style_bg_color(date_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(date_box, 0, 0);
    lv_obj_set_style_radius(date_box, 6, 0);
    lv_obj_add_flag(date_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(date_box, date_box_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * date_icon = lv_label_create(date_box);
    lv_label_set_text(date_icon, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(date_icon, COLOR_TEXT_MUTED, 0);
    lv_obj_align(date_icon, LV_ALIGN_LEFT_MID, 10, 0);

    lbl_clock = lv_label_create(date_box);
    lv_label_set_text(lbl_clock, "20 May 2024\n10:24 AM");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 30, 0);

    /* Battery Indicator Box (Right) */
    lv_obj_t * right_hdr = lv_obj_create(top_bar);
    lv_obj_set_size(right_hdr, 120, 42);
    lv_obj_align(right_hdr, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(right_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_hdr, 0, 0);

    bat_lbl = lv_label_create(right_hdr);
    battery_update_label(bat_lbl);
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(bat_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * set_icon = lv_label_create(right_hdr);
    lv_label_set_text(set_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(set_icon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(set_icon, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(set_icon, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Toast Notification Banner (Floating top center) */
    toast_banner = lv_obj_create(main_screen_obj);
    lv_obj_set_size(toast_banner, 600, 40);
    lv_obj_align(toast_banner, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(toast_banner, lv_color_hex(0x0D6436), 0);
    lv_obj_set_style_border_color(toast_banner, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_border_width(toast_banner, 1, 0);
    lv_obj_set_style_radius(toast_banner, 8, 0);
    lv_obj_add_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);

    lbl_toast_msg = lv_label_create(toast_banner);
    lv_label_set_text(lbl_toast_msg, "");
    lv_obj_set_style_text_font(lbl_toast_msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_toast_msg, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_toast_msg);

    /* ==================================================================== */
    /* 2. MAIN CONTENT GRID - 4 SETTING PANELS                              */
    /* ==================================================================== */

    /* -------------------------------------------------------------------- */
    /* PANEL 1: SCREEN BRIGHTNESS & DIMMING (Col 0, Row 0: x=20, y=68, w=605, h=325) */
    /* -------------------------------------------------------------------- */
    lv_obj_t * p1 = lv_obj_create(main_screen_obj);
    lv_obj_set_size(p1, 605, 325);
    lv_obj_set_pos(p1, 20, 68);
    lv_obj_set_style_bg_color(p1, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(p1, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(p1, 1, 0);
    lv_obj_set_style_radius(p1, 12, 0);
    lv_obj_set_style_pad_all(p1, 16, 0);

    brightness_val = brightness_get_level();
    auto_brightness_enabled = brightness_is_auto();

    /* Header */
    lv_obj_t * p1_hdr = lv_label_create(p1);
    lv_label_set_text(p1_hdr, LV_SYMBOL_IMAGE "  SCREEN BRIGHTNESS & DIMMING");
    lv_obj_set_style_text_font(p1_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(p1_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(p1_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Brightness Slider Label & Value */
    lv_obj_t * lbl_b_title = lv_label_create(p1);
    lv_label_set_text(lbl_b_title, "Brightness Level");
    lv_obj_set_style_text_font(lbl_b_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_b_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_b_title, LV_ALIGN_TOP_LEFT, 0, 35);

    lbl_brightness_val = lv_label_create(p1);
    lv_label_set_text_fmt(lbl_brightness_val, "%d%%", brightness_val);
    lv_obj_set_style_text_font(lbl_brightness_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_brightness_val, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(lbl_brightness_val, LV_ALIGN_TOP_RIGHT, 0, 35);

    /* Decrement (-) Button */
    lv_obj_t * btn_dec_b = lv_button_create(p1);
    lv_obj_set_size(btn_dec_b, 40, 34);
    lv_obj_align(btn_dec_b, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_set_style_bg_color(btn_dec_b, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_dec_b, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_dec_b, 1, 0);
    lv_obj_set_style_radius(btn_dec_b, 6, 0);

    lv_obj_t * lbl_dec = lv_label_create(btn_dec_b);
    lv_label_set_text(lbl_dec, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lbl_dec, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_dec, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_dec);
    lv_obj_add_event_cb(btn_dec_b, brightness_dec_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Brightness Slider */
    slider_brightness = lv_slider_create(p1);
    lv_obj_set_size(slider_brightness, 474, 16);
    lv_obj_align(slider_brightness, LV_ALIGN_TOP_LEFT, 48, 69);
    lv_slider_set_range(slider_brightness, 10, 100);
    lv_slider_set_value(slider_brightness, brightness_val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_brightness, lv_color_hex(0x061D3B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_brightness, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_brightness, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_brightness, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_brightness, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Increment (+) Button */
    lv_obj_t * btn_inc_b = lv_button_create(p1);
    lv_obj_set_size(btn_inc_b, 40, 34);
    lv_obj_align(btn_inc_b, LV_ALIGN_TOP_LEFT, 530, 60);
    lv_obj_set_style_bg_color(btn_inc_b, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_inc_b, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_inc_b, 1, 0);
    lv_obj_set_style_radius(btn_inc_b, 6, 0);

    lv_obj_t * lbl_inc = lv_label_create(btn_inc_b);
    lv_label_set_text(lbl_inc, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(lbl_inc, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_inc, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_inc);
    lv_obj_add_event_cb(btn_inc_b, brightness_inc_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Quick Preset Buttons Container */
    lv_obj_t * presets_cont = lv_obj_create(p1);
    lv_obj_set_size(presets_cont, 570, 42);
    lv_obj_align(presets_cont, LV_ALIGN_TOP_LEFT, 0, 98);
    lv_obj_set_style_bg_opa(presets_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(presets_cont, 0, 0);
    lv_obj_set_style_pad_all(presets_cont, 0, 0);
    lv_obj_set_flex_flow(presets_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(presets_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * preset_labels[4] = {"25%", "50%", "75%", "100%"};
    int preset_vals[4] = {25, 50, 75, 100};
    for(int i = 0; i < 4; i++) {
        preset_btns[i] = lv_button_create(presets_cont);
        lv_obj_set_size(preset_btns[i], 132, 38);
        lv_obj_set_style_bg_color(preset_btns[i], COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(preset_btns[i], COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(preset_btns[i], 1, 0);
        lv_obj_set_style_radius(preset_btns[i], 6, 0);

        lv_obj_t * p_lbl = lv_label_create(preset_btns[i]);
        lv_label_set_text(p_lbl, preset_labels[i]);
        lv_obj_set_style_text_font(p_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(p_lbl, COLOR_TEXT_MAIN, 0);
        lv_obj_center(p_lbl);

        lv_obj_add_event_cb(preset_btns[i], preset_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)preset_vals[i]);
    }
    update_preset_buttons_ui();

    /* Separator line */
    lv_obj_t * line_p1 = lv_obj_create(p1);
    lv_obj_set_size(line_p1, 570, 1);
    lv_obj_align(line_p1, LV_ALIGN_TOP_LEFT, 0, 152);
    lv_obj_set_style_bg_color(line_p1, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(line_p1, 0, 0);

    /* Auto-brightness Toggle */
    lv_obj_t * lbl_auto_bright = lv_label_create(p1);
    lv_label_set_text(lbl_auto_bright, "Ambient Light Auto-Adjust\n#7097ba Automatically adjusts brightness based on room sensor#");
    lv_label_set_recolor(lbl_auto_bright, true);
    lv_obj_set_style_text_font(lbl_auto_bright, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_auto_bright, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_auto_bright, LV_ALIGN_TOP_LEFT, 0, 162);

    sw_auto_bright = lv_switch_create(p1);
    lv_obj_set_size(sw_auto_bright, 54, 28);
    lv_obj_align(sw_auto_bright, LV_ALIGN_TOP_RIGHT, 0, 165);
    if(auto_brightness_enabled) lv_obj_add_state(sw_auto_bright, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw_auto_bright, COLOR_ACCENT_BLUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_auto_bright, sw_auto_bright_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Night Mode Toggle */
    lv_obj_t * lbl_night_dim = lv_label_create(p1);
    lv_label_set_text(lbl_night_dim, "Night Auto-Dimming (10:00 PM - 06:00 AM)\n#7097ba Lower display glare during nighttime monitoring#");
    lv_label_set_recolor(lbl_night_dim, true);
    lv_obj_set_style_text_font(lbl_night_dim, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_night_dim, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_night_dim, LV_ALIGN_TOP_LEFT, 0, 222);

    sw_night_dim = lv_switch_create(p1);
    lv_obj_set_size(sw_night_dim, 54, 28);
    lv_obj_align(sw_night_dim, LV_ALIGN_TOP_RIGHT, 0, 225);
    if(night_dimming_enabled) lv_obj_add_state(sw_night_dim, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw_night_dim, COLOR_ACCENT_BLUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_night_dim, sw_night_dim_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* -------------------------------------------------------------------- */
    /* PANEL 2: SCREEN TIMEOUT & SAVER (Col 0, Row 1: x=20, y=403, w=605, h=325) */
    /* -------------------------------------------------------------------- */
    lv_obj_t * p2 = lv_obj_create(main_screen_obj);
    lv_obj_set_size(p2, 605, 325);
    lv_obj_set_pos(p2, 20, 403);
    lv_obj_set_style_bg_color(p2, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(p2, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(p2, 1, 0);
    lv_obj_set_style_radius(p2, 12, 0);
    lv_obj_set_style_pad_all(p2, 16, 0);

    /* Header */
    lv_obj_t * p2_hdr = lv_label_create(p2);
    lv_label_set_text(p2_hdr, LV_SYMBOL_LIST "  SCREEN TIMEOUT & SAVER");
    lv_obj_set_style_text_font(p2_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(p2_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(p2_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Screen Timeout Dropdown */
    lv_obj_t * lbl_t_title = lv_label_create(p2);
    lv_label_set_text(lbl_t_title, "Screen Timeout\n#7097ba Inactivity delay before screen dimming#");
    lv_label_set_recolor(lbl_t_title, true);
    lv_obj_set_style_text_font(lbl_t_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_t_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_t_title, LV_ALIGN_TOP_LEFT, 0, 32);

    dd_timeout = lv_dropdown_create(p2);
    lv_dropdown_set_options(dd_timeout, "Never (Always On)\n1 Minute\n5 Minutes\n15 Minutes\n30 Minutes");
    lv_dropdown_set_selected(dd_timeout, screen_timeout_idx);
    lv_obj_set_size(dd_timeout, 220, 38);
    lv_obj_align(dd_timeout, LV_ALIGN_TOP_RIGHT, 0, 32);
    lv_obj_set_style_bg_color(dd_timeout, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(dd_timeout, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(dd_timeout, COLOR_TEXT_MAIN, 0);
    lv_obj_add_event_cb(dd_timeout, dd_timeout_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Separator line */
    lv_obj_t * line_p2_1 = lv_obj_create(p2);
    lv_obj_set_size(line_p2_1, 570, 1);
    lv_obj_align(line_p2_1, LV_ALIGN_TOP_LEFT, 0, 85);
    lv_obj_set_style_bg_color(line_p2_1, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(line_p2_1, 0, 0);

    /* Screensaver Mode Selection */
    lv_obj_t * lbl_saver_title = lv_label_create(p2);
    lv_label_set_text(lbl_saver_title, "Screensaver Mode");
    lv_obj_set_style_text_font(lbl_saver_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_saver_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_saver_title, LV_ALIGN_TOP_LEFT, 0, 95);

    lv_obj_t * saver_cont = lv_obj_create(p2);
    lv_obj_set_size(saver_cont, 570, 42);
    lv_obj_align(saver_cont, LV_ALIGN_TOP_LEFT, 0, 122);
    lv_obj_set_style_bg_opa(saver_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(saver_cont, 0, 0);
    lv_obj_set_style_pad_all(saver_cont, 0, 0);
    lv_obj_set_flex_flow(saver_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(saver_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * saver_labels[3] = {"Disabled", "Clock & Status", "Blank Screen"};
    for(int i = 0; i < 3; i++) {
        btn_saver_modes[i] = lv_button_create(saver_cont);
        lv_obj_set_size(btn_saver_modes[i], 180, 38);
        lv_obj_set_style_bg_color(btn_saver_modes[i], COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(btn_saver_modes[i], COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(btn_saver_modes[i], 1, 0);
        lv_obj_set_style_radius(btn_saver_modes[i], 6, 0);

        lv_obj_t * s_lbl = lv_label_create(btn_saver_modes[i]);
        lv_label_set_text(s_lbl, saver_labels[i]);
        lv_obj_set_style_text_font(s_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_lbl, COLOR_TEXT_MAIN, 0);
        lv_obj_center(s_lbl);

        lv_obj_add_event_cb(btn_saver_modes[i], btn_saver_mode_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    update_saver_buttons_ui();

    /* Separator line */
    lv_obj_t * line_p2_2 = lv_obj_create(p2);
    lv_obj_set_size(line_p2_2, 570, 1);
    lv_obj_align(line_p2_2, LV_ALIGN_TOP_LEFT, 0, 175);
    lv_obj_set_style_bg_color(line_p2_2, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(line_p2_2, 0, 0);

    /* Sleep Mode Dimming Level Slider */
    lv_obj_t * lbl_sleep_title = lv_label_create(p2);
    lv_label_set_text(lbl_sleep_title, "Standby Dimming Level");
    lv_obj_set_style_text_font(lbl_sleep_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_sleep_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_sleep_title, LV_ALIGN_TOP_LEFT, 0, 185);

    lbl_sleep_dim_val = lv_label_create(p2);
    lv_label_set_text_fmt(lbl_sleep_dim_val, "%d%%", sleep_dim_level);
    lv_obj_set_style_text_font(lbl_sleep_dim_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sleep_dim_val, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(lbl_sleep_dim_val, LV_ALIGN_TOP_RIGHT, 0, 185);

    slider_sleep_dim = lv_slider_create(p2);
    lv_obj_set_size(slider_sleep_dim, 570, 16);
    lv_obj_align(slider_sleep_dim, LV_ALIGN_TOP_LEFT, 0, 218);
    lv_slider_set_range(slider_sleep_dim, 10, 50);
    lv_slider_set_value(slider_sleep_dim, sleep_dim_level, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_sleep_dim, lv_color_hex(0x061D3B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_sleep_dim, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_sleep_dim, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_sleep_dim, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_sleep_dim, slider_sleep_dim_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* -------------------------------------------------------------------- */
    /* PANEL 3: COLOR THEME & VISUAL MODE (Col 1, Row 0: x=655, y=68, w=605, h=325) */
    /* -------------------------------------------------------------------- */
    lv_obj_t * p3 = lv_obj_create(main_screen_obj);
    lv_obj_set_size(p3, 605, 325);
    lv_obj_set_pos(p3, 655, 68);
    lv_obj_set_style_bg_color(p3, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(p3, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(p3, 1, 0);
    lv_obj_set_style_radius(p3, 12, 0);
    lv_obj_set_style_pad_all(p3, 16, 0);

    /* Header */
    lv_obj_t * p3_hdr = lv_label_create(p3);
    lv_label_set_text(p3_hdr, LV_SYMBOL_EDIT "  COLOR THEME & VISUAL MODE");
    lv_obj_set_style_text_font(p3_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(p3_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(p3_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Theme Mode Cards (3 side-by-side cards) */
    lv_obj_t * themes_cont = lv_obj_create(p3);
    lv_obj_set_size(themes_cont, 570, 115);
    lv_obj_align(themes_cont, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_style_bg_opa(themes_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(themes_cont, 0, 0);
    lv_obj_set_style_pad_all(themes_cont, 0, 0);
    lv_obj_set_flex_flow(themes_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(themes_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * theme_titles[3] = {"Dark Medical", "Light Mode", "Night Vision"};
    const char * theme_descs[3] = {"High Contrast\nICU Dark Mode", "Clean Bright\nDaytime Mode", "Low Intensity\nRed/Green Tint"};

    for(int i = 0; i < 3; i++) {
        theme_cards[i] = lv_obj_create(themes_cont);
        lv_obj_set_size(theme_cards[i], 180, 110);
        lv_obj_set_style_bg_color(theme_cards[i], COLOR_CARD_BG, 0);
        lv_obj_set_style_border_color(theme_cards[i], COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(theme_cards[i], 1, 0);
        lv_obj_set_style_radius(theme_cards[i], 8, 0);
        lv_obj_set_style_pad_all(theme_cards[i], 8, 0);
        lv_obj_add_flag(theme_cards[i], LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t * t_icon = lv_label_create(theme_cards[i]);
        if(i == 0) lv_label_set_text(t_icon, LV_SYMBOL_EYE_OPEN);
        else if(i == 1) lv_label_set_text(t_icon, LV_SYMBOL_IMAGE);
        else lv_label_set_text(t_icon, LV_SYMBOL_POWER);
        lv_obj_set_style_text_font(t_icon, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(t_icon, COLOR_ACCENT_BLUE, 0);
        lv_obj_align(t_icon, LV_ALIGN_TOP_LEFT, 6, 4);

        lv_obj_t * t_name = lv_label_create(theme_cards[i]);
        lv_label_set_text(t_name, theme_titles[i]);
        lv_obj_set_style_text_font(t_name, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(t_name, COLOR_TEXT_MAIN, 0);
        lv_obj_align(t_name, LV_ALIGN_TOP_LEFT, 6, 32);

        lv_obj_t * t_desc = lv_label_create(theme_cards[i]);
        lv_label_set_text(t_desc, theme_descs[i]);
        lv_obj_set_style_text_font(t_desc, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(t_desc, COLOR_TEXT_MUTED, 0);
        lv_obj_align(t_desc, LV_ALIGN_TOP_LEFT, 6, 56);

        lv_obj_add_event_cb(theme_cards[i], theme_card_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    update_theme_cards_ui();

    /* Separator line */
    lv_obj_t * line_p3 = lv_obj_create(p3);
    lv_obj_set_size(line_p3, 570, 1);
    lv_obj_align(line_p3, LV_ALIGN_TOP_LEFT, 0, 155);
    lv_obj_set_style_bg_color(line_p3, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(line_p3, 0, 0);

    /* Waveform Color Palette Dropdown */
    lv_obj_t * lbl_p_title = lv_label_create(p3);
    lv_label_set_text(lbl_p_title, "Waveform Color Palette\n#7097ba Color scheme for real-time pressure & flow graphs#");
    lv_label_set_recolor(lbl_p_title, true);
    lv_obj_set_style_text_font(lbl_p_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_p_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_p_title, LV_ALIGN_TOP_LEFT, 0, 168);

    dd_palette = lv_dropdown_create(p3);
    lv_dropdown_set_options(dd_palette, "Standard (Cyan/Yellow/Pink)\nHigh Contrast Neon\nMonochromatic Blue");
    lv_dropdown_set_selected(dd_palette, waveform_palette_idx);
    lv_obj_set_size(dd_palette, 240, 38);
    lv_obj_align(dd_palette, LV_ALIGN_TOP_RIGHT, 0, 168);
    lv_obj_set_style_bg_color(dd_palette, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(dd_palette, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(dd_palette, COLOR_TEXT_MAIN, 0);
    lv_obj_add_event_cb(dd_palette, dd_palette_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* -------------------------------------------------------------------- */
    /* PANEL 4: LAYOUT, SCALING & FEEDBACK (Col 1, Row 1: x=655, y=403, w=605, h=325) */
    /* -------------------------------------------------------------------- */
    lv_obj_t * p4 = lv_obj_create(main_screen_obj);
    lv_obj_set_size(p4, 605, 325);
    lv_obj_set_pos(p4, 655, 403);
    lv_obj_set_style_bg_color(p4, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(p4, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(p4, 1, 0);
    lv_obj_set_style_radius(p4, 12, 0);
    lv_obj_set_style_pad_all(p4, 16, 0);

    /* Header */
    lv_obj_t * p4_hdr = lv_label_create(p4);
    lv_label_set_text(p4_hdr, LV_SYMBOL_SETTINGS "  LAYOUT, SCALING & FEEDBACK");
    lv_obj_set_style_text_font(p4_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(p4_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(p4_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* UI Scale Dropdown */
    lv_obj_t * lbl_scale_title = lv_label_create(p4);
    lv_label_set_text(lbl_scale_title, "UI Display Scale");
    lv_obj_set_style_text_font(lbl_scale_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_scale_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_scale_title, LV_ALIGN_TOP_LEFT, 0, 30);

    dd_scale = lv_dropdown_create(p4);
    lv_dropdown_set_options(dd_scale, "100% Standard\n115% Medium\n130% Large Text");
    lv_dropdown_set_selected(dd_scale, ui_scale_idx);
    lv_obj_set_size(dd_scale, 200, 36);
    lv_obj_align(dd_scale, LV_ALIGN_TOP_RIGHT, 0, 25);
    lv_obj_set_style_bg_color(dd_scale, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(dd_scale, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(dd_scale, COLOR_TEXT_MAIN, 0);
    lv_obj_add_event_cb(dd_scale, dd_scale_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Orientation Dropdown */
    lv_obj_t * lbl_ori_title = lv_label_create(p4);
    lv_label_set_text(lbl_ori_title, "Screen Orientation");
    lv_obj_set_style_text_font(lbl_ori_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ori_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_ori_title, LV_ALIGN_TOP_LEFT, 0, 75);

    dd_orientation = lv_dropdown_create(p4);
    lv_dropdown_set_options(dd_orientation, "Landscape Standard (0°)\nLandscape Inverted (180°)");
    lv_dropdown_set_selected(dd_orientation, orientation_idx);
    lv_obj_set_size(dd_orientation, 220, 36);
    lv_obj_align(dd_orientation, LV_ALIGN_TOP_RIGHT, 0, 70);
    lv_obj_set_style_bg_color(dd_orientation, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(dd_orientation, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(dd_orientation, COLOR_TEXT_MAIN, 0);
    lv_obj_add_event_cb(dd_orientation, dd_orientation_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* FPS Refresh Rate Dropdown */
    lv_obj_t * lbl_fps_title = lv_label_create(p4);
    lv_label_set_text(lbl_fps_title, "Display Refresh Rate");
    lv_obj_set_style_text_font(lbl_fps_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_fps_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_fps_title, LV_ALIGN_TOP_LEFT, 0, 120);

    dd_fps = lv_dropdown_create(p4);
    lv_dropdown_set_options(dd_fps, "60 FPS (Smooth)\n30 FPS (Power Saver)");
    lv_dropdown_set_selected(dd_fps, fps_mode_idx);
    lv_obj_set_size(dd_fps, 200, 36);
    lv_obj_align(dd_fps, LV_ALIGN_TOP_RIGHT, 0, 115);
    lv_obj_set_style_bg_color(dd_fps, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(dd_fps, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(dd_fps, COLOR_TEXT_MAIN, 0);
    lv_obj_add_event_cb(dd_fps, dd_fps_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Separator line */
    lv_obj_t * line_p4 = lv_obj_create(p4);
    lv_obj_set_size(line_p4, 570, 1);
    lv_obj_align(line_p4, LV_ALIGN_TOP_LEFT, 0, 160);
    lv_obj_set_style_bg_color(line_p4, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(line_p4, 0, 0);

    /* Touch Sound Toggle */
    lv_obj_t * lbl_ts_title = lv_label_create(p4);
    lv_label_set_text(lbl_ts_title, "Touch Audio Feedback\n#7097ba Play subtle click beep on touch screen input#");
    lv_label_set_recolor(lbl_ts_title, true);
    lv_obj_set_style_text_font(lbl_ts_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ts_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_ts_title, LV_ALIGN_TOP_LEFT, 0, 170);

    sw_touch_sound = lv_switch_create(p4);
    lv_obj_set_size(sw_touch_sound, 54, 28);
    lv_obj_align(sw_touch_sound, LV_ALIGN_TOP_RIGHT, 0, 172);
    if(touch_sound_enabled) lv_obj_add_state(sw_touch_sound, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw_touch_sound, COLOR_ACCENT_BLUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_touch_sound, sw_touch_sound_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Touch Calibration Action Button */
    lv_obj_t * btn_cal = lv_button_create(p4);
    lv_obj_set_size(btn_cal, 240, 36);
    lv_obj_align(btn_cal, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_cal, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_cal, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(btn_cal, 1, 0);
    lv_obj_set_style_radius(btn_cal, 6, 0);

    lv_obj_t * lbl_c_btn = lv_label_create(btn_cal);
    lv_label_set_text(lbl_c_btn, LV_SYMBOL_REFRESH "  Calibrate Touch Screen");
    lv_obj_set_style_text_font(lbl_c_btn, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_c_btn, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(lbl_c_btn);
    lv_obj_add_event_cb(btn_cal, btn_calibrate_cb, LV_EVENT_CLICKED, NULL);

    /* ==================================================================== */
    /* 3. BOTTOM FOOTER NAVIGATION BAR                                      */
    /* ==================================================================== */
    lv_obj_t * bot_bar = lv_obj_create(main_screen_obj);
    lv_obj_set_size(bot_bar, 1260, 55);
    lv_obj_set_pos(bot_bar, 10, 740);
    lv_obj_set_style_bg_opa(bot_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bot_bar, 0, 0);
    lv_obj_set_style_pad_all(bot_bar, 0, 0);

    /* BACK TO SETTINGS Button */
    lv_obj_t * back_btn = lv_button_create(bot_bar);
    lv_obj_set_size(back_btn, 210, 48);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(back_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(back_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);

    lv_obj_t * back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT "  BACK TO SETTINGS");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, back_to_settings_cb, LV_EVENT_CLICKED, NULL);

    /* HOME Button */
    lv_obj_t * home_btn = lv_button_create(bot_bar);
    lv_obj_set_size(home_btn, 140, 48);
    lv_obj_align(home_btn, LV_ALIGN_LEFT_MID, 225, 0);
    lv_obj_set_style_bg_color(home_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(home_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(home_btn, 1, 0);
    lv_obj_set_style_radius(home_btn, 8, 0);

    lv_obj_t * home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, LV_SYMBOL_HOME "  HOME");
    lv_obj_set_style_text_font(home_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(home_lbl);
    lv_obj_add_event_cb(home_btn, home_btn_cb, LV_EVENT_CLICKED, NULL);

    /* RESET DEFAULTS Button */
    lv_obj_t * reset_btn = lv_button_create(bot_bar);
    lv_obj_set_size(reset_btn, 180, 48);
    lv_obj_align(reset_btn, LV_ALIGN_RIGHT_MID, -210, 0);
    lv_obj_set_style_bg_color(reset_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(reset_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(reset_btn, 1, 0);
    lv_obj_set_style_radius(reset_btn, 8, 0);

    lv_obj_t * reset_lbl = lv_label_create(reset_btn);
    lv_label_set_text(reset_lbl, LV_SYMBOL_REFRESH "  RESET DEFAULTS");
    lv_obj_set_style_text_font(reset_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(reset_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_center(reset_lbl);
    lv_obj_add_event_cb(reset_btn, btn_reset_cb, LV_EVENT_CLICKED, NULL);

    /* SAVE & APPLY Button */
    lv_obj_t * save_btn = lv_button_create(bot_bar);
    lv_obj_set_size(save_btn, 190, 48);
    lv_obj_align(save_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x008044), 0);
    lv_obj_set_style_border_color(save_btn, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_border_width(save_btn, 1, 0);
    lv_obj_set_style_radius(save_btn, 8, 0);

    lv_obj_t * save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, LV_SYMBOL_OK "  SAVE & APPLY");
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(save_lbl, lv_color_white(), 0);
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, btn_save_cb, LV_EVENT_CLICKED, NULL);

    /* Disable scrolling tree filter */
    disable_scroll_recursive(main_screen_obj);

    /* Clock dynamic sync timer */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);

    /* Load Display Settings Screen */
    lv_screen_load_anim(main_screen_obj, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
