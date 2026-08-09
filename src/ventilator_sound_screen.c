#include "ventilator_sound_screen.h"
#include "ventilator_settings_screen.h"
#include "ventilator_main_screen.h"
#include "ventilator_time_screen.h"
#include "battery_detect.h"
#include "theme_manager.h"
#include "audio_manager.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static lv_timer_t * clock_timer = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * bat_lbl = NULL;

/* Widgets for live updates */
static lv_obj_t * sw_ecg_beep = NULL;
static lv_obj_t * slider_ecg_vol = NULL;
static lv_obj_t * lbl_ecg_vol_val = NULL;

static lv_obj_t * slider_master_vol = NULL;
static lv_obj_t * lbl_master_vol_val = NULL;

static lv_obj_t * slider_alarm_vol = NULL;
static lv_obj_t * lbl_alarm_vol_val = NULL;

static lv_obj_t * sw_touch_sound = NULL;
static lv_obj_t * slider_touch_vol = NULL;
static lv_obj_t * lbl_touch_vol_val = NULL;

/* Clock update callback */
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

/* Helper toast notification */
static void show_toast(const char * msg)
{
    lv_obj_t * scr = lv_screen_active();
    if(!scr) return;

    lv_obj_t * toast = lv_obj_create(scr);
    lv_obj_set_size(toast, 420, 48);
    lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(toast, lv_color_hex(0x0F2E4A), 0);
    lv_obj_set_style_border_color(toast, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(toast, 2, 0);
    lv_obj_set_style_radius(toast, 24, 0);

    lv_obj_t * lbl = lv_label_create(toast);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, toast);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, 800);
    lv_anim_set_delay(&a, 1500);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_completed_cb(&a, (lv_anim_completed_cb_t)lv_obj_delete);
    lv_anim_start(&a);
}

/* Event Callbacks */
static void back_to_settings_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    create_ventilator_settings_screen();
}

static void back_to_home_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    create_ventilator_main_screen();
}

static void sw_ecg_beep_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    audio_set_ecg_enabled(en);
    if(en) {
        show_toast(LV_SYMBOL_AUDIO " ECG Wave Spike Beep Enabled");
        audio_play_ecg_beep();
    } else {
        show_toast(LV_SYMBOL_MUTE " ECG Wave Spike Beep Muted");
    }
}

static void slider_ecg_vol_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    audio_set_ecg_volume(val);
    if(lbl_ecg_vol_val) {
        lv_label_set_text_fmt(lbl_ecg_vol_val, "%d%%", val);
    }
}

static void test_ecg_beep_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    show_toast(LV_SYMBOL_VOLUME_MAX " Playing ECG Beep Sound Sample...");
    audio_play_ecg_beep();
}

static void slider_master_vol_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    audio_set_master_volume(val);
    if(lbl_master_vol_val) {
        lv_label_set_text_fmt(lbl_master_vol_val, "%d%%", val);
    }
}

static void slider_alarm_vol_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    audio_set_alarm_volume(val);
    if(lbl_alarm_vol_val) {
        lv_label_set_text_fmt(lbl_alarm_vol_val, "%d%%", val);
    }
}

static void test_alarm_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    show_toast(LV_SYMBOL_BELL " Playing Alarm Sound Test...");
    audio_play_alarm_sound();
}

static void sw_touch_sound_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    audio_set_touch_enabled(en);
    if(en) {
        show_toast("Touch Click Audio Feedback Enabled");
        audio_play_touch_sound();
    } else {
        show_toast("Touch Click Audio Feedback Muted");
    }
}

static void slider_touch_vol_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    audio_set_touch_volume(val);
    if(lbl_touch_vol_val) {
        lv_label_set_text_fmt(lbl_touch_vol_val, "%d%%", val);
    }
}

