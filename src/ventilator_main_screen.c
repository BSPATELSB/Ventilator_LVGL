#include "ventilator_main_screen.h"
#include "ventilator_time_screen.h"
#include "battery_detect.h"
#include "theme_manager.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/lvgl_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Colors matching reference UI screenshot */
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

/* Waveform Charts references */
static lv_obj_t * chart_pressure = NULL;
static lv_obj_t * chart_flow = NULL;
static lv_obj_t * chart_volume = NULL;

static lv_chart_series_t * ser_pressure = NULL;
static lv_chart_series_t * ser_flow = NULL;
static lv_chart_series_t * ser_volume = NULL;

static lv_timer_t * wave_timer = NULL;
static float wave_phase_counter = 0.0f;

/* References to live parameter widgets */
static lv_obj_t * lbl_val_paw = NULL;
static lv_obj_t * bar_paw = NULL;
static lv_obj_t * lbl_val_ppeak = NULL;
static lv_obj_t * lbl_val_peep = NULL;
static lv_obj_t * lbl_val_mv = NULL;
static lv_obj_t * lbl_val_rr = NULL;
static lv_obj_t * lbl_val_tv = NULL;
static lv_obj_t * lbl_val_fio2 = NULL;
static lv_obj_t * lbl_val_o2 = NULL;
static lv_obj_t * lbl_val_spo2 = NULL;
static lv_obj_t * lbl_val_pulse = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * bat_lbl = NULL;

/* Forward declarations for screen callbacks */
static void back_to_main_cb(lv_event_t * e);
static void nav_btn_cb(lv_event_t * e);
static void pat_badge_click_cb(lv_event_t * e);

static void breathing_anim_cb(void * obj, int32_t scale)
{
    lv_image_set_scale((lv_obj_t *)obj, scale);
}

typedef enum {
    WAVE_TYPE_PRESSURE = 0,
    WAVE_TYPE_FLOW = 1,
    WAVE_TYPE_VOLUME = 2
} wave_type_t;

