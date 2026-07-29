#include "ventilator_monitoring_screen.h"
#include "ventilator_main_screen.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Colors matching reference UI screenshot */
#define COLOR_DASHBOARD_BG      lv_color_hex(0x040B16)
#define COLOR_CARD_BG           lv_color_hex(0x09182B)
#define COLOR_CARD_BORDER       lv_color_hex(0x132C4A)
#define COLOR_ACCENT_BLUE       lv_color_hex(0x00A8FF)
#define COLOR_ACCENT_GREEN      lv_color_hex(0x00E676)
#define COLOR_ACCENT_YELLOW     lv_color_hex(0xFFD600)
#define COLOR_ACCENT_RED        lv_color_hex(0xD50000)
#define COLOR_ACCENT_PURPLE     lv_color_hex(0xA020F0)
#define COLOR_TEXT_MAIN         lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_MUTED        lv_color_hex(0x7097BA)
#define COLOR_BTN_NAV_BG        lv_color_hex(0x091D36)
#define COLOR_BTN_NAV_ACTIVE    lv_color_hex(0x0A3B73)

/* Waveform Charts references */
static lv_obj_t * chart_pressure = NULL;
static lv_obj_t * chart_flow = NULL;
static lv_obj_t * chart_volume = NULL;

static lv_chart_series_t * ser_pressure = NULL;
static lv_chart_series_t * ser_flow = NULL;
static lv_chart_series_t * ser_volume = NULL;

/* Loop Charts references */
static lv_obj_t * chart_pv_loop = NULL;
static lv_obj_t * chart_fv_loop = NULL;

static lv_chart_series_t * ser_pv_loop = NULL;
static lv_chart_series_t * ser_fv_loop = NULL;

static lv_timer_t * wave_timer = NULL;
static float wave_phase_counter = 0.0f;

/* References to live parameter widgets */
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * lbl_val_ppeak = NULL;
static lv_obj_t * lbl_val_pmean = NULL;
static lv_obj_t * lbl_val_vt = NULL;

static lv_obj_t * lbl_pf_ratio = NULL;
static lv_obj_t * lbl_o2_delivery = NULL;

/* Forward declarations for screen callbacks */
static void nav_btn_cb(lv_event_t * e);
static void goto_settings_cb(lv_event_t * e);
static void pat_badge_click_cb(lv_event_t * e);

/* Populate standard loop graph coordinates representing inspiration and expiration */
static void populate_loops_data(void)
{
    if(!ser_pv_loop || !ser_fv_loop) return;

    /* Populate 60 points to make a complete clinical closed loop */
    int num_points = 60;
    for(int i = 0; i < num_points; i++) {
        float t = (float)i / (float)(num_points / 2);
        int32_t vol = 50;
        int32_t press = 10;
        int32_t flow = 0;

        if(i < num_points / 2) {
            /* 1. Inspiration Phase */
            float r = t; /* sweeps 0.0 to 1.0 */
            vol = (int32_t)(50.0f + 550.0f * (0.5f * (1.0f - cosf(r * M_PI))));
            press = (int32_t)(10.0f + 28.0f * powf(r, 0.7f));
            flow = (int32_t)(50.0f * sinf(r * M_PI));
        } else {
            /* 2. Expiration Phase */
            float r = t - 1.0f; /* sweeps 0.0 to 1.0 */
            vol = (int32_t)(50.0f + 550.0f * (0.5f * (1.0f + cosf(r * M_PI))));
            press = (int32_t)(10.0f + 28.0f * powf(1.0f - r, 2.0f));
            flow = (int32_t)(-60.0f * sinf(r * M_PI));
        }

        /* Set points on the scatter loops charts */
        lv_chart_set_series_value_by_id2(chart_pv_loop, ser_pv_loop, i, vol, press);
        lv_chart_set_series_value_by_id2(chart_fv_loop, ser_fv_loop, i, vol, flow);
    }
}