void create_ventilator_sound_screen(void)
{
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }

    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ==================================================================== */
    /* 1. TOP HEADER BAR                                                    */
    /* ==================================================================== */
    lv_obj_t * top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, 1280, 60);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(top_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_hor(top_bar, 16, 0);

    /* Header Title */
    lv_obj_t * title_icon = lv_label_create(top_bar);
    lv_label_set_text(title_icon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(title_icon, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title_icon, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(title_icon, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * title_lbl = lv_label_create(top_bar);
    lv_label_set_text(title_lbl, "Sound & Audio Settings");
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 45, -8);

    lv_obj_t * sub_lbl = lv_label_create(top_bar);
    lv_label_set_text(sub_lbl, "Configure ECG wave spike beep, volume controls & touch feedback");
    lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sub_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(sub_lbl, LV_ALIGN_LEFT_MID, 45, 12);

    /* Header Clock & Battery */
    lv_obj_t * right_box = lv_obj_create(top_bar);
    lv_obj_set_size(right_box, 240, 48);
    lv_obj_align(right_box, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(right_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_box, 0, 0);

    bat_lbl = lv_label_create(right_box);
    battery_update_label(bat_lbl);
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(bat_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lbl_clock = lv_label_create(right_box);
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_RIGHT_MID, 0, 0);

    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(clock_timer);

    /* ==================================================================== */
    /* 2. CARD 1: ECG WAVEFORM BEEP CONTROL (Left Panel)                   */
    /* ==================================================================== */
    lv_obj_t * card_ecg = lv_obj_create(scr);
    lv_obj_set_size(card_ecg, 615, 330);
    lv_obj_set_pos(card_ecg, 20, 80);
    lv_obj_set_style_bg_color(card_ecg, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_ecg, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_ecg, 1, 0);
    lv_obj_set_style_radius(card_ecg, 12, 0);
    lv_obj_set_style_pad_all(card_ecg, 20, 0);

    lv_obj_t * ecg_card_title = lv_label_create(card_ecg);
    lv_label_set_text(ecg_card_title, LV_SYMBOL_AUDIO "  ECG WAVEFORM BEEP SOUND");
    lv_obj_set_style_text_font(ecg_card_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ecg_card_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(ecg_card_title, 0, 0);

    lv_obj_t * ecg_card_desc = lv_label_create(card_ecg);
    lv_label_set_text(ecg_card_desc, "Plays realistic ECG audio beep on wave spikes in Main & Monitor screens.");
    lv_obj_set_style_text_font(ecg_card_desc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ecg_card_desc, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(ecg_card_desc, 0, 24);

    /* Toggle Switch Row */
    lv_obj_t * lbl_ecg_sw_title = lv_label_create(card_ecg);
    lv_label_set_text(lbl_ecg_sw_title, "Enable ECG Spike Beep:");
    lv_obj_set_style_text_font(lbl_ecg_sw_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ecg_sw_title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(lbl_ecg_sw_title, 0, 68);

    sw_ecg_beep = lv_switch_create(card_ecg);
    lv_obj_set_size(sw_ecg_beep, 60, 30);
    lv_obj_set_pos(sw_ecg_beep, 510, 62);
    if(audio_get_ecg_enabled()) lv_obj_add_state(sw_ecg_beep, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw_ecg_beep, COLOR_ACCENT_BLUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_ecg_beep, sw_ecg_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Volume Slider Row */
    lv_obj_t * lbl_ecg_vol_title = lv_label_create(card_ecg);
    lv_label_set_text(lbl_ecg_vol_title, "ECG Beep Volume:");
    lv_obj_set_style_text_font(lbl_ecg_vol_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ecg_vol_title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(lbl_ecg_vol_title, 0, 120);

    lbl_ecg_vol_val = lv_label_create(card_ecg);
    lv_label_set_text_fmt(lbl_ecg_vol_val, "%d%%", audio_get_ecg_volume());
    lv_obj_set_style_text_font(lbl_ecg_vol_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_ecg_vol_val, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(lbl_ecg_vol_val, 520, 118);

    slider_ecg_vol = lv_slider_create(card_ecg);
    lv_obj_set_size(slider_ecg_vol, 575, 12);
    lv_obj_set_pos(slider_ecg_vol, 0, 152);
    lv_slider_set_range(slider_ecg_vol, 0, 100);
    lv_slider_set_value(slider_ecg_vol, audio_get_ecg_volume(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_ecg_vol, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_ecg_vol, COLOR_ACCENT_BLUE, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_ecg_vol, slider_ecg_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Test ECG Beep Button */
    lv_obj_t * btn_test_ecg = lv_button_create(card_ecg);
    lv_obj_set_size(btn_test_ecg, 220, 44);
    lv_obj_set_pos(btn_test_ecg, 0, 220);
    lv_obj_set_style_bg_color(btn_test_ecg, lv_color_hex(0x0E3254), 0);
    lv_obj_set_style_border_color(btn_test_ecg, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(btn_test_ecg, 1, 0);
    lv_obj_set_style_radius(btn_test_ecg, 8, 0);

    lv_obj_t * lbl_btn_test = lv_label_create(btn_test_ecg);
    lv_label_set_text(lbl_btn_test, LV_SYMBOL_VOLUME_MAX "  Test ECG Beep");
    lv_obj_set_style_text_font(lbl_btn_test, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_btn_test, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_btn_test);
    lv_obj_add_event_cb(btn_test_ecg, test_ecg_beep_btn_cb, LV_EVENT_CLICKED, NULL);

    /* ==================================================================== */
    /* 3. CARD 2: MASTER & ALARM VOLUME CONTROL (Right Panel)               */
    /* ==================================================================== */
    lv_obj_t * card_master = lv_obj_create(scr);
    lv_obj_set_size(card_master, 615, 330);
    lv_obj_set_pos(card_master, 645, 80);
    lv_obj_set_style_bg_color(card_master, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_master, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_master, 1, 0);
    lv_obj_set_style_radius(card_master, 12, 0);
    lv_obj_set_style_pad_all(card_master, 20, 0);

    lv_obj_t * master_card_title = lv_label_create(card_master);
    lv_label_set_text(master_card_title, LV_SYMBOL_SETTINGS "  MASTER & ALARM AUDIO CONTROL");
    lv_obj_set_style_text_font(master_card_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(master_card_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(master_card_title, 0, 0);

    /* Master Volume Row */
    lv_obj_t * lbl_master_title = lv_label_create(card_master);
    lv_label_set_text(lbl_master_title, "Master System Volume:");
    lv_obj_set_style_text_font(lbl_master_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_master_title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(lbl_master_title, 0, 45);

    lbl_master_vol_val = lv_label_create(card_master);
    lv_label_set_text_fmt(lbl_master_vol_val, "%d%%", audio_get_master_volume());
    lv_obj_set_style_text_font(lbl_master_vol_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_master_vol_val, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(lbl_master_vol_val, 520, 43);

    slider_master_vol = lv_slider_create(card_master);
    lv_obj_set_size(slider_master_vol, 575, 12);
    lv_obj_set_pos(slider_master_vol, 0, 75);
    lv_slider_set_range(slider_master_vol, 0, 100);
    lv_slider_set_value(slider_master_vol, audio_get_master_volume(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_master_vol, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_master_vol, COLOR_ACCENT_BLUE, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_master_vol, slider_master_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Alarm Volume Row */
    lv_obj_t * lbl_alarm_title = lv_label_create(card_master);
    lv_label_set_text(lbl_alarm_title, "Critical Alarm Volume:");
    lv_obj_set_style_text_font(lbl_alarm_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_alarm_title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(lbl_alarm_title, 0, 125);

    lbl_alarm_vol_val = lv_label_create(card_master);
    lv_label_set_text_fmt(lbl_alarm_vol_val, "%d%%", audio_get_alarm_volume());
    lv_obj_set_style_text_font(lbl_alarm_vol_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_alarm_vol_val, COLOR_ACCENT_RED, 0);
    lv_obj_set_pos(lbl_alarm_vol_val, 520, 123);

    slider_alarm_vol = lv_slider_create(card_master);
    lv_obj_set_size(slider_alarm_vol, 575, 12);
    lv_obj_set_pos(slider_alarm_vol, 0, 155);
    lv_slider_set_range(slider_alarm_vol, 0, 100);
    lv_slider_set_value(slider_alarm_vol, audio_get_alarm_volume(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_alarm_vol, COLOR_ACCENT_RED, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_alarm_vol, COLOR_ACCENT_RED, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_alarm_vol, slider_alarm_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Test Alarm Button */
    lv_obj_t * btn_test_alarm = lv_button_create(card_master);
    lv_obj_set_size(btn_test_alarm, 220, 44);
    lv_obj_set_pos(btn_test_alarm, 0, 220);
    lv_obj_set_style_bg_color(btn_test_alarm, lv_color_hex(0x3B1515), 0);
    lv_obj_set_style_border_color(btn_test_alarm, COLOR_ACCENT_RED, 0);
    lv_obj_set_style_border_width(btn_test_alarm, 1, 0);
    lv_obj_set_style_radius(btn_test_alarm, 8, 0);

    lv_obj_t * lbl_test_alarm = lv_label_create(btn_test_alarm);
    lv_label_set_text(lbl_test_alarm, LV_SYMBOL_BELL "  Test Alarm Sound");
    lv_obj_set_style_text_font(lbl_test_alarm, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_test_alarm, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_test_alarm);
    lv_obj_add_event_cb(btn_test_alarm, test_alarm_btn_cb, LV_EVENT_CLICKED, NULL);

    /* ==================================================================== */
    /* 4. CARD 3: TOUCH & CLICK FEEDBACK (Bottom Panel)                    */
    /* ==================================================================== */
    lv_obj_t * card_touch = lv_obj_create(scr);
    lv_obj_set_size(card_touch, 1240, 200);
    lv_obj_set_pos(card_touch, 20, 430);
    lv_obj_set_style_bg_color(card_touch, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_touch, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_touch, 1, 0);
    lv_obj_set_style_radius(card_touch, 12, 0);
    lv_obj_set_style_pad_all(card_touch, 20, 0);

    lv_obj_t * touch_card_title = lv_label_create(card_touch);
    lv_label_set_text(touch_card_title, LV_SYMBOL_BELL "  TOUCH & KEYFEEDBACK TONE");
    lv_obj_set_style_text_font(touch_card_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(touch_card_title, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_pos(touch_card_title, 0, 0);

    /* Touch Sound Toggle */
    lv_obj_t * lbl_touch_sw = lv_label_create(card_touch);
    lv_label_set_text(lbl_touch_sw, "Enable Touch Click Sound:");
    lv_obj_set_style_text_font(lbl_touch_sw, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_touch_sw, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(lbl_touch_sw, 0, 40);

    sw_touch_sound = lv_switch_create(card_touch);
    lv_obj_set_size(sw_touch_sound, 60, 30);
    lv_obj_set_pos(sw_touch_sound, 1130, 35);
    if(audio_get_touch_enabled()) lv_obj_add_state(sw_touch_sound, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw_touch_sound, COLOR_ACCENT_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_touch_sound, sw_touch_sound_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Touch Volume Slider */
    lv_obj_t * lbl_touch_vol = lv_label_create(card_touch);
    lv_label_set_text(lbl_touch_vol, "Touch Feedback Volume:");
    lv_obj_set_style_text_font(lbl_touch_vol, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_touch_vol, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(lbl_touch_vol, 0, 90);

    lbl_touch_vol_val = lv_label_create(card_touch);
    lv_label_set_text_fmt(lbl_touch_vol_val, "%d%%", audio_get_touch_volume());
    lv_obj_set_style_text_font(lbl_touch_vol_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_touch_vol_val, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_pos(lbl_touch_vol_val, 1140, 88);

    slider_touch_vol = lv_slider_create(card_touch);
    lv_obj_set_size(slider_touch_vol, 1200, 12);
    lv_obj_set_pos(slider_touch_vol, 0, 120);
    lv_slider_set_range(slider_touch_vol, 0, 100);
    lv_slider_set_value(slider_touch_vol, audio_get_touch_volume(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_touch_vol, COLOR_ACCENT_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_touch_vol, COLOR_ACCENT_GREEN, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_touch_vol, slider_touch_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ==================================================================== */
    /* 5. BOTTOM NAVIGATION BAR                                             */
    /* ==================================================================== */
    lv_obj_t * bot_bar = lv_obj_create(scr);
    lv_obj_set_size(bot_bar, 1260, 55);
    lv_obj_set_pos(bot_bar, 10, 720);
    lv_obj_set_style_bg_opa(bot_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bot_bar, 0, 0);
    lv_obj_set_style_pad_all(bot_bar, 0, 0);

    /* Back to Settings Button */
    lv_obj_t * btn_back = lv_button_create(bot_bar);
    lv_obj_set_size(btn_back, 160, 48);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_back, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_back, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_back, 1, 0);
    lv_obj_set_style_radius(btn_back, 8, 0);

    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " SETTINGS");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_back, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, back_to_settings_cb, LV_EVENT_CLICKED, NULL);

    /* HOME Button */
    lv_obj_t * btn_home = lv_button_create(bot_bar);
    lv_obj_set_size(btn_home, 140, 48);
    lv_obj_align(btn_home, LV_ALIGN_LEFT_MID, 180, 0);
    lv_obj_set_style_bg_color(btn_home, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_home, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_home, 1, 0);
    lv_obj_set_style_radius(btn_home, 8, 0);

    lv_obj_t * lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, LV_SYMBOL_HOME " HOME");
    lv_obj_set_style_text_font(lbl_home, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_home, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_home);
    lv_obj_add_event_cb(btn_home, back_to_home_cb, LV_EVENT_CLICKED, NULL);

    lv_screen_load(scr);
}