static void custom_chart_draw_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_DRAW_MAIN_BEGIN) return;

    lv_obj_t * obj = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    uintptr_t type_val = (uintptr_t)lv_event_get_user_data(e);
    wave_type_t wave_type = (wave_type_t)type_val;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    int32_t border_w = lv_obj_get_style_border_width(obj, LV_PART_MAIN);
    int32_t pad_l = lv_obj_get_style_pad_left(obj, LV_PART_MAIN) + border_w;
    int32_t pad_t = lv_obj_get_style_pad_top(obj, LV_PART_MAIN) + border_w;
    int32_t w = lv_obj_get_content_width(obj);
    int32_t h = lv_obj_get_content_height(obj);

    int32_t x_ofs = coords.x1 + pad_l;
    int32_t y_ofs = coords.y1 + pad_t;

    lv_chart_t * chart = (lv_chart_t *)obj;
    lv_chart_series_t * ser = lv_chart_get_series_next(obj, NULL);
    if(!ser) return;

    uint32_t point_cnt = chart->point_cnt;
    if(point_cnt < 2) return;

    int32_t * y_points = lv_chart_get_y_array(obj, ser);
    if(!y_points) return;

    int32_t start_point = (chart->update_mode == LV_CHART_UPDATE_MODE_SHIFT) ? ser->start_point : 0;
    int32_t min_v = chart->ymin[ser->y_axis_sec];
    int32_t max_v = chart->ymax[ser->y_axis_sec];

    /* 1. Draw Grid Lines matching reference graph screenshot */
    lv_draw_line_dsc_t grid_dsc;
    lv_draw_line_dsc_init(&grid_dsc);
    grid_dsc.color = lv_color_hex(0x162438);
    grid_dsc.width = 1;
    grid_dsc.opa = LV_OPA_50;

    int v_lines = 12;
    for(int i = 0; i <= v_lines; i++) {
        int32_t gx = x_ofs + (w * i) / v_lines;
        grid_dsc.p1.x = gx;
        grid_dsc.p1.y = y_ofs;
        grid_dsc.p2.x = gx;
        grid_dsc.p2.y = y_ofs + h;
        lv_draw_line(layer, &grid_dsc);
    }

    int h_lines = 4;
    for(int i = 0; i <= h_lines; i++) {
        int32_t gy = y_ofs + (h * i) / h_lines;
        grid_dsc.p1.x = x_ofs;
        grid_dsc.p1.y = gy;
        grid_dsc.p2.x = x_ofs + w;
        grid_dsc.p2.y = gy;
        lv_draw_line(layer, &grid_dsc);
    }

    /* 2. Draw Translucent Area Fill Under/Over Baseline */
    lv_draw_line_dsc_t fill_dsc;
    lv_draw_line_dsc_init(&fill_dsc);

    int32_t step_w = (w / (point_cnt - 1)) + 1;
    if(step_w < 2) step_w = 2;
    fill_dsc.width = step_w;

    lv_color_t fill_color;
    int32_t y_base = y_ofs + h;

    if(wave_type == WAVE_TYPE_PRESSURE) {
        fill_color = lv_color_hex(0x005b7f); // Cyan fill
        fill_dsc.opa = LV_OPA_40;
        y_base = y_ofs + h;
    } else if(wave_type == WAVE_TYPE_FLOW) {
        fill_color = lv_color_hex(0x7f1818); // Dark Red fill
        fill_dsc.opa = LV_OPA_40;
        y_base = (int32_t)lv_map(0, min_v, max_v, y_ofs + h, y_ofs);
    } else {
        fill_color = lv_color_hex(0x755600); // Yellow/Amber fill
        fill_dsc.opa = LV_OPA_40;
        y_base = y_ofs + h;
    }
    fill_dsc.color = fill_color;

    for(uint32_t i = 0; i < point_cnt; i++) {
        int32_t p_act = (start_point + i) % point_cnt;
        int32_t val = y_points[p_act];
        if(val == LV_CHART_POINT_NONE) continue;

        int32_t px = x_ofs + (int32_t)((w * i) / (point_cnt - 1));
        int32_t py = (int32_t)lv_map(val, min_v, max_v, y_ofs + h, y_ofs);

        fill_dsc.p1.x = px;
        fill_dsc.p1.y = y_base;
        fill_dsc.p2.x = px;
        fill_dsc.p2.y = py;

        lv_draw_line(layer, &fill_dsc);
    }
}