/* Generate exact clinical ventilator wave points matching the target screenshot */
static void waveform_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    wave_phase_counter += 0.025f;
    if(wave_phase_counter >= 1.0f) wave_phase_counter -= 1.0f;

    float t = wave_phase_counter;
    float p_val = 10.0f;
    float f_val = 0.0f;
    float v_val = 50.0f;

    /* 1. Pressure Curve (cmH2O): Baseline 10, Plateau 38 */
    if(t < 0.08f) {
        float r = t / 0.08f;
        p_val = 10.0f + 28.0f * (0.5f * (1.0f - cosf(r * M_PI)));
    } else if(t < 0.28f) {
        p_val = 38.0f;
    } else if(t < 0.38f) {
        float r = (t - 0.28f) / 0.10f;
        p_val = 10.0f + 28.0f * (0.5f * (1.0f + cosf(r * M_PI)));
    } else {
        p_val = 10.0f;
    }

    /* 2. Flow Curve (L/min): Positive inspiratory peak + Negative expiratory peak */
    if(t < 0.06f) {
        float r = t / 0.06f;
        f_val = 50.0f * sinf(r * M_PI / 2.0f);
    } else if(t < 0.28f) {
        float r = (t - 0.06f) / 0.22f;
        f_val = 50.0f * expf(-3.0f * r);
    } else if(t < 0.35f) {
        f_val = 0.0f;
    } else if(t < 0.40f) {
        float r = (t - 0.35f) / 0.05f;
        f_val = -58.0f * sinf(r * M_PI / 2.0f);
    } else if(t < 0.80f) {
        float r = (t - 0.40f) / 0.40f;
        f_val = -58.0f * expf(-3.5f * r);
    } else {
        f_val = 0.0f;
    }

    /* 3. Volume Curve (mL): Baseline 50 to Peak 600 */
    if(t < 0.28f) {
        float r = t / 0.28f;
        v_val = 50.0f + 550.0f * (0.5f * (1.0f - cosf(r * M_PI)));
    } else if(t < 0.75f) {
        float r = (t - 0.28f) / 0.47f;
        v_val = 50.0f + 550.0f * expf(-4.0f * r);
    } else {
        v_val = 50.0f;
    }

    if(chart_pressure && ser_pressure) {
        lv_chart_set_next_value(chart_pressure, ser_pressure, (int32_t)p_val);
    }
    if(chart_flow && ser_flow) {
        lv_chart_set_next_value(chart_flow, ser_flow, (int32_t)f_val);
    }
    if(chart_volume && ser_volume) {
        lv_chart_set_next_value(chart_volume, ser_volume, (int32_t)v_val);
    }

    /* Fluctuate parameters once per breath cycle */
    static float prev_t = 0.0f;
    static float live_ppeak = 32.0f;
    static float live_pmean = 35.0f;
    static float live_vt = 420.0f;

    if(t < prev_t || (lbl_val_ppeak && strcmp(lv_label_get_text(lbl_val_ppeak), "32") == 0)) {
        live_ppeak = 31.0f + (float)(rand() % 20) / 10.0f;
        live_pmean = 34.0f + (float)(rand() % 20) / 10.0f;
        live_vt = 410.0f + (float)(rand() % 20);

        if(lbl_val_ppeak) lv_label_set_text_fmt(lbl_val_ppeak, "%.0f", live_ppeak);
        if(lbl_val_pmean) lv_label_set_text_fmt(lbl_val_pmean, "%.0f", live_pmean);
        if(lbl_val_vt) lv_label_set_text_fmt(lbl_val_vt, "%.0f", live_vt);
    }

    /* Update real-time clock label */
    if(lbl_clock) {
        time_t raw_time;
        struct tm * time_info;
        time(&raw_time);
        time_info = localtime(&raw_time);

        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%b %d, %Y\n%I:%M %p", time_info);

        if(strcmp(lv_label_get_text(lbl_clock), clock_buf) != 0) {
            lv_label_set_text(lbl_clock, clock_buf);
        }
    }

    prev_t = t;
}

/**
 * @brief Create and render the detailed Loops/Monitoring screen.
 */
