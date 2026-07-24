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
static lv_obj_t * lbl_val_peep = NULL;
static lv_obj_t * lbl_val_mv = NULL;
static lv_obj_t * lbl_val_rr = NULL;
static lv_obj_t * lbl_val_tv = NULL;
static lv_obj_t * lbl_val_fio2 = NULL;

/* Forward declarations for screen callbacks */
static void nav_btn_cb(lv_event_t * e);

/* Helper to Create a Small Vital Card */
static lv_obj_t * create_vital_tile(lv_obj_t * parent, const char * title, const char * unit, const char * val_str, int width, int height, lv_obj_t ** out_label)
{
    lv_obj_t * tile = lv_obj_create(parent);
    lv_obj_set_size(tile, width, height);
    lv_obj_set_style_bg_color(tile, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(tile, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_radius(tile, 8, 0);
    lv_obj_set_style_pad_all(tile, 6, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    /* Header Container (Title + Unit) */
    lv_obj_t * head_box = lv_obj_create(tile);
    lv_obj_set_size(head_box, width - 16, 22);
    lv_obj_set_style_bg_opa(head_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head_box, 0, 0);
    lv_obj_set_style_pad_all(head_box, 0, 0);

    lv_obj_t * lbl_t = lv_label_create(head_box);
    lv_label_set_text(lbl_t, title);
    lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_t, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_t, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * lbl_u = lv_label_create(head_box);
    lv_label_set_text(lbl_u, unit);
    lv_obj_set_style_text_font(lbl_u, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_u, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_u, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Big Value Text */
    lv_obj_t * lbl_v = lv_label_create(tile);
    lv_label_set_text(lbl_v, val_str);
    lv_obj_set_style_text_font(lbl_v, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_v, COLOR_TEXT_MAIN, 0);
    lv_obj_set_style_pad_left(lbl_v, 4, 0);

    if(out_label) {
        *out_label = lbl_v;
    }

    return tile;
}

/* Helper to Create Quick Setting Box */
static void create_quick_setting_box(lv_obj_t * parent, const char * title, const char * unit, const char * val_str, const char * range_str)
{
    lv_obj_t * box = lv_obj_create(parent);
    lv_obj_set_size(box, 120, 125);
    lv_obj_set_style_bg_color(box, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(box, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_pad_all(box, 4, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Title + Unit */
    lv_obj_t * lbl_t = lv_label_create(box);
    lv_label_set_text_fmt(lbl_t, "%s\n#7097ba %s#", title, unit);
    lv_label_set_recolor(lbl_t, true);
    lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_t, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_align(lbl_t, LV_TEXT_ALIGN_CENTER, 0);

    /* Value + Up/Down Arrows Container */
    lv_obj_t * val_cont = lv_obj_create(box);
    lv_obj_set_size(val_cont, 110, 42);
    lv_obj_set_style_bg_opa(val_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(val_cont, 0, 0);
    lv_obj_set_style_pad_all(val_cont, 0, 0);

    lv_obj_t * lbl_v = lv_label_create(val_cont);
    lv_label_set_text(lbl_v, val_str);
    lv_obj_set_style_text_font(lbl_v, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_v, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_v, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t * lbl_arr = lv_label_create(val_cont);
    lv_label_set_text(lbl_arr, "^\nv");
    lv_obj_set_style_text_font(lbl_arr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_arr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(lbl_arr, LV_ALIGN_RIGHT_MID, -4, 0);

    /* Range Subtext */
    lv_obj_t * lbl_r = lv_label_create(box);
    lv_label_set_text(lbl_r, range_str);
    lv_obj_set_style_text_font(lbl_r, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_r, COLOR_TEXT_MUTED, 0);
}

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
    static float live_peep = 5.0f;
    static float live_mv = 6.8f;
    static float live_rr = 16.0f;
    static float live_tv = 420.0f;
    static float live_fio2 = 40.0f;

    if(t < prev_t || (lbl_val_ppeak && strcmp(lv_label_get_text(lbl_val_ppeak), "32") == 0)) {
        live_ppeak = 31.0f + (float)(rand() % 20) / 10.0f;
        live_peep = 4.8f + (float)(rand() % 5) / 10.0f;
        live_mv = 6.5f + (float)(rand() % 7) / 10.0f;
        live_tv = 410.0f + (float)(rand() % 20);

        if(lbl_val_ppeak) lv_label_set_text_fmt(lbl_val_ppeak, "%.1f", live_ppeak);
        if(lbl_val_peep) lv_label_set_text_fmt(lbl_val_peep, "%.1f", live_peep);
        if(lbl_val_mv) lv_label_set_text_fmt(lbl_val_mv, "%.1f", live_mv);
        if(lbl_val_rr) lv_label_set_text_fmt(lbl_val_rr, "%.0f", live_rr);
        if(lbl_val_tv) lv_label_set_text_fmt(lbl_val_tv, "%.0f", live_tv);
        if(lbl_val_fio2) lv_label_set_text_fmt(lbl_val_fio2, "%.0f", live_fio2);
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

    /* ==================================================================== */
    /* 2. LEFT PANEL - PATIENT VITAL READOUTS                               */
    /* ==================================================================== */
    lv_obj_t * left_panel = lv_obj_create(scr);
    lv_obj_set_size(left_panel, 150, 475);
    lv_obj_set_pos(left_panel, 10, 60);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_set_style_pad_all(left_panel, 0, 0);
    lv_obj_set_flex_flow(left_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_vital_tile(left_panel, "Ppeak", "cmH2O", "32", 146, 72, &lbl_val_ppeak);
    create_vital_tile(left_panel, "PEEP", "cmH2O", "5", 146, 72, &lbl_val_peep);
    create_vital_tile(left_panel, "Vt", "mL", "420", 146, 72, &lbl_val_tv);
    create_vital_tile(left_panel, "Minute Vol", "L/min", "6.8", 146, 72, &lbl_val_mv);
    create_vital_tile(left_panel, "Resp Rate", "bpm", "16", 146, 72, &lbl_val_rr);
    create_vital_tile(left_panel, "FiO2", "%", "40", 146, 72, &lbl_val_fio2);

    /* ==================================================================== */
    /* 3. CENTER GRAPHICS SECTION (Waveforms + Loops)                       */
    /* ==================================================================== */
    lv_obj_t * center_cont = lv_obj_create(scr);
    lv_obj_set_size(center_cont, 690, 675);
    lv_obj_set_pos(center_cont, 170, 60);
    lv_obj_set_style_bg_opa(center_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(center_cont, 0, 0);
    lv_obj_set_style_pad_all(center_cont, 0, 0);

    /* Stacked Waveforms Panel (Top half: 690 x 240) */
    lv_obj_t * wave_cont = lv_obj_create(center_cont);
    lv_obj_set_size(wave_cont, 690, 240);
    lv_obj_align(wave_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(wave_cont, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(wave_cont, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(wave_cont, 1, 0);
    lv_obj_set_style_radius(wave_cont, 10, 0);
    lv_obj_set_style_pad_all(wave_cont, 4, 0);

    /* Pressure Waveform */
    lv_obj_t * box_p = lv_obj_create(wave_cont);
    lv_obj_set_size(box_p, 678, 76);
    lv_obj_align(box_p, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(box_p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_p, 0, 0);
    lv_obj_t * lbl_p = lv_label_create(box_p);
    lv_label_set_text(lbl_p, "Pressure cmH2O");
    lv_obj_set_style_text_font(lbl_p, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_p, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_p, LV_ALIGN_TOP_LEFT, 10, 0);

    chart_pressure = lv_chart_create(box_p);
    lv_obj_set_size(chart_pressure, 620, 58);
    lv_obj_align(chart_pressure, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_chart_set_type(chart_pressure, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_pressure, 120);
    lv_chart_set_update_mode(chart_pressure, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_axis_range(chart_pressure, LV_CHART_AXIS_PRIMARY_Y, -10, 60);
    lv_obj_set_style_bg_opa(chart_pressure, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_pressure, 0, 0);
    lv_obj_set_style_line_opa(chart_pressure, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_pressure, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_pressure, 0, 0, LV_PART_INDICATOR);
    ser_pressure = lv_chart_add_series(chart_pressure, COLOR_ACCENT_BLUE, LV_CHART_AXIS_PRIMARY_Y);

    /* Flow Waveform */
    lv_obj_t * box_f = lv_obj_create(wave_cont);
    lv_obj_set_size(box_f, 678, 76);
    lv_obj_align(box_f, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_bg_opa(box_f, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_f, 0, 0);
    lv_obj_t * lbl_f = lv_label_create(box_f);
    lv_label_set_text(lbl_f, "Flow L/min");
    lv_obj_set_style_text_font(lbl_f, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_f, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_f, LV_ALIGN_TOP_LEFT, 10, 0);

    chart_flow = lv_chart_create(box_f);
    lv_obj_set_size(chart_flow, 620, 58);
    lv_obj_align(chart_flow, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
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

    /* Volume Waveform */
    lv_obj_t * box_v = lv_obj_create(wave_cont);
    lv_obj_set_size(box_v, 678, 76);
    lv_obj_align(box_v, LV_ALIGN_TOP_MID, 0, 156);
    lv_obj_set_style_bg_opa(box_v, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_v, 0, 0);
    lv_obj_t * lbl_v_title = lv_label_create(box_v);
    lv_label_set_text(lbl_v_title, "Volume mL");
    lv_obj_set_style_text_font(lbl_v_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_v_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_v_title, LV_ALIGN_TOP_LEFT, 10, 0);

    chart_volume = lv_chart_create(box_v);
    lv_obj_set_size(chart_volume, 620, 58);
    lv_obj_align(chart_volume, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
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

    /* Loops Panel (Bottom half: 690 x 235) */
    lv_obj_t * loops_cont = lv_obj_create(center_cont);
    lv_obj_set_size(loops_cont, 690, 235);
    lv_obj_align(loops_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(loops_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(loops_cont, 0, 0);
    lv_obj_set_style_pad_all(loops_cont, 0, 0);

    /* Loop Card 1: PRESSURE-VOLUME LOOP */
    lv_obj_t * card_pv = lv_obj_create(loops_cont);
    lv_obj_set_size(card_pv, 245, 235);
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
    lv_obj_align(lbl_pv_title, LV_ALIGN_TOP_LEFT, 6, 2);

    chart_pv_loop = lv_chart_create(card_pv);
    lv_obj_set_size(chart_pv_loop, 220, 180);
    lv_obj_align(chart_pv_loop, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_chart_set_type(chart_pv_loop, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(chart_pv_loop, 60);
    lv_chart_set_range(chart_pv_loop, LV_CHART_AXIS_PRIMARY_X, 0, 1000);
    lv_chart_set_range(chart_pv_loop, LV_CHART_AXIS_PRIMARY_Y, -15, 60);
    lv_obj_set_style_bg_opa(chart_pv_loop, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_pv_loop, 0, 0);
    lv_obj_set_style_line_width(chart_pv_loop, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_pv_loop, 0, 0, LV_PART_INDICATOR);
    ser_pv_loop = lv_chart_add_series(chart_pv_loop, lv_color_hex(0xA020F0), LV_CHART_AXIS_PRIMARY_Y);

    /* Loop Card 2: FLOW-VOLUME LOOP */
    lv_obj_t * card_fv = lv_obj_create(loops_cont);
    lv_obj_set_size(card_fv, 245, 235);
    lv_obj_align(card_fv, LV_ALIGN_LEFT_MID, 255, 0);
    lv_obj_set_style_bg_color(card_fv, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_fv, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_fv, 1, 0);
    lv_obj_set_style_radius(card_fv, 10, 0);
    lv_obj_set_style_pad_all(card_fv, 6, 0);

    lv_obj_t * lbl_fv_title = lv_label_create(card_fv);
    lv_label_set_text(lbl_fv_title, "FLOW-VOLUME LOOP");
    lv_obj_set_style_text_font(lbl_fv_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_fv_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_fv_title, LV_ALIGN_TOP_LEFT, 6, 2);

    chart_fv_loop = lv_chart_create(card_fv);
    lv_obj_set_size(chart_fv_loop, 220, 180);
    lv_obj_align(chart_fv_loop, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_chart_set_type(chart_fv_loop, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(chart_fv_loop, 60);
    lv_chart_set_range(chart_fv_loop, LV_CHART_AXIS_PRIMARY_X, 0, 1000);
    lv_chart_set_range(chart_fv_loop, LV_CHART_AXIS_PRIMARY_Y, -80, 80);
    lv_obj_set_style_bg_opa(chart_fv_loop, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_fv_loop, 0, 0);
    lv_obj_set_style_line_width(chart_fv_loop, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_fv_loop, 0, 0, LV_PART_INDICATOR);
    ser_fv_loop = lv_chart_add_series(chart_fv_loop, COLOR_ACCENT_GREEN, LV_CHART_AXIS_PRIMARY_Y);

    populate_loops_data();

    /* Oxygenation Statistics Panel (690 - 510 = 180 x 235) */
    lv_obj_t * oxy_card = lv_obj_create(loops_cont);
    lv_obj_set_size(oxy_card, 180, 235);
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
    lv_obj_align(oxy_title, LV_ALIGN_TOP_LEFT, 6, 2);

    /* O2 Stats Grid */
    lv_obj_t * stats_lbl1 = lv_label_create(oxy_card);
    lv_label_set_text(stats_lbl1, "FiO2\n#7097ba %#");
    lv_label_set_recolor(stats_lbl1, true);
    lv_obj_set_style_text_font(stats_lbl1, &lv_font_montserrat_12, 0);
    lv_obj_align(stats_lbl1, LV_ALIGN_TOP_LEFT, 6, 32);

    lv_obj_t * stats_val1 = lv_label_create(oxy_card);
    lv_label_set_text(stats_val1, "40");
    lv_obj_set_style_text_font(stats_val1, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(stats_val1, lv_color_hex(0xFF80AB), 0);
    lv_obj_align(stats_val1, LV_ALIGN_TOP_RIGHT, -12, 32);

    lv_obj_t * stats_lbl2 = lv_label_create(oxy_card);
    lv_label_set_text(stats_lbl2, "P/F Ratio\n#7097ba mmHg#");
    lv_label_set_recolor(stats_lbl2, true);
    lv_obj_set_style_text_font(stats_lbl2, &lv_font_montserrat_12, 0);
    lv_obj_align(stats_lbl2, LV_ALIGN_TOP_LEFT, 6, 95);

    lv_obj_t * stats_val2 = lv_label_create(oxy_card);
    lv_label_set_text(stats_val2, "265");
    lv_obj_set_style_text_font(stats_val2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(stats_val2, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(stats_val2, LV_ALIGN_TOP_RIGHT, -12, 95);

    lv_obj_t * stats_lbl3 = lv_label_create(oxy_card);
    lv_label_set_text(stats_lbl3, "O2 Delivery\n#7097ba mL/min#");
    lv_label_set_recolor(stats_lbl3, true);
    lv_obj_set_style_text_font(stats_lbl3, &lv_font_montserrat_12, 0);
    lv_obj_align(stats_lbl3, LV_ALIGN_TOP_LEFT, 6, 158);

    lv_obj_t * stats_val3 = lv_label_create(oxy_card);
    lv_label_set_text(stats_val3, "268");
    lv_obj_set_style_text_font(stats_val3, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(stats_val3, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(stats_val3, LV_ALIGN_TOP_RIGHT, -12, 158);

    /* ==================================================================== */
    /* 4. RIGHT SIDE PANEL (Vitals + Alarms List)                           */
    /* ==================================================================== */
    lv_obj_t * right_panel = lv_obj_create(scr);
    lv_obj_set_size(right_panel, 270, 675);
    lv_obj_set_pos(right_panel, 870, 60);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 0, 0);

    /* Card 1: Vitals Display Panel */
    lv_obj_t * card_vitals = lv_obj_create(right_panel);
    lv_obj_set_size(card_vitals, 270, 240);
    lv_obj_align(card_vitals, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(card_vitals, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_vitals, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_vitals, 1, 0);
    lv_obj_set_style_radius(card_vitals, 10, 0);
    lv_obj_set_style_pad_all(card_vitals, 8, 0);

    lv_obj_t * vit_title = lv_label_create(card_vitals);
    lv_label_set_text(vit_title, "VITALS");
    lv_obj_set_style_text_font(vit_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(vit_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(vit_title, LV_ALIGN_TOP_LEFT, 6, 2);

    /* 1. HR bpm */
    lv_obj_t * vit_hr_lbl = lv_label_create(card_vitals);
    lv_label_set_text(vit_hr_lbl, "HR\nbpm");
    lv_obj_set_style_text_font(vit_hr_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(vit_hr_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(vit_hr_lbl, LV_ALIGN_TOP_LEFT, 10, 30);

    lv_obj_t * vit_hr_val = lv_label_create(card_vitals);
    lv_label_set_text(vit_hr_val, "78");
    lv_obj_set_style_text_font(vit_hr_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(vit_hr_val, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(vit_hr_val, LV_ALIGN_TOP_RIGHT, -15, 24);

    /* 2. SpO2 % */
    lv_obj_t * vit_spo2_lbl = lv_label_create(card_vitals);
    lv_label_set_text(vit_spo2_lbl, "SpO2\n%");
    lv_obj_set_style_text_font(vit_spo2_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(vit_spo2_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(vit_spo2_lbl, LV_ALIGN_TOP_LEFT, 10, 82);

    lv_obj_t * vit_spo2_val = lv_label_create(card_vitals);
    lv_label_set_text(vit_spo2_val, "98");
    lv_obj_set_style_text_font(vit_spo2_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(vit_spo2_val, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(vit_spo2_val, LV_ALIGN_TOP_RIGHT, -15, 76);

    /* 3. NIBP mmHg */
    lv_obj_t * vit_nibp_lbl = lv_label_create(card_vitals);
    lv_label_set_text(vit_nibp_lbl, "NIBP\nmmHg");
    lv_obj_set_style_text_font(vit_nibp_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(vit_nibp_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(vit_nibp_lbl, LV_ALIGN_TOP_LEFT, 10, 134);

    lv_obj_t * vit_nibp_val = lv_label_create(card_vitals);
    lv_label_set_text(vit_nibp_val, "120 / 80\n#7097ba (93)#");
    lv_label_set_recolor(vit_nibp_val, true);
    lv_obj_set_style_text_font(vit_nibp_val, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(vit_nibp_val, lv_color_hex(0xFF80AB), 0);
    lv_obj_align(vit_nibp_val, LV_ALIGN_TOP_RIGHT, -15, 128);

    /* 4. TEMP °C */
    lv_obj_t * vit_temp_lbl = lv_label_create(card_vitals);
    lv_label_set_text(vit_temp_lbl, "TEMP\n°C");
    lv_obj_set_style_text_font(vit_temp_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(vit_temp_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(vit_temp_lbl, LV_ALIGN_TOP_LEFT, 10, 186);

    lv_obj_t * vit_temp_val = lv_label_create(card_vitals);
    lv_label_set_text(vit_temp_val, "36.6");
    lv_obj_set_style_text_font(vit_temp_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(vit_temp_val, lv_color_hex(0xB388FF), 0);
    lv_obj_align(vit_temp_val, LV_ALIGN_TOP_RIGHT, -15, 180);

    /* Card 2: Alarms Panel */
    lv_obj_t * card_alarms = lv_obj_create(right_panel);
    lv_obj_set_size(card_alarms, 270, 235);
    lv_obj_align(card_alarms, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(card_alarms, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_alarms, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_alarms, 1, 0);
    lv_obj_set_style_radius(card_alarms, 10, 0);
    lv_obj_set_style_pad_all(card_alarms, 8, 0);

    lv_obj_t * al_title = lv_label_create(card_alarms);
    lv_label_set_text(al_title, "ALARMS");
    lv_obj_set_style_text_font(al_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(al_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(al_title, LV_ALIGN_TOP_LEFT, 6, 2);

    /* Alarm List Rows */
    lv_obj_t * row1 = lv_label_create(card_alarms);
    lv_label_set_text(row1, LV_SYMBOL_BELL "  HIGH PRESSURE\n#d50000 Ppeak above limit#");
    lv_label_set_recolor(row1, true);
    lv_obj_set_style_text_font(row1, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(row1, COLOR_TEXT_MAIN, 0);
    lv_obj_align(row1, LV_ALIGN_TOP_LEFT, 6, 30);

    lv_obj_t * row1_t = lv_label_create(card_alarms);
    lv_label_set_text(row1_t, "10:24");
    lv_obj_set_style_text_font(row1_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(row1_t, COLOR_TEXT_MUTED, 0);
    lv_obj_align(row1_t, LV_ALIGN_TOP_RIGHT, -6, 30);

    lv_obj_t * row2 = lv_label_create(card_alarms);
    lv_label_set_text(row2, LV_SYMBOL_WARNING "  LOW MINUTE VOLUME\n#ffd600 MV below limit#");
    lv_label_set_recolor(row2, true);
    lv_obj_set_style_text_font(row2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(row2, COLOR_TEXT_MAIN, 0);
    lv_obj_align(row2, LV_ALIGN_TOP_LEFT, 6, 82);

    lv_obj_t * row2_t = lv_label_create(card_alarms);
    lv_label_set_text(row2_t, "10:20");
    lv_obj_set_style_text_font(row2_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(row2_t, COLOR_TEXT_MUTED, 0);
    lv_obj_align(row2_t, LV_ALIGN_TOP_RIGHT, -6, 82);

    lv_obj_t * row3 = lv_label_create(card_alarms);
    lv_label_set_text(row3, LV_SYMBOL_WARNING "  DISCONNECTION\n#ffd600 Check patient circuit#");
    lv_label_set_recolor(row3, true);
    lv_obj_set_style_text_font(row3, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(row3, COLOR_TEXT_MAIN, 0);
    lv_obj_align(row3, LV_ALIGN_TOP_LEFT, 6, 134);

    lv_obj_t * row3_t = lv_label_create(card_alarms);
    lv_label_set_text(row3_t, "10:18");
    lv_obj_set_style_text_font(row3_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(row3_t, COLOR_TEXT_MUTED, 0);
    lv_obj_align(row3_t, LV_ALIGN_TOP_RIGHT, -6, 134);

    /* Alarm History Button */
    lv_obj_t * btn_al_hist = lv_button_create(card_alarms);
    lv_obj_set_size(btn_al_hist, 238, 40);
    lv_obj_align(btn_al_hist, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_color(btn_al_hist, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_al_hist, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_al_hist, 1, 0);
    lv_obj_set_style_radius(btn_al_hist, 8, 0);

    lv_obj_t * btn_al_hist_lbl = lv_label_create(btn_al_hist);
    lv_label_set_text(btn_al_hist_lbl, "ALARM HISTORY");
    lv_obj_set_style_text_font(btn_al_hist_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_al_hist_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(btn_al_hist_lbl);

    /* ==================================================================== */
    /* 5. QUICK SETTINGS CONTROLS BAR                                       */
    /* ==================================================================== */
    lv_obj_t * quick_cont = lv_obj_create(scr);
    lv_obj_set_size(quick_cont, 850, 185);
    lv_obj_set_pos(quick_cont, 10, 545);
    lv_obj_set_style_bg_color(quick_cont, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(quick_cont, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(quick_cont, 1, 0);
    lv_obj_set_style_radius(quick_cont, 10, 0);
    lv_obj_set_style_pad_all(quick_cont, 8, 0);

    lv_obj_t * q_title = lv_label_create(quick_cont);
    lv_label_set_text(q_title, "QUICK SETTINGS");
    lv_obj_set_style_text_font(q_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(q_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(q_title, LV_ALIGN_TOP_LEFT, 6, 2);

    lv_obj_t * q_boxes = lv_obj_create(quick_cont);
    lv_obj_set_size(q_boxes, 830, 140);
    lv_obj_align(q_boxes, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(q_boxes, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(q_boxes, 0, 0);
    lv_obj_set_style_pad_all(q_boxes, 0, 0);
    lv_obj_set_flex_flow(q_boxes, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(q_boxes, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_quick_setting_box(q_boxes, "FiO2", "%", "40", "21 - 100");
    create_quick_setting_box(q_boxes, "Tidal Volume", "mL", "420", "100 - 1000");
    create_quick_setting_box(q_boxes, "Respiratory Rate", "bpm", "16", "4 - 40");
    create_quick_setting_box(q_boxes, "PEEP", "cmH2O", "5", "0 - 20");
    create_quick_setting_box(q_boxes, "Insp. Time", "sec", "1.00", "0.20 - 3.00");
    create_quick_setting_box(q_boxes, "Flow", "L/min", "60", "20 - 100");

    /* ==================================================================== */
    /* 6. RIGHT SIDEBAR NAVIGATION MENU                                    */
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
    /* 7. BOTTOM ACTION CONTROL BAR                                         */
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
}