/* Generate exact clinical ventilator wave points matching the target screenshot */
static void waveform_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    wave_phase_counter += (1.0f / 30.0f);
    if(wave_phase_counter >= 1.0f) wave_phase_counter -= 1.0f;

    float t = wave_phase_counter;
    float p_val = 8.0f;
    float f_val = 0.0f;
    float v_val = 0.0f;

    /* 1. Pressure Curve (cmH2O): Baseline 8, Peak 30 cmH2O */
    if(t < 0.05f) {
        p_val = 8.0f + 0.2f * sinf(t * 100.0f);
    } else if(t < 0.18f) {
        float r = (t - 0.05f) / 0.13f;
        p_val = 8.0f + 22.0f * (0.5f * (1.0f - cosf(r * M_PI)));
    } else if(t < 0.35f) {
        float r = (t - 0.18f) / 0.17f;
        p_val = 30.0f - 2.5f * sinf(r * M_PI) + 1.2f * sinf(r * 2.0f * M_PI);
    } else if(t < 0.50f) {
        float r = (t - 0.35f) / 0.15f;
        p_val = 8.0f + 20.0f * expf(-4.5f * r);
    } else {
        p_val = 8.0f + 0.3f * sinf(t * 80.0f);
    }

    /* 2. Flow Curve (L/min): Inspiratory Peak +52, Expiratory Peak -62 */
    if(t < 0.05f) {
        f_val = 0.0f;
    } else if(t < 0.10f) {
        float r = (t - 0.05f) / 0.05f;
        f_val = 52.0f * sinf(r * M_PI / 2.0f);
    } else if(t < 0.32f) {
        float r = (t - 0.10f) / 0.22f;
        f_val = 52.0f * (1.0f - 0.75f * r) * expf(-1.2f * r);
    } else if(t < 0.35f) {
        f_val = 0.0f;
    } else if(t < 0.42f) {
        float r = (t - 0.35f) / 0.07f;
        f_val = -62.0f * sinf(r * M_PI / 2.0f);
    } else if(t < 0.75f) {
        float r = (t - 0.42f) / 0.33f;
        f_val = -62.0f * expf(-3.8f * r);
    } else {
        f_val = 0.0f;
    }

    /* 3. Volume Curve (mL): Baseline 0 to Peak 480 mL */
    if(t < 0.05f) {
        v_val = 0.0f;
    } else if(t < 0.32f) {
        float r = (t - 0.05f) / 0.27f;
        v_val = 480.0f * (0.5f * (1.0f - cosf(r * M_PI)));
    } else if(t < 0.35f) {
        v_val = 480.0f;
    } else if(t < 0.75f) {
        float r = (t - 0.35f) / 0.40f;
        v_val = 480.0f * expf(-4.0f * r);
    } else {
        v_val = 0.0f;
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

    /* Update Paw real-time digits and gauge */
    if(lbl_val_paw) {
        lv_label_set_text_fmt(lbl_val_paw, "%d", (int)p_val);
    }
    if(bar_paw) {
        lv_bar_set_value(bar_paw, (int)p_val, LV_ANIM_OFF);
    }

    /* Fluctuate other parameters once per breath cycle */
    static float prev_t = 0.0f;
    static float live_ppeak = 32.0f;
    static float live_peep = 5.0f;
    static float live_mv = 6.8f;
    static float live_rr = 16.0f;
    static float live_tv = 420.0f;
    static float live_fio2 = 40.0f;
    static float live_o2 = 40.0f;
    static float live_spo2 = 98.0f;
    static float live_pulse = 78.0f;

    if(t < prev_t || (lbl_val_ppeak && strcmp(lv_label_get_text(lbl_val_ppeak), "32") == 0)) {
        live_ppeak = 31.0f + (float)(rand() % 20) / 10.0f;
        live_peep = 4.8f + (float)(rand() % 5) / 10.0f;
        live_mv = 6.5f + (float)(rand() % 7) / 10.0f;
        live_tv = 410.0f + (float)(rand() % 20);
        live_spo2 = 97.0f + (float)(rand() % 3);
        live_pulse = 76.0f + (float)(rand() % 5);

        if(lbl_val_ppeak) lv_label_set_text_fmt(lbl_val_ppeak, "%.1f", live_ppeak);
        if(lbl_val_peep) lv_label_set_text_fmt(lbl_val_peep, "%.1f", live_peep);
        if(lbl_val_mv) lv_label_set_text_fmt(lbl_val_mv, "%.1f", live_mv);
        if(lbl_val_rr) lv_label_set_text_fmt(lbl_val_rr, "%.0f", live_rr);
        if(lbl_val_tv) lv_label_set_text_fmt(lbl_val_tv, "%.0f", live_tv);
        if(lbl_val_fio2) lv_label_set_text_fmt(lbl_val_fio2, "%.0f", live_fio2);
        if(lbl_val_o2) lv_label_set_text_fmt(lbl_val_o2, "%.0f", live_o2);
        if(lbl_val_spo2) lv_label_set_text_fmt(lbl_val_spo2, "%.0f", live_spo2);
        if(lbl_val_pulse) lv_label_set_text_fmt(lbl_val_pulse, "%.0f", live_pulse);
    }
    /* Update real-time clock label */
    if(lbl_clock) {
        time_t raw_time = ventilator_get_current_time(NULL);
        struct tm * time_info;
        time_info = localtime(&raw_time);

        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%b %d, %Y\n%I:%M %p", time_info);

        if(strcmp(lv_label_get_text(lbl_clock), clock_buf) != 0) {
            lv_label_set_text(lbl_clock, clock_buf);
        }
    }

    if(bat_lbl) {
        battery_update_label(bat_lbl);
    }

    prev_t = t;
}

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
    lv_obj_set_size(box, 120, 140);
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