void create_ventilator_monitoring_screen(void)
{
    if(wave_timer) {
        lv_timer_delete(wave_timer);
        wave_timer = NULL;
    }

    /* Create screen root and disable scrollable */
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ==================================================================== */
    /* 1. TOP HEADER BAR                                                    */
    /* ==================================================================== */
    lv_obj_t * top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, 1280, 55);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(top_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_hor(top_bar, 12, 0);

    /* Mode Badge (Left) */
    lv_obj_t * mode_box = lv_obj_create(top_bar);
    lv_obj_set_size(mode_box, 180, 42);
    lv_obj_align(mode_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(mode_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(mode_box, 0, 0);
    lv_obj_set_style_radius(mode_box, 6, 0);
    lv_obj_set_style_pad_all(mode_box, 4, 0);

    lv_obj_t * mode_title = lv_label_create(mode_box);
    lv_label_set_text(mode_title, "VC-A/C");
    lv_obj_set_style_text_font(mode_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(mode_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(mode_title, LV_ALIGN_LEFT_MID, 6, -6);

    lv_obj_t * mode_sub = lv_label_create(mode_box);
    lv_label_set_text(mode_sub, "Adult");
    lv_obj_set_style_text_font(mode_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_sub, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(mode_sub, LV_ALIGN_LEFT_MID, 6, 10);

    /* Patient ID Badge */
    lv_obj_t * pat_box = lv_obj_create(top_bar);
    lv_obj_set_size(pat_box, 140, 42);
    lv_obj_align(pat_box, LV_ALIGN_LEFT_MID, 190, 0);
    lv_obj_set_style_bg_color(pat_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(pat_box, 0, 0);
    lv_obj_set_style_radius(pat_box, 6, 0);
    lv_obj_add_flag(pat_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pat_box, pat_badge_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * pat_lbl = lv_label_create(pat_box);
    lv_label_set_text(pat_lbl, LV_SYMBOL_DIRECTORY " Patient\n#7097ba ID: 12345678#");
    lv_label_set_recolor(pat_lbl, true);
    lv_obj_set_style_text_font(pat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pat_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(pat_lbl, LV_ALIGN_LEFT_MID, 4, 0);

    /* Alarm Banner (Center) */
    lv_obj_t * alarm_box = lv_obj_create(top_bar);
    lv_obj_set_size(alarm_box, 520, 42);
    lv_obj_align(alarm_box, LV_ALIGN_CENTER, -40, 0);
    lv_obj_set_style_bg_color(alarm_box, COLOR_ACCENT_RED, 0);
    lv_obj_set_style_border_width(alarm_box, 0, 0);
    lv_obj_set_style_radius(alarm_box, 6, 0);

    lv_obj_t * alarm_lbl = lv_label_create(alarm_box);
    lv_label_set_text(alarm_lbl, LV_SYMBOL_BELL "  HIGH PRESSURE ALARM\n   Ppeak above limit");
    lv_obj_set_style_text_font(alarm_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(alarm_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * alarm_time = lv_label_create(alarm_box);
    lv_label_set_text(alarm_time, "10:24  >");
    lv_obj_set_style_text_font(alarm_time, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_time, COLOR_TEXT_MAIN, 0);
    lv_obj_align(alarm_time, LV_ALIGN_RIGHT_MID, -10, 0);

    /* Battery, Clock & Settings (Right) */
    lv_obj_t * right_hdr = lv_obj_create(top_bar);
    lv_obj_set_size(right_hdr, 280, 42);
    lv_obj_align(right_hdr, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(right_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_hdr, 0, 0);

    lv_obj_t * bat_lbl = lv_label_create(right_hdr);
    lv_label_set_text(bat_lbl, LV_SYMBOL_BATTERY_FULL " 100%");
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bat_lbl, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(bat_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lbl_clock = lv_label_create(right_hdr);
    lv_label_set_text(lbl_clock, "May 20, 2024\n10:24 AM");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_CENTER, 20, 0);

    lv_obj_t * set_icon = lv_label_create(right_hdr);
    lv_label_set_text(set_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(set_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(set_icon, COLOR_TEXT_MUTED, 0);
    lv_obj_align(set_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(set_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(set_icon, goto_settings_cb, LV_EVENT_CLICKED, NULL);

    /* ==================================================================== */
    /* 2. MAIN CENTER CONTAINER (Takes entire remaining left side space)    */
    /* ==================================================================== */
    lv_obj_t * center_cont = lv_obj_create(scr);
    lv_obj_set_size(center_cont, 1120, 675);
    lv_obj_set_pos(center_cont, 10, 60);
    lv_obj_set_style_bg_opa(center_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(center_cont, 0, 0);
    lv_obj_set_style_pad_all(center_cont, 0, 0);

    /* Stacked Waveforms Panel (Top: 1120 x 370) */
    lv_obj_t * wave_cont = lv_obj_create(center_cont);
    lv_obj_set_size(wave_cont, 1120, 360);
    lv_obj_align(wave_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(wave_cont, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(wave_cont, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(wave_cont, 1, 0);
    lv_obj_set_style_radius(wave_cont, 10, 0);
    lv_obj_set_style_pad_all(wave_cont, 4, 0);

    /* 2A. Pressure Waveform Box */
    lv_obj_t * box_p = lv_obj_create(wave_cont);
    lv_obj_set_size(box_p, 1110, 112);
    lv_obj_align(box_p, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(box_p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_p, 0, 0);

    /* Left Info */
    lv_obj_t * lbl_p_title = lv_label_create(box_p);
    lv_label_set_text(lbl_p_title, "Pressure");
    lv_obj_set_style_text_font(lbl_p_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_p_title, COLOR_ACCENT_PURPLE, 0);
    lv_obj_align(lbl_p_title, LV_ALIGN_TOP_LEFT, 4, 6);

    lv_obj_t * lbl_p_unit = lv_label_create(box_p);
    lv_label_set_text(lbl_p_unit, "cmH2O");
    lv_obj_set_style_text_font(lbl_p_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_p_unit, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_p_unit, LV_ALIGN_TOP_LEFT, 4, 24);

    /* Ticks Axis Labels */
    lv_obj_t * tick_p_max = lv_label_create(box_p);
    lv_label_set_text(tick_p_max, "60");
    lv_obj_set_style_text_font(tick_p_max, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_p_max, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_p_max, LV_ALIGN_TOP_LEFT, 80, 8);

    lv_obj_t * tick_p_mid = lv_label_create(box_p);
    lv_label_set_text(tick_p_mid, "30");
    lv_obj_set_style_text_font(tick_p_mid, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_p_mid, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_p_mid, LV_ALIGN_TOP_LEFT, 80, 42);

    lv_obj_t * tick_p_base = lv_label_create(box_p);
    lv_label_set_text(tick_p_base, "0");
    lv_obj_set_style_text_font(tick_p_base, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_p_base, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_p_base, LV_ALIGN_TOP_LEFT, 85, 76);

    lv_obj_t * tick_p_min = lv_label_create(box_p);
    lv_label_set_text(tick_p_min, "-10");
    lv_obj_set_style_text_font(tick_p_min, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_p_min, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_p_min, LV_ALIGN_TOP_LEFT, 76, 92);

    /* Chart */
    chart_pressure = lv_chart_create(box_p);
    lv_obj_set_size(chart_pressure, 880, 92);
    lv_obj_align(chart_pressure, LV_ALIGN_TOP_LEFT, 110, 8);
    lv_chart_set_type(chart_pressure, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_pressure, 120);
    lv_chart_set_update_mode(chart_pressure, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_axis_range(chart_pressure, LV_CHART_AXIS_PRIMARY_Y, -10, 60);
    lv_obj_set_style_bg_opa(chart_pressure, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_pressure, 0, 0);
    lv_obj_set_style_line_opa(chart_pressure, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_pressure, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_pressure, 0, 0, LV_PART_INDICATOR);
    ser_pressure = lv_chart_add_series(chart_pressure, COLOR_ACCENT_PURPLE, LV_CHART_AXIS_PRIMARY_Y);

    /* Right Readout */
    lbl_val_ppeak = lv_label_create(box_p);
    lv_label_set_text(lbl_val_ppeak, "32");
    lv_obj_set_style_text_font(lbl_val_ppeak, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_val_ppeak, COLOR_ACCENT_PURPLE, 0);
    lv_obj_align(lbl_val_ppeak, LV_ALIGN_TOP_RIGHT, -45, 12);

    lv_obj_t * lbl_ppeak_sub = lv_label_create(box_p);
    lv_label_set_text(lbl_ppeak_sub, "Ppeak");
    lv_obj_set_style_text_font(lbl_ppeak_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_ppeak_sub, COLOR_ACCENT_PURPLE, 0);
    lv_obj_align(lbl_ppeak_sub, LV_ALIGN_TOP_RIGHT, -40, 46);

    /* 2B. Flow Waveform Box */
    lv_obj_t * box_f = lv_obj_create(wave_cont);
    lv_obj_set_size(box_f, 1110, 112);
    lv_obj_align(box_f, LV_ALIGN_TOP_MID, 0, 118);
    lv_obj_set_style_bg_opa(box_f, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_f, 0, 0);

    /* Left Info */
    lv_obj_t * lbl_f_title = lv_label_create(box_f);
    lv_label_set_text(lbl_f_title, "Flow");
    lv_obj_set_style_text_font(lbl_f_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_f_title, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(lbl_f_title, LV_ALIGN_TOP_LEFT, 4, 6);

    lv_obj_t * lbl_f_unit = lv_label_create(box_f);
    lv_label_set_text(lbl_f_unit, "L/min");
    lv_obj_set_style_text_font(lbl_f_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_f_unit, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_f_unit, LV_ALIGN_TOP_LEFT, 4, 24);

    /* Ticks Axis Labels */
    lv_obj_t * tick_f_max = lv_label_create(box_f);
    lv_label_set_text(tick_f_max, "80");
    lv_obj_set_style_text_font(tick_f_max, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_f_max, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_f_max, LV_ALIGN_TOP_LEFT, 80, 8);

    lv_obj_t * tick_f_base = lv_label_create(box_f);
    lv_label_set_text(tick_f_base, "0");
    lv_obj_set_style_text_font(tick_f_base, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_f_base, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_f_base, LV_ALIGN_TOP_LEFT, 85, 48);

    lv_obj_t * tick_f_min = lv_label_create(box_f);
    lv_label_set_text(tick_f_min, "-80");
    lv_obj_set_style_text_font(tick_f_min, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_f_min, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_f_min, LV_ALIGN_TOP_LEFT, 76, 88);

    /* Chart */
    chart_flow = lv_chart_create(box_f);
    lv_obj_set_size(chart_flow, 880, 92);
    lv_obj_align(chart_flow, LV_ALIGN_TOP_LEFT, 110, 8);
    lv_chart_set_type(chart_flow, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_flow, 120);
    lv_chart_set_update_mode(chart_flow, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_axis_range(chart_flow, LV_CHART_AXIS_PRIMARY_Y, -80, 80);
    lv_obj_set_style_bg_opa(chart_flow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_flow, 0, 0);
    lv_obj_set_style_line_opa(chart_flow, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_flow, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_flow, 0, 0, LV_PART_INDICATOR);
    ser_flow = lv_chart_add_series(chart_flow, COLOR_ACCENT_GREEN, LV_CHART_AXIS_PRIMARY_Y);

    /* Right Readout */
    lbl_val_pmean = lv_label_create(box_f);
    lv_label_set_text(lbl_val_pmean, "35");
    lv_obj_set_style_text_font(lbl_val_pmean, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_val_pmean, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(lbl_val_pmean, LV_ALIGN_TOP_RIGHT, -45, 12);

    lv_obj_t * lbl_pmean_sub = lv_label_create(box_f);
    lv_label_set_text(lbl_pmean_sub, "Pmean");
    lv_obj_set_style_text_font(lbl_pmean_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_pmean_sub, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(lbl_pmean_sub, LV_ALIGN_TOP_RIGHT, -40, 46);

    /* 2C. Volume Waveform Box */
    lv_obj_t * box_v = lv_obj_create(wave_cont);
    lv_obj_set_size(box_v, 1110, 112);
    lv_obj_align(box_v, LV_ALIGN_TOP_MID, 0, 236);
    lv_obj_set_style_bg_opa(box_v, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_v, 0, 0);

    /* Left Info */
    lv_obj_t * lbl_v_text = lv_label_create(box_v);
    lv_label_set_text(lbl_v_text, "Volume");
    lv_obj_set_style_text_font(lbl_v_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_v_text, COLOR_ACCENT_YELLOW, 0);
    lv_obj_align(lbl_v_text, LV_ALIGN_TOP_LEFT, 4, 6);

    lv_obj_t * lbl_v_unit = lv_label_create(box_v);
    lv_label_set_text(lbl_v_unit, "mL");
    lv_obj_set_style_text_font(lbl_v_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_v_unit, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_v_unit, LV_ALIGN_TOP_LEFT, 4, 24);

    /* Ticks Axis Labels */
    lv_obj_t * tick_v_max = lv_label_create(box_v);
    lv_label_set_text(tick_v_max, "800");
    lv_obj_set_style_text_font(tick_v_max, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_v_max, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_v_max, LV_ALIGN_TOP_LEFT, 76, 8);

    lv_obj_t * tick_v_mid = lv_label_create(box_v);
    lv_label_set_text(tick_v_mid, "400");
    lv_obj_set_style_text_font(tick_v_mid, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_v_mid, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_v_mid, LV_ALIGN_TOP_LEFT, 76, 48);

    lv_obj_t * tick_v_base = lv_label_create(box_v);
    lv_label_set_text(tick_v_base, "0");
    lv_obj_set_style_text_font(tick_v_base, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tick_v_base, COLOR_TEXT_MUTED, 0);
    lv_obj_align(tick_v_base, LV_ALIGN_TOP_LEFT, 85, 88);

    /* Chart */
    chart_volume = lv_chart_create(box_v);
    lv_obj_set_size(chart_volume, 880, 92);
    lv_obj_align(chart_volume, LV_ALIGN_TOP_LEFT, 110, 8);
    lv_chart_set_type(chart_volume, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_volume, 120);
    lv_chart_set_update_mode(chart_volume, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_axis_range(chart_volume, LV_CHART_AXIS_PRIMARY_Y, 0, 800);
    lv_obj_set_style_bg_opa(chart_volume, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_volume, 0, 0);
    lv_obj_set_style_line_opa(chart_volume, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_volume, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_volume, 0, 0, LV_PART_INDICATOR);
    ser_volume = lv_chart_add_series(chart_volume, COLOR_ACCENT_YELLOW, LV_CHART_AXIS_PRIMARY_Y);

    /* Right Readout */
    lbl_val_vt = lv_label_create(box_v);
    lv_label_set_text(lbl_val_vt, "420");
    lv_obj_set_style_text_font(lbl_val_vt, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_val_vt, COLOR_ACCENT_YELLOW, 0);
    lv_obj_align(lbl_val_vt, LV_ALIGN_TOP_RIGHT, -45, 12);

    lv_obj_t * lbl_vt_sub = lv_label_create(box_v);
    lv_label_set_text(lbl_vt_sub, "Vt");
    lv_obj_set_style_text_font(lbl_vt_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_vt_sub, COLOR_ACCENT_YELLOW, 0);
    lv_obj_align(lbl_vt_sub, LV_ALIGN_TOP_RIGHT, -40, 46);

    /* ==================================================================== */
    /* 3. LOOPS SECTION (Bottom: PV Loop + FV Loop + Oxygenation)           */
    /* ==================================================================== */
    lv_obj_t * loops_cont = lv_obj_create(center_cont);
    lv_obj_set_size(loops_cont, 1120, 300);
    lv_obj_align(loops_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(loops_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(loops_cont, 0, 0);
    lv_obj_set_style_pad_all(loops_cont, 0, 0);

    /* Loop Card 1: PRESSURE-VOLUME LOOP */
    lv_obj_t * card_pv = lv_obj_create(loops_cont);
    lv_obj_set_size(card_pv, 430, 295);
    lv_obj_align(card_pv, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(card_pv, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_pv, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_pv, 1, 0);
    lv_obj_set_style_radius(card_pv, 10, 0);
    lv_obj_set_style_pad_all(card_pv, 6, 0);

    lv_obj_t * lbl_pv_title = lv_label_create(card_pv);
    lv_label_set_text(lbl_pv_title, "PRESSURE-VOLUME LOOP");
    lv_obj_set_style_text_font(lbl_pv_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_pv_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_pv_title, LV_ALIGN_TOP_LEFT, 10, 4);

    lv_obj_t * lbl_pv_y = lv_label_create(card_pv);
    lv_label_set_text(lbl_pv_y, "Pressure (cmH2O)");
    lv_obj_set_style_text_font(lbl_pv_y, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_pv_y, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_pv_y, LV_ALIGN_TOP_LEFT, 10, 24);

    /* Chart PV Loop */
    chart_pv_loop = lv_chart_create(card_pv);
    lv_obj_set_size(chart_pv_loop, 360, 220);
    lv_obj_align(chart_pv_loop, LV_ALIGN_BOTTOM_MID, 15, -4);
    lv_chart_set_type(chart_pv_loop, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(chart_pv_loop, 60);
    lv_chart_set_range(chart_pv_loop, LV_CHART_AXIS_PRIMARY_X, 0, 1000);
    lv_chart_set_range(chart_pv_loop, LV_CHART_AXIS_PRIMARY_Y, -15, 60);
    lv_obj_set_style_bg_opa(chart_pv_loop, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_pv_loop, 0, 0);
    lv_obj_set_style_line_width(chart_pv_loop, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_pv_loop, 0, 0, LV_PART_INDICATOR);
    ser_pv_loop = lv_chart_add_series(chart_pv_loop, COLOR_ACCENT_PURPLE, LV_CHART_AXIS_PRIMARY_Y);

    /* PV Axes Ticks */
    lv_obj_t * pv_y_60 = lv_label_create(card_pv);
    lv_label_set_text(pv_y_60, "60");
    lv_obj_set_style_text_font(pv_y_60, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pv_y_60, COLOR_TEXT_MUTED, 0);
    lv_obj_align(pv_y_60, LV_ALIGN_BOTTOM_LEFT, 12, -220);

    lv_obj_t * pv_y_0 = lv_label_create(card_pv);
    lv_label_set_text(pv_y_0, "0");
    lv_obj_set_style_text_font(pv_y_0, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pv_y_0, COLOR_TEXT_MUTED, 0);
    lv_obj_align(pv_y_0, LV_ALIGN_BOTTOM_LEFT, 18, -60);

    lv_obj_t * pv_y_min15 = lv_label_create(card_pv);
    lv_label_set_text(pv_y_min15, "-15");
    lv_obj_set_style_text_font(pv_y_min15, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pv_y_min15, COLOR_TEXT_MUTED, 0);
    lv_obj_align(pv_y_min15, LV_ALIGN_BOTTOM_LEFT, 8, -12);

    lv_obj_t * pv_x_0 = lv_label_create(card_pv);
    lv_label_set_text(pv_x_0, "0");
    lv_obj_set_style_text_font(pv_x_0, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pv_x_0, COLOR_TEXT_MUTED, 0);
    lv_obj_align(pv_x_0, LV_ALIGN_BOTTOM_LEFT, 45, 0);

    lv_obj_t * pv_x_500 = lv_label_create(card_pv);
    lv_label_set_text(pv_x_500, "500");
    lv_obj_set_style_text_font(pv_x_500, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pv_x_500, COLOR_TEXT_MUTED, 0);
    lv_obj_align(pv_x_500, LV_ALIGN_BOTTOM_LEFT, 215, 0);

    lv_obj_t * pv_x_1000 = lv_label_create(card_pv);
    lv_label_set_text(pv_x_1000, "1000");
    lv_obj_set_style_text_font(pv_x_1000, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pv_x_1000, COLOR_TEXT_MUTED, 0);
    lv_obj_align(pv_x_1000, LV_ALIGN_BOTTOM_RIGHT, -15, 0);

    lv_obj_t * pv_x_title = lv_label_create(card_pv);
    lv_label_set_text(pv_x_title, "Volume (mL)");
    lv_obj_set_style_text_font(pv_x_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pv_x_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(pv_x_title, LV_ALIGN_BOTTOM_MID, 15, -12);

    /* Loop Card 2: FLOW-VOLUME LOOP */
    lv_obj_t * card_fv = lv_obj_create(loops_cont);
    lv_obj_set_size(card_fv, 430, 295);
    lv_obj_align(card_fv, LV_ALIGN_LEFT_MID, 445, 0);
    lv_obj_set_style_bg_color(card_fv, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_fv, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_fv, 1, 0);
    lv_obj_set_style_radius(card_fv, 10, 0);
    lv_obj_set_style_pad_all(card_fv, 6, 0);

    lv_obj_t * lbl_fv_title = lv_label_create(card_fv);
    lv_label_set_text(lbl_fv_title, "FLOW-VOLUME LOOP");
    lv_obj_set_style_text_font(lbl_fv_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_fv_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_fv_title, LV_ALIGN_TOP_LEFT, 10, 4);

    lv_obj_t * lbl_fv_y = lv_label_create(card_fv);
    lv_label_set_text(lbl_fv_y, "Flow (L/min)");
    lv_obj_set_style_text_font(lbl_fv_y, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_fv_y, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_fv_y, LV_ALIGN_TOP_LEFT, 10, 24);

    /* Chart FV Loop */
    chart_fv_loop = lv_chart_create(card_fv);
    lv_obj_set_size(chart_fv_loop, 360, 220);
    lv_obj_align(chart_fv_loop, LV_ALIGN_BOTTOM_MID, 15, -4);
    lv_chart_set_type(chart_fv_loop, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(chart_fv_loop, 60);
    lv_chart_set_range(chart_fv_loop, LV_CHART_AXIS_PRIMARY_X, 0, 1000);
    lv_chart_set_range(chart_fv_loop, LV_CHART_AXIS_PRIMARY_Y, -80, 80);
    lv_obj_set_style_bg_opa(chart_fv_loop, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_fv_loop, 0, 0);
    lv_obj_set_style_line_width(chart_fv_loop, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_fv_loop, 0, 0, LV_PART_INDICATOR);
    ser_fv_loop = lv_chart_add_series(chart_fv_loop, COLOR_ACCENT_GREEN, LV_CHART_AXIS_PRIMARY_Y);

    /* FV Axes Ticks */
    lv_obj_t * fv_y_80 = lv_label_create(card_fv);
    lv_label_set_text(fv_y_80, "80");
    lv_obj_set_style_text_font(fv_y_80, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fv_y_80, COLOR_TEXT_MUTED, 0);
    lv_obj_align(fv_y_80, LV_ALIGN_BOTTOM_LEFT, 12, -220);

    lv_obj_t * fv_y_0 = lv_label_create(card_fv);
    lv_label_set_text(fv_y_0, "0");
    lv_obj_set_style_text_font(fv_y_0, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fv_y_0, COLOR_TEXT_MUTED, 0);
    lv_obj_align(fv_y_0, LV_ALIGN_BOTTOM_LEFT, 18, -114);

    lv_obj_t * fv_y_min80 = lv_label_create(card_fv);
    lv_label_set_text(fv_y_min80, "-80");
    lv_obj_set_style_text_font(fv_y_min80, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fv_y_min80, COLOR_TEXT_MUTED, 0);
    lv_obj_align(fv_y_min80, LV_ALIGN_BOTTOM_LEFT, 8, -12);

    lv_obj_t * fv_x_0 = lv_label_create(card_fv);
    lv_label_set_text(fv_x_0, "0");
    lv_obj_set_style_text_font(fv_x_0, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fv_x_0, COLOR_TEXT_MUTED, 0);
    lv_obj_align(fv_x_0, LV_ALIGN_BOTTOM_LEFT, 45, 0);

    lv_obj_t * fv_x_500 = lv_label_create(card_fv);
    lv_label_set_text(fv_x_500, "500");
    lv_obj_set_style_text_font(fv_x_500, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fv_x_500, COLOR_TEXT_MUTED, 0);
    lv_obj_align(fv_x_500, LV_ALIGN_BOTTOM_LEFT, 215, 0);

    lv_obj_t * fv_x_1000 = lv_label_create(card_fv);
    lv_label_set_text(fv_x_1000, "1000");
    lv_obj_set_style_text_font(fv_x_1000, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fv_x_1000, COLOR_TEXT_MUTED, 0);
    lv_obj_align(fv_x_1000, LV_ALIGN_BOTTOM_RIGHT, -15, 0);

    lv_obj_t * fv_x_title = lv_label_create(card_fv);
    lv_label_set_text(fv_x_title, "Volume (mL)");
    lv_obj_set_style_text_font(fv_x_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fv_x_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(fv_x_title, LV_ALIGN_BOTTOM_MID, 15, -12);

    populate_loops_data();

    /* Oxygenation Statistics Panel (230 x 295) */
    lv_obj_t * oxy_card = lv_obj_create(loops_cont);
    lv_obj_set_size(oxy_card, 230, 295);
    lv_obj_align(oxy_card, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(oxy_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(oxy_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(oxy_card, 1, 0);
    lv_obj_set_style_radius(oxy_card, 10, 0);
    lv_obj_set_style_pad_all(oxy_card, 8, 0);

    lv_obj_t * oxy_title = lv_label_create(oxy_card);
    lv_label_set_text(oxy_title, "OXYGENATION");
    lv_obj_set_style_text_font(oxy_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(oxy_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(oxy_title, LV_ALIGN_TOP_LEFT, 10, 4);

    /* O2 Stats Grid */
    lv_obj_t * stats_lbl1 = lv_label_create(oxy_card);
    lv_label_set_text(stats_lbl1, "FiO2\n#7097ba %#");
    lv_label_set_recolor(stats_lbl1, true);
    lv_obj_set_style_text_font(stats_lbl1, &lv_font_montserrat_12, 0);
    lv_obj_align(stats_lbl1, LV_ALIGN_TOP_LEFT, 10, 45);

    lv_obj_t * stats_val1 = lv_label_create(oxy_card);
    lv_label_set_text(stats_val1, "40");
    lv_obj_set_style_text_font(stats_val1, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(stats_val1, lv_color_hex(0xFF80AB), 0);
    lv_obj_align(stats_val1, LV_ALIGN_TOP_RIGHT, -60, 42);

    /* Vertical Scale bar */
    lv_obj_t * bar_container = lv_obj_create(oxy_card);
    lv_obj_set_size(bar_container, 25, 75);
    lv_obj_align(bar_container, LV_ALIGN_TOP_RIGHT, -12, 38);
    lv_obj_set_style_bg_color(bar_container, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(bar_container, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(bar_container, 1, 0);
    lv_obj_set_style_radius(bar_container, 4, 0);

    lv_obj_t * bar_fill = lv_obj_create(bar_container);
    lv_obj_set_size(bar_fill, 23, 30);
    lv_obj_align(bar_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar_fill, lv_color_hex(0xFF80AB), 0);
    lv_obj_set_style_border_width(bar_fill, 0, 0);
    lv_obj_set_style_radius(bar_fill, 2, 0);

    lv_obj_t * bar_max = lv_label_create(oxy_card);
    lv_label_set_text(bar_max, "100");
    lv_obj_set_style_text_font(bar_max, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bar_max, COLOR_TEXT_MUTED, 0);
    lv_obj_align(bar_max, LV_ALIGN_TOP_RIGHT, -42, 38);

    lv_obj_t * bar_min = lv_label_create(oxy_card);
    lv_label_set_text(bar_min, "21");
    lv_obj_set_style_text_font(bar_min, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bar_min, COLOR_TEXT_MUTED, 0);
    lv_obj_align(bar_min, LV_ALIGN_TOP_RIGHT, -42, 102);

    /* P/F Ratio */
    lv_obj_t * stats_lbl2 = lv_label_create(oxy_card);
    lv_label_set_text(stats_lbl2, "P/F Ratio\n#7097ba mmHg#");
    lv_label_set_recolor(stats_lbl2, true);
    lv_obj_set_style_text_font(stats_lbl2, &lv_font_montserrat_12, 0);
    lv_obj_align(stats_lbl2, LV_ALIGN_TOP_LEFT, 10, 135);

    lbl_pf_ratio = lv_label_create(oxy_card);
    lv_label_set_text(lbl_pf_ratio, "265");
    lv_obj_set_style_text_font(lbl_pf_ratio, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_pf_ratio, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(lbl_pf_ratio, LV_ALIGN_TOP_RIGHT, -20, 132);

    /* O2 Delivery */
    lv_obj_t * stats_lbl3 = lv_label_create(oxy_card);
    lv_label_set_text(stats_lbl3, "O2 Delivery\n#7097ba mL/min#");
    lv_label_set_recolor(stats_lbl3, true);
    lv_obj_set_style_text_font(stats_lbl3, &lv_font_montserrat_12, 0);
    lv_obj_align(stats_lbl3, LV_ALIGN_TOP_LEFT, 10, 215);

    lbl_o2_delivery = lv_label_create(oxy_card);
    lv_label_set_text(lbl_o2_delivery, "268");
    lv_obj_set_style_text_font(lbl_o2_delivery, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_o2_delivery, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(lbl_o2_delivery, LV_ALIGN_TOP_RIGHT, -20, 212);

    /* ==================================================================== */
    /* 4. RIGHT SIDEBAR NAVIGATION MENU                                    */
    /* ==================================================================== */
    lv_obj_t * nav_bar = lv_obj_create(scr);
    lv_obj_set_size(nav_bar, 130, 675);
    lv_obj_set_pos(nav_bar, 1140, 60);
    lv_obj_set_style_bg_color(nav_bar, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(nav_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(nav_bar, 1, 0);
    lv_obj_set_style_radius(nav_bar, 10, 0);
    lv_obj_set_style_pad_all(nav_bar, 6, 0);
    lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(nav_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * nav_items[][2] = {
        {LV_SYMBOL_AUDIO, "Waveforms"},
        {LV_SYMBOL_LIST, "Monitor"},
        {LV_SYMBOL_IMAGE, "Lung Tools"},
        {LV_SYMBOL_FILE, "Trends & Logs"},
        {LV_SYMBOL_BELL, "Alarms"},
        {LV_SYMBOL_SETTINGS, "More"}
    };

    for(int i = 0; i < 6; i++) {
        lv_obj_t * btn = lv_button_create(nav_bar);
        lv_obj_set_size(btn, 114, 98);
        lv_obj_set_style_bg_color(btn, (i == 1) ? COLOR_BTN_NAV_ACTIVE : COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(btn, (i == 1) ? COLOR_ACCENT_BLUE : COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t * icon = lv_label_create(btn);
        lv_label_set_text(icon, nav_items[i][0]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, (i == 1) ? COLOR_ACCENT_BLUE : COLOR_TEXT_MAIN, 0);

        lv_obj_t * txt = lv_label_create(btn);
        lv_label_set_text(txt, nav_items[i][1]);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(txt, COLOR_TEXT_MAIN, 0);
        lv_obj_set_style_pad_top(txt, 4, 0);

        lv_obj_add_event_cb(btn, nav_btn_cb, LV_EVENT_CLICKED, (void*)nav_items[i][1]);
    }

    /* ==================================================================== */
    /* 5. BOTTOM ACTION CONTROL BAR                                         */
    /* ==================================================================== */
    lv_obj_t * bot_bar = lv_obj_create(scr);
    lv_obj_set_size(bot_bar, 1260, 55);
    lv_obj_set_pos(bot_bar, 10, 740);
    lv_obj_set_style_bg_opa(bot_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bot_bar, 0, 0);
    lv_obj_set_style_pad_all(bot_bar, 0, 0);
    lv_obj_set_flex_flow(bot_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bot_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Action Button 1: Silence Alarm */
    lv_obj_t * btn_act1 = lv_button_create(bot_bar);
    lv_obj_set_size(btn_act1, 230, 48);
    lv_obj_set_style_bg_color(btn_act1, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_act1, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_act1, 1, 0);
    lv_obj_t * l1 = lv_label_create(btn_act1);
    lv_label_set_text(l1, LV_SYMBOL_MUTE "  SILENCE ALARM");
    lv_obj_center(l1);

    /* Action Button 2: Pause Alarm */
    lv_obj_t * btn_act2 = lv_button_create(bot_bar);
    lv_obj_set_size(btn_act2, 230, 48);
    lv_obj_set_style_bg_color(btn_act2, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_act2, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_act2, 1, 0);
    lv_obj_t * l2 = lv_label_create(btn_act2);
    lv_label_set_text(l2, LV_SYMBOL_PAUSE "  PAUSE ALARM");
    lv_obj_center(l2);

    /* Action Button 3: START / RESUME */
    lv_obj_t * btn_act3 = lv_button_create(bot_bar);
    lv_obj_set_size(btn_act3, 270, 48);
    lv_obj_set_style_bg_color(btn_act3, lv_color_hex(0x008E3C), 0);
    lv_obj_set_style_border_color(btn_act3, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_border_width(btn_act3, 1, 0);
    lv_obj_t * l3 = lv_label_create(btn_act3);
    lv_label_set_text(l3, LV_SYMBOL_PLAY "  START / RESUME");
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_14, 0);
    lv_obj_center(l3);

    /* Action Button 4: Manual Breath */
    lv_obj_t * btn_act4 = lv_button_create(bot_bar);
    lv_obj_set_size(btn_act4, 230, 48);
    lv_obj_set_style_bg_color(btn_act4, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_act4, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_act4, 1, 0);
    lv_obj_t * l4 = lv_label_create(btn_act4);
    lv_label_set_text(l4, LV_SYMBOL_IMAGE "  MANUAL BREATH");
    lv_obj_center(l4);

    /* Action Button 5: Standby */
    lv_obj_t * btn_act5 = lv_button_create(bot_bar);
    lv_obj_set_size(btn_act5, 230, 48);
    lv_obj_set_style_bg_color(btn_act5, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_act5, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_act5, 1, 0);
    lv_obj_t * l5 = lv_label_create(btn_act5);
    lv_label_set_text(l5, LV_SYMBOL_POWER "  STANDBY");
    lv_obj_center(l5);

    /* Disable scrolling tree filter */
    extern void disable_scroll_recursive(lv_obj_t * obj);
    disable_scroll_recursive(scr);

    /* Set waveform timer callback */
    wave_timer = lv_timer_create(waveform_timer_cb, 50, NULL);

    /* Load screen */
    lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

static void goto_settings_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(wave_timer) {
        lv_timer_delete(wave_timer);
        wave_timer = NULL;
    }
    extern void create_ventilator_settings_screen(void);
    create_ventilator_settings_screen();
}

static void pat_badge_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(wave_timer) {
        lv_timer_delete(wave_timer);
        wave_timer = NULL;
    }
    extern void create_ventilator_patient_screen(void);
    create_ventilator_patient_screen();
}

static void nav_btn_cb(lv_event_t * e)
{
    const char * name = (const char *)lv_event_get_user_data(e);
    if(!name) return;

    if(strcmp(name, "Waveforms") == 0) {
        if(wave_timer) {
            lv_timer_delete(wave_timer);
            wave_timer = NULL;
        }
        extern void create_ventilator_main_screen(void);
        create_ventilator_main_screen();
        return;
    }

    if(strcmp(name, "More") == 0) {
        if(wave_timer) {
            lv_timer_delete(wave_timer);
            wave_timer = NULL;
        }
        extern void create_ventilator_settings_screen(void);
        create_ventilator_settings_screen();
        return;
    }
}