/* Navigation Callback for Sidebar Buttons (Fading Screen Transition) */
static void nav_btn_cb(lv_event_t * e)
{
    const char * name = (const char *)lv_event_get_user_data(e);
    if(!name) return;

    if(strcmp(name, "Waveforms") == 0) {
        return; /* Already on main dashboard */
    }

    if(strcmp(name, "Monitor") == 0) {
        if(wave_timer) {
            lv_timer_delete(wave_timer);
            wave_timer = NULL;
        }
        extern void create_ventilator_monitoring_screen(void);
        create_ventilator_monitoring_screen();
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

    if(wave_timer) {
        lv_timer_delete(wave_timer);
        wave_timer = NULL;
    }

    /* Create New Sub-Screen for Fading Transition */
    lv_obj_t * sub_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sub_scr, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(sub_scr, LV_OPA_COVER, 0);

    /* Top Bar */
    lv_obj_t * hdr = lv_obj_create(sub_scr);
    lv_obj_set_size(hdr, 1280, 60);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(hdr, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);

    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text_fmt(title, "MediVent  •  %s Page", name);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);

    /* Back Button */
    lv_obj_t * back_btn = lv_button_create(hdr);
    lv_obj_set_size(back_btn, 220, 42);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(back_btn, COLOR_BTN_NAV_ACTIVE, 0);
    lv_obj_set_style_border_color(back_btn, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);

    lv_obj_t * back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT "  Back to Dashboard");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(back_lbl);

    lv_obj_add_event_cb(back_btn, back_to_main_cb, LV_EVENT_CLICKED, NULL);

    /* Center Page Content Card */
    lv_obj_t * card = lv_obj_create(sub_scr);
    lv_obj_set_size(card, 1240, 700);
    lv_obj_set_pos(card, 20, 80);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);

    lv_obj_t * c_lbl = lv_label_create(card);
    lv_label_set_text_fmt(c_lbl, "%s Module\n\n#7097ba Medical Ventilator View Screen#", name);
    lv_label_set_recolor(c_lbl, true);
    lv_obj_set_style_text_font(c_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(c_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(c_lbl);

    /* Recursive helper to remove scrollable flag from all children in the tree */
    extern void disable_scroll_recursive(lv_obj_t * obj);
    disable_scroll_recursive(sub_scr);

    /* Instant Screen Transition (No Fade Effect) */
    lv_screen_load_anim(sub_scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

void disable_scroll_recursive(lv_obj_t * obj)
{
    if(!obj) return;
    lv_obj_set_scrollable(obj, false);
    uint32_t i;
    uint32_t child_count = lv_obj_get_child_count(obj);
    for(i = 0; i < child_count; i++) {
        disable_scroll_recursive(lv_obj_get_child(obj, i));
    }
}

static void back_to_main_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    create_ventilator_main_screen();
}

/**
 * @brief Render Main Monitoring Dashboard Screen (1280x800) with Smooth Fading Transition
 */
void create_ventilator_main_screen(void)
{
    if(wave_timer) {
        lv_timer_delete(wave_timer);
        wave_timer = NULL;
    }

    /* Create New Screen for Fading Transition */
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ==================================================================== */
    /* 1. TOP HEADER BAR (1280 x 55)                                         */
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

    /* Battery, Date & Settings (Right) */
    lv_obj_t * right_hdr = lv_obj_create(top_bar);
    lv_obj_set_size(right_hdr, 280, 42);
    lv_obj_align(right_hdr, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(right_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_hdr, 0, 0);

    bat_lbl = lv_label_create(right_hdr);
    battery_update_label(bat_lbl);
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_12, 0);
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
    /* 2. LEFT PANEL - PATIENT VITAL READOUTS (300 x 675)                    */
    /* ==================================================================== */
    lv_obj_t * left_panel = lv_obj_create(scr);
    lv_obj_set_size(left_panel, 300, 675);
    lv_obj_set_pos(left_panel, 10, 60);
    lv_obj_set_style_bg_color(left_panel, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(left_panel, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(left_panel, 1, 0);
    lv_obj_set_style_radius(left_panel, 10, 0);
    lv_obj_set_style_pad_all(left_panel, 8, 0);

    /* Main Paw Card */
    lv_obj_t * paw_card = lv_obj_create(left_panel);
    lv_obj_set_size(paw_card, 280, 160);
    lv_obj_set_style_bg_color(paw_card, lv_color_hex(0x061A33), 0);
    lv_obj_set_style_border_color(paw_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(paw_card, 1, 0);
    lv_obj_set_style_radius(paw_card, 8, 0);
    lv_obj_align(paw_card, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * paw_t = lv_label_create(paw_card);
    lv_label_set_text(paw_t, "Paw\n#7097ba cmH2O#");
    lv_label_set_recolor(paw_t, true);
    lv_obj_set_style_text_font(paw_t, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(paw_t, COLOR_TEXT_MAIN, 0);
    lv_obj_align(paw_t, LV_ALIGN_TOP_LEFT, 6, 4);

    /* Pressure Bar Scale */
    bar_paw = lv_bar_create(paw_card);
    lv_obj_set_size(bar_paw, 10, 100);
    lv_obj_align(bar_paw, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    lv_bar_set_range(bar_paw, 0, 60);
    lv_bar_set_value(bar_paw, 24, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_paw, lv_color_hex(0x0C2A4A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_paw, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);

    /* Paw Giant Value Readout: 24 */
    lbl_val_paw = lv_label_create(paw_card);
    lv_label_set_text(lbl_val_paw, "24");
    lv_obj_set_style_text_font(lbl_val_paw, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_val_paw, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_val_paw, LV_ALIGN_CENTER, 35, -10);

    lv_obj_t * paw_unit = lv_label_create(paw_card);
    lv_label_set_text(paw_unit, "cmH2O");
    lv_obj_set_style_text_font(paw_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(paw_unit, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(paw_unit, LV_ALIGN_CENTER, 35, 25);

    /* Vital Readout Tiles Matrix */
    lv_obj_t * grid_cont = lv_obj_create(left_panel);
    lv_obj_set_size(grid_cont, 284, 485);
    lv_obj_align(grid_cont, LV_ALIGN_TOP_MID, 0, 168);
    lv_obj_set_style_bg_opa(grid_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_cont, 0, 0);
    lv_obj_set_style_pad_all(grid_cont, 0, 0);

    create_vital_tile(grid_cont, "Ppeak", "cmH2O", "32", 136, 75, &lbl_val_ppeak);
    create_vital_tile(grid_cont, "PEEP", "cmH2O", "5", 136, 75, &lbl_val_peep);
    create_vital_tile(grid_cont, "Minute Vol", "L/min", "6.8", 136, 75, &lbl_val_mv);
    create_vital_tile(grid_cont, "Resp Rate", "bpm", "16", 136, 75, &lbl_val_rr);
    create_vital_tile(grid_cont, "Tidal Vol", "mL", "420", 136, 75, &lbl_val_tv);
    create_vital_tile(grid_cont, "FiO2", "%", "40", 136, 75, &lbl_val_fio2);
    create_vital_tile(grid_cont, "Oxygen", "%", "40", 88, 75, &lbl_val_o2);
    create_vital_tile(grid_cont, "SpO2", "%", "98", 88, 75, &lbl_val_spo2);
    create_vital_tile(grid_cont, "Pulse", "bpm", "78", 88, 75, &lbl_val_pulse);

    /* Rearrange Grid positions */
    lv_obj_t * children[9];
    uint32_t cnt = lv_obj_get_child_count(grid_cont);
    for(uint32_t i = 0; i < cnt; i++) {
        children[i] = lv_obj_get_child(grid_cont, i);
    }
    if(cnt >= 9) {
        lv_obj_set_pos(children[0], 0, 0);
        lv_obj_set_pos(children[1], 144, 0);
        lv_obj_set_pos(children[2], 0, 82);
        lv_obj_set_pos(children[3], 144, 82);
        lv_obj_set_pos(children[4], 0, 164);
        lv_obj_set_pos(children[5], 144, 164);
        lv_obj_set_pos(children[6], 0, 246);
        lv_obj_set_pos(children[7], 96, 246);
        lv_obj_set_pos(children[8], 192, 246);
    }

    /* ==================================================================== */
    /* 3. CENTER WAVEFORMS SECTION (530 x 460)                              */
    /* ==================================================================== */
    lv_obj_t * wave_cont = lv_obj_create(scr);
    lv_obj_set_size(wave_cont, 530, 460);
    lv_obj_set_pos(wave_cont, 320, 60);
    lv_obj_set_style_bg_color(wave_cont, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(wave_cont, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(wave_cont, 1, 0);
    lv_obj_set_style_radius(wave_cont, 10, 0);
    lv_obj_set_style_pad_all(wave_cont, 6, 0);

    /* --- Waveform 1: Pressure (cmH2O) --- */
    lv_obj_t * box_p = lv_obj_create(wave_cont);
    lv_obj_set_size(box_p, 516, 140);
    lv_obj_align(box_p, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(box_p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_p, 0, 0);
    lv_obj_set_style_pad_all(box_p, 0, 0);

    /* Left Title + Unit */
    lv_obj_t * lbl_p_title = lv_label_create(box_p);
    lv_label_set_text(lbl_p_title, "Pressure\n#7097ba cmH2O#");
    lv_label_set_recolor(lbl_p_title, true);
    lv_obj_set_style_text_font(lbl_p_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_p_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(lbl_p_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* Y-Axis Numbers */
    lv_obj_t * lbl_p_scale = lv_label_create(box_p);
    lv_label_set_text(lbl_p_scale, "60\n\n30\n\n  0\n-10");
    lv_obj_set_style_text_font(lbl_p_scale, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_p_scale, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_p_scale, LV_ALIGN_LEFT_MID, 82, 0);

    /* Pressure Chart */
    chart_pressure = lv_chart_create(box_p);
    lv_obj_set_size(chart_pressure, 395, 130);
    lv_obj_align(chart_pressure, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_chart_set_type(chart_pressure, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_pressure, 90);
    lv_chart_set_range(chart_pressure, LV_CHART_AXIS_PRIMARY_Y, -10, 60);
    lv_chart_set_div_line_count(chart_pressure, 0, 0);
    lv_obj_set_style_bg_color(chart_pressure, lv_color_hex(0x040b16), 0);
    lv_obj_set_style_border_width(chart_pressure, 0, 0);
    lv_obj_set_style_line_opa(chart_pressure, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_pressure, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_pressure, 0, 0, LV_PART_INDICATOR);
    ser_pressure = lv_chart_add_series(chart_pressure, COLOR_ACCENT_BLUE, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_add_event_cb(chart_pressure, custom_chart_draw_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void *)(uintptr_t)WAVE_TYPE_PRESSURE);

    /* --- Waveform 2: Flow (L/min) --- */
    lv_obj_t * box_f = lv_obj_create(wave_cont);
    lv_obj_set_size(box_f, 516, 140);
    lv_obj_align(box_f, LV_ALIGN_TOP_MID, 0, 145);
    lv_obj_set_style_bg_opa(box_f, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_f, 0, 0);
    lv_obj_set_style_pad_all(box_f, 0, 0);

    /* Left Title + Unit */
    lv_obj_t * lbl_f_title = lv_label_create(box_f);
    lv_label_set_text(lbl_f_title, "Flow\n#7097ba L/min#");
    lv_label_set_recolor(lbl_f_title, true);
    lv_obj_set_style_text_font(lbl_f_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_f_title, COLOR_ACCENT_RED, 0);
    lv_obj_align(lbl_f_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* Y-Axis Numbers */
    lv_obj_t * lbl_f_scale = lv_label_create(box_f);
    lv_label_set_text(lbl_f_scale, "80\n\n  0\n\n-80");
    lv_obj_set_style_text_font(lbl_f_scale, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_f_scale, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_f_scale, LV_ALIGN_LEFT_MID, 82, 0);

    /* Flow Chart */
    chart_flow = lv_chart_create(box_f);
    lv_obj_set_size(chart_flow, 395, 130);
    lv_obj_align(chart_flow, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_chart_set_type(chart_flow, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_flow, 90);
    lv_chart_set_range(chart_flow, LV_CHART_AXIS_PRIMARY_Y, -80, 80);
    lv_chart_set_div_line_count(chart_flow, 0, 0);
    lv_obj_set_style_bg_color(chart_flow, lv_color_hex(0x040b16), 0);
    lv_obj_set_style_border_width(chart_flow, 0, 0);
    lv_obj_set_style_line_opa(chart_flow, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_flow, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_flow, 0, 0, LV_PART_INDICATOR);
    ser_flow = lv_chart_add_series(chart_flow, COLOR_ACCENT_RED, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_add_event_cb(chart_flow, custom_chart_draw_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void *)(uintptr_t)WAVE_TYPE_FLOW);

    /* --- Waveform 3: Volume (mL) --- */
    lv_obj_t * box_v = lv_obj_create(wave_cont);
    lv_obj_set_size(box_v, 516, 160);
    lv_obj_align(box_v, LV_ALIGN_TOP_MID, 0, 288);
    lv_obj_set_style_bg_opa(box_v, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_v, 0, 0);
    lv_obj_set_style_pad_all(box_v, 0, 0);

    /* Left Title + Unit */
    lv_obj_t * lbl_v_title = lv_label_create(box_v);
    lv_label_set_text(lbl_v_title, "Volume\n#7097ba mL#");
    lv_label_set_recolor(lbl_v_title, true);
    lv_obj_set_style_text_font(lbl_v_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_v_title, COLOR_ACCENT_YELLOW, 0);
    lv_obj_align(lbl_v_title, LV_ALIGN_LEFT_MID, 0, -10);

    /* Y-Axis Numbers */
    lv_obj_t * lbl_v_scale = lv_label_create(box_v);
    lv_label_set_text(lbl_v_scale, "800\n\n400\n\n   0");
    lv_obj_set_style_text_font(lbl_v_scale, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_v_scale, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_v_scale, LV_ALIGN_LEFT_MID, 80, -10);

    /* Volume Chart */
    chart_volume = lv_chart_create(box_v);
    lv_obj_set_size(chart_volume, 395, 130);
    lv_obj_align(chart_volume, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_chart_set_type(chart_volume, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_volume, 90);
    lv_chart_set_range(chart_volume, LV_CHART_AXIS_PRIMARY_Y, 0, 800);
    lv_chart_set_div_line_count(chart_volume, 0, 0);
    lv_obj_set_style_bg_color(chart_volume, lv_color_hex(0x040b16), 0);
    lv_obj_set_style_border_width(chart_volume, 0, 0);
    lv_obj_set_style_line_opa(chart_volume, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_volume, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_volume, 0, 0, LV_PART_INDICATOR);
    ser_volume = lv_chart_add_series(chart_volume, COLOR_ACCENT_YELLOW, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_add_event_cb(chart_volume, custom_chart_draw_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void *)(uintptr_t)WAVE_TYPE_VOLUME);

    /* X-Axis Time Scale Labels at bottom */
    lv_obj_t * lbl_x_scale = lv_label_create(box_v);
    lv_label_set_text(lbl_x_scale, "0                                   10                                   20        sec");
    lv_obj_set_style_text_font(lbl_x_scale, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_x_scale, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_x_scale, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* Start Waveform Data Timer */
    wave_timer = lv_timer_create(waveform_timer_cb, 50, NULL);

    /* ==================================================================== */
    /* 4. LUNGS VIEWPORT - Display Images/Lungs_image.png (270 x 460)        */
    /* ==================================================================== */
    lv_obj_t * lungs_vp = lv_obj_create(scr);
    lv_obj_set_size(lungs_vp, 270, 460);
    lv_obj_set_pos(lungs_vp, 860, 60);
    lv_obj_set_style_bg_color(lungs_vp, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(lungs_vp, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(lungs_vp, 1, 0);
    lv_obj_set_style_radius(lungs_vp, 10, 0);
    lv_obj_set_style_pad_all(lungs_vp, 4, 0);

    const char * lungs_path = "A:Images/Lungs_image.png";
    FILE * f_lungs = fopen("Images/Lungs_image.png", "rb");
    if(!f_lungs) {
        f_lungs = fopen("../Images/Lungs_image.png", "rb");
        if(f_lungs) {
            lungs_path = "A:../Images/Lungs_image.png";
        }
    }
    if(f_lungs) fclose(f_lungs);

    lv_obj_t * img_lungs = lv_image_create(lungs_vp);
    lv_image_set_src(img_lungs, lungs_path);
    lv_image_set_scale(img_lungs, 256);
    lv_obj_center(img_lungs);


    lv_anim_t a;
    lv_anim_init(&a);

    lv_anim_set_var(&a, img_lungs);
    lv_anim_set_exec_cb(&a, breathing_anim_cb);

    // Scale from 256 -> 282 (inhale)
    lv_anim_set_values(&a, 256, 282);

    // Duration of one inhale
    lv_anim_set_duration(&a, 1500);

// Exhale
lv_anim_set_playback_duration(&a, 1500);

// Repeat forever
lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);

// Smooth motion
lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

lv_anim_start(&a);

    /* ==================================================================== */
    /* 5. QUICK SETTINGS CONTROLS BAR (810 x 205)                           */
    /* ==================================================================== */
    lv_obj_t * quick_cont = lv_obj_create(scr);
    lv_obj_set_size(quick_cont, 810, 205);
    lv_obj_set_pos(quick_cont, 320, 530);
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
    lv_obj_set_size(q_boxes, 790, 150);
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
    /* 6. RIGHT SIDEBAR NAVIGATION MENU (130 x 675)                        */
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
        lv_obj_set_style_bg_color(btn, (i == 0) ? COLOR_BTN_NAV_ACTIVE : COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(btn, (i == 0) ? COLOR_ACCENT_BLUE : COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t * icon = lv_label_create(btn);
        lv_label_set_text(icon, nav_items[i][0]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, (i == 0) ? COLOR_ACCENT_BLUE : COLOR_TEXT_MAIN, 0);

        lv_obj_t * txt = lv_label_create(btn);
        lv_label_set_text(txt, nav_items[i][1]);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(txt, COLOR_TEXT_MAIN, 0);
        lv_obj_set_style_pad_top(txt, 4, 0);

        /* Add Click Event for Smooth Screen Fading Transition */
        lv_obj_add_event_cb(btn, nav_btn_cb, LV_EVENT_CLICKED, (void*)nav_items[i][1]);
    }

    /* ==================================================================== */
    /* 7. BOTTOM ACTION CONTROL BAR (1260 x 55)                             */
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

    /* Action Button 3: START / RESUME (Primary Green Button) */
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

    /* Disable scrolling on all elements in the main screen tree */
    disable_scroll_recursive(scr);

    /* Instant Screen Load (No Fade Effect) */
    lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
