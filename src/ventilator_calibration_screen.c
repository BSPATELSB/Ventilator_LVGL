#include "ventilator_calibration_screen.h"
#include "ventilator_main_screen.h"
#include "ventilator_settings_screen.h"
#include "ventilator_time_screen.h"
#include "battery_detect.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Color palette matching the medical ventilator premium dark UI theme */
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
#define COLOR_SIDEBAR_ACTIVE    lv_color_hex(0x1D124C) /* Premium dark purple highlight */

/* Static timer references */
static lv_timer_t * clock_timer = NULL;
static lv_timer_t * simulation_timer = NULL;
static lv_timer_t * cal_seq_timer = NULL;

/* Base layout objects */
static lv_obj_t * main_screen_obj = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * bat_lbl = NULL;

/* O2 Overview widgets */
static lv_obj_t * o2_gauge_arc = NULL;
static lv_obj_t * o2_gauge_val_lbl = NULL;

/* Calibration actions stack references */
static lv_obj_t * btn_start_cal = NULL;
static lv_obj_t * lbl_start_cal_title = NULL;

/* Calibration Progress checklist widgets */
static lv_obj_t * cal_prog_arc = NULL;
static lv_obj_t * cal_prog_val_lbl = NULL;
static lv_obj_t * cal_prog_status_lbl = NULL;
static lv_obj_t * cal_time_lbl = NULL;
static lv_obj_t * cal_time_bar = NULL;
static lv_obj_t * btn_cancel_cal = NULL;

/* Checklist step nodes */
static lv_obj_t * step_badge[4];
static lv_obj_t * step_lbl[4];
static lv_obj_t * step_badge_lbl[4];

/* Real-Time O2 Chart */
static lv_obj_t * o2_chart = NULL;
static lv_chart_series_t * o2_series = NULL;
static lv_obj_t * chart_digits_lbl = NULL;
static lv_obj_t * chart_time_lbl = NULL;

/* Sensor Health card refs to toggle dynamically */
static lv_obj_t * health_status_val_lbl = NULL;
static lv_obj_t * health_accuracy_val_lbl = NULL;
static lv_obj_t * health_shield_icon = NULL;

/* Interactive Calibration States */
typedef enum {
    CAL_STATE_IDLE = 0,
    CAL_STATE_PREPARE,
    CAL_STATE_ZERO,
    CAL_STATE_SPAN,
    CAL_STATE_VERIFY,
    CAL_STATE_COMPLETED
} cal_state_t;

static cal_state_t current_cal_state = CAL_STATE_IDLE;
static int calibration_percent = 0;
static float current_o2_reading = 40.5f;

/* Forward declarations */
extern void disable_scroll_recursive(lv_obj_t * obj);
static void back_to_settings_cb(lv_event_t * e);
static void back_to_home_cb(lv_event_t * e);
static void start_calibration_click_cb(lv_event_t * e);
static void cancel_calibration_click_cb(lv_event_t * e);

/* Dynamic clock updates */
static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if (lbl_clock) {
        time_t raw_time = ventilator_get_current_time(NULL);
        struct tm * time_info;
        time_info = localtime(&raw_time);
        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%d %b %Y\n%I:%M %p", time_info);
        lv_label_set_text(lbl_clock, clock_buf);
    }
    if (bat_lbl) {
        battery_update_label(bat_lbl);
    }
}

/* Helper to update step check items visual states */
static void update_checklist_step(int step_index, const char * text, int status_type) 
{
    /* status_type: 0 = Pending, 1 = In Progress, 2 = Completed */
    if (step_index < 0 || step_index >= 4) return;

    if (status_type == 0) {
        /* Pending: Grey badge, muted text */
        lv_obj_set_style_bg_color(step_badge[step_index], lv_color_hex(0x132C4A), 0);
        lv_obj_set_style_border_color(step_badge[step_index], COLOR_CARD_BORDER, 0);
        char num_str[2] = { (char)('1' + step_index), '\0' };
        lv_label_set_text(step_badge_lbl[step_index], num_str);
        lv_obj_set_style_text_color(step_badge_lbl[step_index], COLOR_TEXT_MUTED, 0);
        
        lv_label_set_text_fmt(step_lbl[step_index], "%s\n#7097ba Pending#", text);
        lv_label_set_recolor(step_lbl[step_index], true);
    } 
    else if (status_type == 1) {
        /* In Progress: Blue badge, highlighted text */
        lv_obj_set_style_bg_color(step_badge[step_index], COLOR_BTN_NAV_ACTIVE, 0);
        lv_obj_set_style_border_color(step_badge[step_index], COLOR_ACCENT_BLUE, 0);
        char num_str[2] = { (char)('1' + step_index), '\0' };
        lv_label_set_text(step_badge_lbl[step_index], num_str);
        lv_obj_set_style_text_color(step_badge_lbl[step_index], COLOR_TEXT_MAIN, 0);

        lv_label_set_text_fmt(step_lbl[step_index], "%s\n#00A8FF In Progress...#", text);
        lv_label_set_recolor(step_lbl[step_index], true);
    } 
    else if (status_type == 2) {
        /* Completed: Green checkmark badge, green text */
        lv_obj_set_style_bg_color(step_badge[step_index], lv_color_hex(0x06281C), 0);
        lv_obj_set_style_border_color(step_badge[step_index], COLOR_ACCENT_GREEN, 0);
        lv_label_set_text(step_badge_lbl[step_index], LV_SYMBOL_OK);
        lv_obj_set_style_text_color(step_badge_lbl[step_index], COLOR_ACCENT_GREEN, 0);

        lv_label_set_text_fmt(step_lbl[step_index], "%s\n#00E676 Completed#", text);
        lv_label_set_recolor(step_lbl[step_index], true);
    }
}

/* Simulation loop timer callback (updates the graph and reading digits) */
static void simulation_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    /* Fluctuate current oxygen level slightly based on active calibration state */
    float target_o2 = 40.5f;

    if (current_cal_state == CAL_STATE_IDLE) {
        target_o2 = 40.5f;
    } 
    else if (current_cal_state == CAL_STATE_PREPARE) {
        target_o2 = 40.5f;
    } 
    else if (current_cal_state == CAL_STATE_ZERO) {
        /* Drop towards 21% */
        target_o2 = 21.0f;
    } 
    else if (current_cal_state == CAL_STATE_SPAN) {
        /* Rise towards 100% */
        target_o2 = 100.0f;
    } 
    else if (current_cal_state == CAL_STATE_VERIFY) {
        target_o2 = 100.0f;
    } 
    else if (current_cal_state == CAL_STATE_COMPLETED) {
        target_o2 = 40.5f;
    }

    /* Move current reading towards target reading to simulate physical system lag */
    float diff = target_o2 - current_o2_reading;
    current_o2_reading += diff * 0.15f;

    /* Add micro-noise fluctuations */
    float noise = ((float)(rand() % 10) - 5.0f) / 30.0f;
    float display_o2 = current_o2_reading + noise;
    if (display_o2 < 0.0f) display_o2 = 0.0f;
    if (display_o2 > 100.0f) display_o2 = 100.0f;

    /* Update gauge text value */
    if (o2_gauge_val_lbl) {
        lv_label_set_text_fmt(o2_gauge_val_lbl, "%.1f", display_o2);
    }
    if (o2_gauge_arc) {
        lv_arc_set_value(o2_gauge_arc, (int)display_o2);
    }

    /* Update real-time chart points */
    if (o2_chart && o2_series) {
        lv_chart_set_next_value(o2_chart, o2_series, (int)display_o2);
    }

    /* Update chart overlay value label */
    if (chart_digits_lbl) {
        lv_label_set_text_fmt(chart_digits_lbl, "%.1f %%", display_o2);
    }

    /* Update chart timestamp label to current time */
    if (chart_time_lbl) {
        time_t raw_time;
        struct tm * tm_info;
        time(&raw_time);
        tm_info = localtime(&raw_time);
        char t_buf[16];
        strftime(t_buf, sizeof(t_buf), "%I:%M:%S %p", tm_info);
        lv_label_set_text(chart_time_lbl, t_buf);
    }
}

/* Active calibration sequence callback */
static void cal_seq_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    
    calibration_percent += 1;
    if (calibration_percent > 100) {
        calibration_percent = 100;
    }

    /* Update calibration progress circle & text */
    if (cal_prog_arc) {
        lv_arc_set_value(cal_prog_arc, calibration_percent);
    }
    if (cal_prog_val_lbl) {
        lv_label_set_text_fmt(cal_prog_val_lbl, "%d%%", calibration_percent);
    }

    /* Calculate remaining time */
    int secs_left = (100 - calibration_percent) * 45 / 100; // 45 seconds total
    if (cal_time_lbl) {
        lv_label_set_text_fmt(cal_time_lbl, "00:%02d min", secs_left);
    }
    if (cal_time_bar) {
        lv_bar_set_value(cal_time_bar, calibration_percent, LV_ANIM_OFF);
    }

    /* Update state machine boundaries and steps */
    if (calibration_percent < 25) {
        current_cal_state = CAL_STATE_PREPARE;
        if (cal_prog_status_lbl) lv_label_set_text(cal_prog_status_lbl, "Preparing Sensor...");
        
        update_checklist_step(0, "Prepare Sensor", 1); /* In progress */
        update_checklist_step(1, "Calibrate at 21% (Room Air)", 0);
        update_checklist_step(2, "Calibrate at 100% (Oxygen)", 0);
        update_checklist_step(3, "Verify & Save", 0);
    } 
    else if (calibration_percent < 50) {
        current_cal_state = CAL_STATE_ZERO;
        if (cal_prog_status_lbl) lv_label_set_text(cal_prog_status_lbl, "Calibrating to Room Air...");

        update_checklist_step(0, "Prepare Sensor", 2); /* Completed */
        update_checklist_step(1, "Calibrate at 21% (Room Air)", 1); /* In Progress */
        update_checklist_step(2, "Calibrate at 100% (Oxygen)", 0);
        update_checklist_step(3, "Verify & Save", 0);
    } 
    else if (calibration_percent < 75) {
        current_cal_state = CAL_STATE_SPAN;
        if (cal_prog_status_lbl) lv_label_set_text(cal_prog_status_lbl, "Calibrating to Pure Oxygen...");

        update_checklist_step(0, "Prepare Sensor", 2);
        update_checklist_step(1, "Calibrate at 21% (Room Air)", 2); /* Completed */
        update_checklist_step(2, "Calibrate at 100% (Oxygen)", 1); /* In Progress */
        update_checklist_step(3, "Verify & Save", 0);
    } 
    else if (calibration_percent < 100) {
        current_cal_state = CAL_STATE_VERIFY;
        if (cal_prog_status_lbl) lv_label_set_text(cal_prog_status_lbl, "Verifying and Saving calibration factors...");

        update_checklist_step(0, "Prepare Sensor", 2);
        update_checklist_step(1, "Calibrate at 21% (Room Air)", 2);
        update_checklist_step(2, "Calibrate at 100% (Oxygen)", 2); /* Completed */
        update_checklist_step(3, "Verify & Save", 1); /* In Progress */
    } 
    else if (calibration_percent == 100) {
        /* Calibration finished successfully! */
        current_cal_state = CAL_STATE_COMPLETED;
        if (cal_prog_status_lbl) lv_label_set_text(cal_prog_status_lbl, "Calibration Successful!");
        
        update_checklist_step(0, "Prepare Sensor", 2);
        update_checklist_step(1, "Calibrate at 21% (Room Air)", 2);
        update_checklist_step(2, "Calibrate at 100% (Oxygen)", 2);
        update_checklist_step(3, "Verify & Save", 2); /* Completed */

        /* Restore action buttons */
        if (btn_start_cal && lbl_start_cal_title) {
            lv_obj_clear_state(btn_start_cal, LV_STATE_DISABLED);
            lv_label_set_text(lbl_start_cal_title, "START CALIBRATION");
            lv_obj_set_style_bg_color(btn_start_cal, COLOR_SIDEBAR_ACTIVE, 0);
        }
        if (btn_cancel_cal) {
            lv_obj_add_state(btn_cancel_cal, LV_STATE_DISABLED);
        }

        /* Update health parameters */
        if (health_status_val_lbl) {
            lv_label_set_text(health_status_val_lbl, "OK");
            lv_obj_set_style_text_color(health_status_val_lbl, COLOR_ACCENT_GREEN, 0);
        }
        if (health_accuracy_val_lbl) {
            lv_label_set_text(health_accuracy_val_lbl, "± 1.0 %");
            lv_obj_set_style_text_color(health_accuracy_val_lbl, COLOR_ACCENT_GREEN, 0);
        }

        /* Delete sequence timer */
        if (cal_seq_timer) {
            lv_timer_delete(cal_seq_timer);
            cal_seq_timer = NULL;
        }

        /* Wait brief moment and reset state back to idle monitoring */
        current_cal_state = CAL_STATE_IDLE;
    }
}

/* Callback for START CALIBRATION click */
static void start_calibration_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    
    calibration_percent = 0;
    current_cal_state = CAL_STATE_PREPARE;

    /* Update button visual indicators */
    lv_obj_add_state(btn_start_cal, LV_STATE_DISABLED);
    lv_label_set_text(lbl_start_cal_title, "CALIBRATING...");
    lv_obj_set_style_bg_color(btn_start_cal, COLOR_BTN_NAV_ACTIVE, 0);

    /* Enable Cancel button */
    if (btn_cancel_cal) {
        lv_obj_clear_state(btn_cancel_cal, LV_STATE_DISABLED);
    }

    /* Start sequence timer (45 seconds total, tick every 450ms) */
    if (cal_seq_timer) {
        lv_timer_delete(cal_seq_timer);
    }
    cal_seq_timer = lv_timer_create(cal_seq_timer_cb, 450, NULL);
}

/* Callback for CANCEL CALIBRATION click */
static void cancel_calibration_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);

    /* Terminate calibration sequence */
    if (cal_seq_timer) {
        lv_timer_delete(cal_seq_timer);
        cal_seq_timer = NULL;
    }

    current_cal_state = CAL_STATE_IDLE;
    calibration_percent = 0;

    /* Restore button states */
    if (btn_start_cal && lbl_start_cal_title) {
        lv_obj_clear_state(btn_start_cal, LV_STATE_DISABLED);
        lv_label_set_text(lbl_start_cal_title, "START CALIBRATION");
        lv_obj_set_style_bg_color(btn_start_cal, COLOR_SIDEBAR_ACTIVE, 0);
    }
    if (btn_cancel_cal) {
        lv_obj_add_state(btn_cancel_cal, LV_STATE_DISABLED);
    }

    /* Reset progress text */
    if (cal_prog_arc) lv_arc_set_value(cal_prog_arc, 0);
    if (cal_prog_val_lbl) lv_label_set_text(cal_prog_val_lbl, "0%");
    if (cal_prog_status_lbl) lv_label_set_text(cal_prog_status_lbl, "Calibration Cancelled");
    if (cal_time_lbl) lv_label_set_text(cal_time_lbl, "00:00 min");
    if (cal_time_bar) lv_bar_set_value(cal_time_bar, 0, LV_ANIM_OFF);

    /* Reset checklist indicators back to pending */
    update_checklist_step(0, "Prepare Sensor", 0);
    update_checklist_step(1, "Calibrate at 21% (Room Air)", 0);
    update_checklist_step(2, "Calibrate at 100% (Oxygen)", 0);
    update_checklist_step(3, "Verify & Save", 0);
}

/* Back navigation callback routines */
static void back_to_settings_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if (simulation_timer) {
        lv_timer_delete(simulation_timer);
        simulation_timer = NULL;
    }
    if (cal_seq_timer) {
        lv_timer_delete(cal_seq_timer);
        cal_seq_timer = NULL;
    }
    create_ventilator_settings_screen();
}

static void back_to_home_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if (simulation_timer) {
        lv_timer_delete(simulation_timer);
        simulation_timer = NULL;
    }
    if (cal_seq_timer) {
        lv_timer_delete(cal_seq_timer);
        cal_seq_timer = NULL;
    }
    create_ventilator_main_screen();
}

static void pat_badge_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if (simulation_timer) {
        lv_timer_delete(simulation_timer);
        simulation_timer = NULL;
    }
    if (cal_seq_timer) {
        lv_timer_delete(cal_seq_timer);
        cal_seq_timer = NULL;
    }
    extern void create_ventilator_patient_screen(void);
    create_ventilator_patient_screen();
}

/* Helper to render Sidebar buttons */
static lv_obj_t * create_sidebar_btn(lv_obj_t * parent, int y_pos, const char * symbol, const char * text, bool is_active)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_size(btn, 200, 42);
    lv_obj_set_pos(btn, 10, y_pos);

    if (is_active) {
        lv_obj_set_style_bg_color(btn, COLOR_SIDEBAR_ACTIVE, 0);
        lv_obj_set_style_border_color(btn, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
    } else {
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
    }
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t * lbl = lv_label_create(btn);
    if (symbol) {
        lv_label_set_text_fmt(lbl, "%s  %s", symbol, text);
    } else {
        lv_label_set_text(lbl, text);
    }
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, is_active ? COLOR_TEXT_MAIN : COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 16, 0);

    return btn;
}

/* Helper to construct vertical action button entries */
static lv_obj_t * create_cal_action_btn(lv_obj_t * parent, int index, const char * title, const char * subtitle, const char * symbol, bool is_primary, lv_obj_t ** out_title_lbl)
{
    int y = 15 + index * 102;
    
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_size(btn, 220, 88);
    lv_obj_set_pos(btn, 10, y);

    if (is_primary) {
        lv_obj_set_style_bg_color(btn, COLOR_SIDEBAR_ACTIVE, 0);
        lv_obj_set_style_border_color(btn, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
    } else {
        lv_obj_set_style_bg_color(btn, COLOR_CARD_BG, 0);
        lv_obj_set_style_border_color(btn, COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
    }
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, COLOR_BTN_NAV_ACTIVE, LV_STATE_PRESSED);

    /* Left Symbol box badge */
    lv_obj_t * badge = lv_obj_create(btn);
    lv_obj_set_size(badge, 44, 44);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x061D3B), 0);
    lv_obj_set_style_border_color(badge, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);

    lv_obj_t * badge_symbol = lv_label_create(badge);
    lv_label_set_text(badge_symbol, symbol);
    lv_obj_set_style_text_font(badge_symbol, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(badge_symbol, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(badge_symbol);

    /* Text details layout */
    lv_obj_t * label_title = lv_label_create(btn);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(label_title, LV_ALIGN_LEFT_MID, 68, -10);

    if (out_title_lbl) {
        *out_title_lbl = label_title;
    }

    lv_obj_t * label_sub = lv_label_create(btn);
    lv_label_set_text(label_sub, subtitle);
    lv_obj_set_style_text_font(label_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_sub, COLOR_TEXT_MUTED, 0);
    lv_obj_align(label_sub, LV_ALIGN_LEFT_MID, 68, 12);

    return btn;
}

/* Helper to render Sensor Health specifications list */
static void create_health_row(lv_obj_t * parent, int y_pos, const char * title, const char * val, bool is_ok, lv_obj_t ** out_val_lbl)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_size(row, 260, 26);
    lv_obj_set_pos(row, 10, y_pos);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    /* Title Label with custom symbol depending on detail type */
    const char * prefix_symbol = " ";
    if (strcmp(title, "Sensor Status") == 0) prefix_symbol = LV_SYMBOL_CHARGE;
    else if (strcmp(title, "Response Time") == 0) prefix_symbol = LV_SYMBOL_LIST;
    else if (strcmp(title, "Accuracy") == 0) prefix_symbol = LV_SYMBOL_SETTINGS;
    else if (strcmp(title, "Stability") == 0) prefix_symbol = LV_SYMBOL_IMAGE;
    else if (strcmp(title, "Last Calibration") == 0) prefix_symbol = LV_SYMBOL_LIST;
    else if (strcmp(title, "Sensor Model") == 0) prefix_symbol = LV_SYMBOL_DIRECTORY;
    else if (strcmp(title, "Serial Number") == 0) prefix_symbol = LV_SYMBOL_KEYBOARD;

    lv_obj_t * lbl_title = lv_label_create(row);
    lv_label_set_text_fmt(lbl_title, "%s  %s", prefix_symbol, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 10, 0);

    /* Value Details */
    lv_obj_t * lbl_val = lv_label_create(row);
    lv_label_set_text(lbl_val, val);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_12, 0);
    if (is_ok) {
        lv_obj_set_style_text_color(lbl_val, COLOR_ACCENT_GREEN, 0);
    } else {
        lv_obj_set_style_text_color(lbl_val, COLOR_TEXT_MAIN, 0);
    }
    lv_obj_align(lbl_val, LV_ALIGN_RIGHT_MID, -30, 0);

    if (out_val_lbl) {
        *out_val_lbl = lbl_val;
    }

    /* Icon suffix for green checkmark indicators */
    if (is_ok) {
        lv_obj_t * check_badge = lv_label_create(row);
        lv_label_set_text(check_badge, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(check_badge, COLOR_ACCENT_GREEN, 0);
        lv_obj_align(check_badge, LV_ALIGN_RIGHT_MID, -10, 0);
    }
}

/* Helper to setup step checklist item elements */
static void create_checklist_step_node(lv_obj_t * parent, int index, const char * text)
{
    int y = 15 + index * 42;

    lv_obj_t * container = lv_obj_create(parent);
    lv_obj_set_size(container, 260, 36);
    lv_obj_set_pos(container, 170, y);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    /* Circular Badge */
    lv_obj_t * badge = lv_obj_create(container);
    lv_obj_set_size(badge, 24, 24);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x132C4A), 0);
    lv_obj_set_style_border_color(badge, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    step_badge[index] = badge;

    lv_obj_t * badge_lbl = lv_label_create(badge);
    char num_str[2] = { (char)('1' + index), '\0' };
    lv_label_set_text(badge_lbl, num_str);
    lv_obj_set_style_text_font(badge_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(badge_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_center(badge_lbl);
    step_badge_lbl[index] = badge_lbl;

    /* Description Status labels */
    lv_obj_t * desc_lbl = lv_label_create(container);
    lv_label_set_text_fmt(desc_lbl, "%s\n#7097ba Pending#", text);
    lv_label_set_recolor(desc_lbl, true);
    lv_obj_set_style_text_font(desc_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(desc_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(desc_lbl, LV_ALIGN_LEFT_MID, 34, 0);
    step_lbl[index] = desc_lbl;
}

/**
 * @brief Create and render the Oxygen Calibration screen (1280x800).
 */
void create_ventilator_calibration_screen(void)
{
    /* Clean timers to avoid leakage */
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if (simulation_timer) {
        lv_timer_delete(simulation_timer);
        simulation_timer = NULL;
    }
    if (cal_seq_timer) {
        lv_timer_delete(cal_seq_timer);
        cal_seq_timer = NULL;
    }

    current_cal_state = CAL_STATE_IDLE;
    calibration_percent = 0;
    current_o2_reading = 40.5f;

    /* Base screen object container */
    main_screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen_obj, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(main_screen_obj, LV_OPA_COVER, 0);

    /* ==================================================================== */
    /* 1. TOP HEADER BAR                                                    */
    /* ==================================================================== */
    lv_obj_t * top_bar = lv_obj_create(main_screen_obj);
    lv_obj_set_size(top_bar, 1280, 55);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(top_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_hor(top_bar, 12, 0);

    /* O2 title badge box */
    lv_obj_t * title_box = lv_obj_create(top_bar);
    lv_obj_set_size(title_box, 250, 42);
    lv_obj_align(title_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(title_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(title_box, 0, 0);
    lv_obj_set_style_radius(title_box, 6, 0);
    lv_obj_set_style_pad_all(title_box, 4, 0);

    lv_obj_t * title_badge = lv_obj_create(title_box);
    lv_obj_set_size(title_badge, 34, 34);
    lv_obj_align(title_badge, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(title_badge, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(title_badge, 6, 0);
    lv_obj_set_style_border_width(title_badge, 0, 0);

    lv_obj_t * title_icon = lv_label_create(title_badge);
    lv_label_set_text(title_icon, "O₂");
    lv_obj_set_style_text_font(title_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_icon, lv_color_white(), 0);
    lv_obj_center(title_icon);

    lv_obj_t * title_lbl = lv_label_create(title_box);
    lv_label_set_text(title_lbl, "OXYGEN CALIBRATION");
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * subtitle_lbl = lv_label_create(title_box);
    lv_label_set_text(subtitle_lbl, "Calibrate oxygen sensor accuracy");
    lv_obj_set_style_text_font(subtitle_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(subtitle_lbl, LV_ALIGN_LEFT_MID, 46, 10);

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

    lv_obj_t * date_icon = lv_label_create(date_box);
    lv_label_set_text(date_icon, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(date_icon, COLOR_TEXT_MUTED, 0);
    lv_obj_align(date_icon, LV_ALIGN_LEFT_MID, 10, 0);

    lbl_clock = lv_label_create(date_box);
    lv_label_set_text(lbl_clock, "20 May 2024\n10:24 AM");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 30, 0);

    /* Battery & Settings Box (Right) */
    lv_obj_t * right_hdr = lv_obj_create(top_bar);
    lv_obj_set_size(right_hdr, 120, 42);
    lv_obj_align(right_hdr, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(right_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_hdr, 0, 0);

    bat_lbl = lv_label_create(right_hdr);
    battery_update_label(bat_lbl);
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(bat_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * set_icon = lv_button_create(right_hdr);
    lv_obj_set_size(set_icon, 32, 32);
    lv_obj_align(set_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(set_icon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(set_icon, 0, 0);
    lv_obj_set_style_pad_all(set_icon, 0, 0);

    lv_obj_t * set_icon_lbl = lv_label_create(set_icon);
    lv_label_set_text(set_icon_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(set_icon_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(set_icon_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(set_icon_lbl);
    lv_obj_add_event_cb(set_icon, back_to_settings_cb, LV_EVENT_CLICKED, NULL);

    /* ==================================================================== */
    /* 2. LEFT SIDEBAR NAVIGATION                                           */
    /* ==================================================================== */
    lv_obj_t * sidebar = lv_obj_create(main_screen_obj);
    lv_obj_set_size(sidebar, 220, 715);
    lv_obj_set_pos(sidebar, 15, 65);
    lv_obj_set_style_bg_color(sidebar, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(sidebar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sidebar, 1, 0);
    lv_obj_set_style_radius(sidebar, 12, 0);
    lv_obj_set_style_pad_all(sidebar, 0, 0);

    /* Sidebar items vertically alignment */
    create_sidebar_btn(sidebar, 15, LV_SYMBOL_HOME, "CALIBRATION HOME", false);
    create_sidebar_btn(sidebar, 65, LV_SYMBOL_SETTINGS, "PRESSURE SENSOR", false);
    create_sidebar_btn(sidebar, 115, LV_SYMBOL_IMAGE, "FLOW SENSOR", false);
    create_sidebar_btn(sidebar, 165, "O₂", "OXYGEN SENSOR", true); /* Active */
    create_sidebar_btn(sidebar, 215, LV_SYMBOL_DOWNLOAD, "LEAK TEST", false);
    create_sidebar_btn(sidebar, 265, LV_SYMBOL_SETTINGS, "CIRCUIT TEST", false);
    create_sidebar_btn(sidebar, 315, LV_SYMBOL_LIST, "CALIBRATION HISTORY", false);

    /* Back to Home action at footer of sidebar */
    lv_obj_t * sidebar_home_btn = lv_button_create(sidebar);
    lv_obj_set_size(sidebar_home_btn, 200, 48);
    lv_obj_set_pos(sidebar_home_btn, 10, 650);
    lv_obj_set_style_bg_opa(sidebar_home_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(sidebar_home_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sidebar_home_btn, 1, 0);
    lv_obj_set_style_radius(sidebar_home_btn, 8, 0);
    lv_obj_set_style_bg_color(sidebar_home_btn, COLOR_BTN_NAV_ACTIVE, LV_STATE_PRESSED);
    lv_obj_add_event_cb(sidebar_home_btn, back_to_home_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * sidebar_home_lbl = lv_label_create(sidebar_home_btn);
    lv_label_set_text(sidebar_home_lbl, "<  BACK TO HOME");
    lv_obj_set_style_text_font(sidebar_home_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sidebar_home_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(sidebar_home_lbl);

    /* ==================================================================== */
    /* 3. ROW 1 CONTENTS                                                    */
    /* ==================================================================== */

    /* Panel A: OXYGEN SENSOR OVERVIEW */
    lv_obj_t * card_overview = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_overview, 460, 330);
    lv_obj_set_pos(card_overview, 250, 65);
    lv_obj_set_style_bg_color(card_overview, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_overview, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_overview, 1, 0);
    lv_obj_set_style_radius(card_overview, 12, 0);
    lv_obj_set_style_pad_all(card_overview, 12, 0);

    lv_obj_t * o2_title = lv_label_create(card_overview);
    lv_label_set_text(o2_title, "OXYGEN SENSOR OVERVIEW");
    lv_obj_set_style_text_font(o2_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(o2_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(o2_title, 10, 5);

    /* Arc Dial Widget */
    o2_gauge_arc = lv_arc_create(card_overview);
    lv_obj_set_size(o2_gauge_arc, 190, 190);
    lv_obj_align(o2_gauge_arc, LV_ALIGN_TOP_MID, 0, 30);
    lv_arc_set_bg_angles(o2_gauge_arc, 135, 45); /* 270 degree dial */
    lv_arc_set_range(o2_gauge_arc, 0, 100);
    lv_arc_set_value(o2_gauge_arc, 40);

    /* Style the dial arc */
    lv_obj_set_style_arc_width(o2_gauge_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(o2_gauge_arc, COLOR_CARD_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(o2_gauge_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(o2_gauge_arc, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_opa(o2_gauge_arc, LV_OPA_TRANSP, LV_PART_KNOB); /* hide knob */
    lv_obj_set_style_bg_opa(o2_gauge_arc, LV_OPA_TRANSP, LV_PART_KNOB);

    /* Text values stacked inside the dial */
    lv_obj_t * dial_lbl_o2 = lv_label_create(o2_gauge_arc);
    lv_label_set_text(dial_lbl_o2, "O₂");
    lv_obj_set_style_text_font(dial_lbl_o2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dial_lbl_o2, COLOR_TEXT_MUTED, 0);
    lv_obj_align(dial_lbl_o2, LV_ALIGN_CENTER, 0, -45);

    o2_gauge_val_lbl = lv_label_create(o2_gauge_arc);
    lv_label_set_text(o2_gauge_val_lbl, "40.5");
    lv_obj_set_style_text_font(o2_gauge_val_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(o2_gauge_val_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(o2_gauge_val_lbl, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t * dial_lbl_pct = lv_label_create(o2_gauge_arc);
    lv_label_set_text(dial_lbl_pct, "%");
    lv_obj_set_style_text_font(dial_lbl_pct, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(dial_lbl_pct, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(dial_lbl_pct, LV_ALIGN_CENTER, 0, 32);

    lv_obj_t * dial_lbl_desc = lv_label_create(o2_gauge_arc);
    lv_label_set_text(dial_lbl_desc, "Current Oxygen");
    lv_obj_set_style_text_font(dial_lbl_desc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(dial_lbl_desc, COLOR_TEXT_MUTED, 0);
    lv_obj_align(dial_lbl_desc, LV_ALIGN_CENTER, 0, 56);

    /* Left and right gauge limit values */
    lv_obj_t * limit_left = lv_label_create(card_overview);
    lv_label_set_text(limit_left, "0");
    lv_obj_set_style_text_font(limit_left, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(limit_left, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(limit_left, 130, 215);

    lv_obj_t * limit_right = lv_label_create(card_overview);
    lv_label_set_text(limit_right, "100");
    lv_obj_set_style_text_font(limit_right, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(limit_right, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(limit_right, 310, 215);

    /* Target percentage sublabel */
    lv_obj_t * target_lbl = lv_label_create(card_overview);
    lv_label_set_text(target_lbl, "Target: #ffffff 40%#  " LV_SYMBOL_DIRECTORY);
    lv_label_set_recolor(target_lbl, true);
    lv_obj_set_style_text_font(target_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(target_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(target_lbl, LV_ALIGN_BOTTOM_MID, 0, -15);

    /* Panel B: CALIBRATION ACTIONS */
    lv_obj_t * card_actions = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_actions, 240, 330);
    lv_obj_set_pos(card_actions, 720, 65);
    lv_obj_set_style_bg_opa(card_actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card_actions, 0, 0);
    lv_obj_set_style_pad_all(card_actions, 0, 0);

    /* 3 Action Stack Buttons */
    btn_start_cal = create_cal_action_btn(card_actions, 0, "START CALIBRATION", "Calibrate Oxygen Sensor", LV_SYMBOL_GPS, true, &lbl_start_cal_title);
    lv_obj_add_event_cb(btn_start_cal, start_calibration_click_cb, LV_EVENT_CLICKED, NULL);

    create_cal_action_btn(card_actions, 1, "ZERO CALIBRATION", "Calibrate to Room Air (21%)", LV_SYMBOL_REFRESH, false, NULL);
    create_cal_action_btn(card_actions, 2, "SPAN CALIBRATION", "Calibrate to 100% Oxygen", "O₂", false, NULL);

    /* Panel C: SENSOR HEALTH */
    lv_obj_t * card_health = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_health, 290, 330);
    lv_obj_set_pos(card_health, 975, 65);
    lv_obj_set_style_bg_color(card_health, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_health, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_health, 1, 0);
    lv_obj_set_style_radius(card_health, 12, 0);
    lv_obj_set_style_pad_all(card_health, 10, 0);

    lv_obj_t * health_title = lv_label_create(card_health);
    lv_label_set_text(health_title, "SENSOR HEALTH");
    lv_obj_set_style_text_font(health_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(health_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(health_title, 10, 5);

    /* Healthy Green Status Badge container */
    lv_obj_t * status_badge = lv_obj_create(card_health);
    lv_obj_set_size(status_badge, 260, 50);
    lv_obj_set_pos(status_badge, 10, 25);
    lv_obj_set_style_bg_color(status_badge, lv_color_hex(0x06281C), 0);
    lv_obj_set_style_border_color(status_badge, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_border_width(status_badge, 1, 0);
    lv_obj_set_style_radius(status_badge, 6, 0);
    lv_obj_set_style_pad_all(status_badge, 4, 0);

    health_shield_icon = lv_label_create(status_badge);
    lv_label_set_text(health_shield_icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(health_shield_icon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(health_shield_icon, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(health_shield_icon, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t * badge_status_lbl = lv_label_create(status_badge);
    lv_label_set_text(badge_status_lbl, "HEALTHY");
    lv_obj_set_style_text_font(badge_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(badge_status_lbl, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(badge_status_lbl, LV_ALIGN_LEFT_MID, 40, -8);

    lv_obj_t * badge_status_sub = lv_label_create(status_badge);
    lv_label_set_text(badge_status_sub, "Sensor is operating within normal range");
    lv_obj_set_style_text_font(badge_status_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(badge_status_sub, COLOR_TEXT_MUTED, 0);
    lv_obj_align(badge_status_sub, LV_ALIGN_LEFT_MID, 40, 10);

    /* Key-Value Sensor parameters checklist details */
    create_health_row(card_health, 85, "Sensor Status", "OK", true, &health_status_val_lbl);
    create_health_row(card_health, 118, "Response Time", "< 2 sec", true, NULL);
    create_health_row(card_health, 151, "Accuracy", "± 1.5 %", true, &health_accuracy_val_lbl);
    create_health_row(card_health, 184, "Stability", "Excellent", true, NULL);
    create_health_row(card_health, 217, "Last Calibration", "19 May 2024 02:15 PM", false, NULL);
    create_health_row(card_health, 250, "Sensor Model", "OX-2000", false, NULL);
    create_health_row(card_health, 283, "Serial Number", "OX2000-12345", false, NULL);

    /* ==================================================================== */
    /* 4. ROW 2 CONTENTS                                                    */
    /* ==================================================================== */

    /* Panel D: CALIBRATION PROGRESS */
    lv_obj_t * card_cal_prog = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_cal_prog, 460, 250);
    lv_obj_set_pos(card_cal_prog, 250, 410);
    lv_obj_set_style_bg_color(card_cal_prog, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_cal_prog, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_cal_prog, 1, 0);
    lv_obj_set_style_radius(card_cal_prog, 12, 0);
    lv_obj_set_style_pad_all(card_cal_prog, 10, 0);

    lv_obj_t * cal_prog_title = lv_label_create(card_cal_prog);
    lv_label_set_text(cal_prog_title, "CALIBRATION PROGRESS");
    lv_obj_set_style_text_font(cal_prog_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cal_prog_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(cal_prog_title, 10, 5);

    /* Progress Ring Arc */
    cal_prog_arc = lv_arc_create(card_cal_prog);
    lv_obj_set_size(cal_prog_arc, 110, 110);
    lv_obj_set_pos(cal_prog_arc, 20, 35);
    lv_arc_set_bg_angles(cal_prog_arc, 0, 360); /* Full circle */
    lv_arc_set_range(cal_prog_arc, 0, 100);
    lv_arc_set_value(cal_prog_arc, 0);

    /* Style progress ring */
    lv_obj_set_style_arc_width(cal_prog_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(cal_prog_arc, COLOR_CARD_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(cal_prog_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(cal_prog_arc, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_opa(cal_prog_arc, LV_OPA_TRANSP, LV_PART_KNOB); /* hide knob */
    lv_obj_set_style_bg_opa(cal_prog_arc, LV_OPA_TRANSP, LV_PART_KNOB);

    /* Display Progress Text */
    cal_prog_val_lbl = lv_label_create(cal_prog_arc);
    lv_label_set_text(cal_prog_val_lbl, "0%");
    lv_obj_set_style_text_font(cal_prog_val_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(cal_prog_val_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(cal_prog_val_lbl);

    cal_prog_status_lbl = lv_label_create(card_cal_prog);
    lv_label_set_text(cal_prog_status_lbl, "Ready to Calibrate");
    lv_obj_set_style_text_font(cal_prog_status_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cal_prog_status_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_align(cal_prog_status_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(cal_prog_status_lbl, 10, 155);

    /* Step checklist layout items */
    create_checklist_step_node(card_cal_prog, 0, "Prepare Sensor");
    create_checklist_step_node(card_cal_prog, 1, "Calibrate at 21% (Room Air)");
    create_checklist_step_node(card_cal_prog, 2, "Calibrate at 100% (Oxygen)");
    create_checklist_step_node(card_cal_prog, 3, "Verify & Save");

    /* Default check list steps visual state initialization */
    update_checklist_step(0, "Prepare Sensor", 0);
    update_checklist_step(1, "Calibrate at 21% (Room Air)", 0);
    update_checklist_step(2, "Calibrate at 100% (Oxygen)", 0);
    update_checklist_step(3, "Verify & Save", 0);

    /* Estimated Countdown Details */
    lv_obj_t * cal_time_title = lv_label_create(card_cal_prog);
    lv_label_set_text(cal_time_title, "Estimated Time Remaining");
    lv_obj_set_style_text_font(cal_time_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cal_time_title, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(cal_time_title, 180, 185);

    cal_time_lbl = lv_label_create(card_cal_prog);
    lv_label_set_text(cal_time_lbl, "00:00 min");
    lv_obj_set_style_text_font(cal_time_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cal_time_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(cal_time_lbl, 350, 184);

    cal_time_bar = lv_bar_create(card_cal_prog);
    lv_obj_set_size(cal_time_bar, 250, 6);
    lv_obj_set_pos(cal_time_bar, 180, 206);
    lv_bar_set_range(cal_time_bar, 0, 100);
    lv_bar_set_value(cal_time_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(cal_time_bar, COLOR_CARD_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cal_time_bar, COLOR_ACCENT_BLUE, LV_PART_INDICATOR);

    /* CANCEL button */
    btn_cancel_cal = lv_button_create(card_cal_prog);
    lv_obj_set_size(btn_cancel_cal, 150, 28);
    lv_obj_set_pos(btn_cancel_cal, 10, 200);
    lv_obj_set_style_bg_opa(btn_cancel_cal, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn_cancel_cal, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_cancel_cal, 1, 0);
    lv_obj_set_style_radius(btn_cancel_cal, 4, 0);
    lv_obj_add_state(btn_cancel_cal, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn_cancel_cal, cancel_calibration_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_cancel_cal_lbl = lv_label_create(btn_cancel_cal);
    lv_label_set_text(btn_cancel_cal_lbl, LV_SYMBOL_CLOSE "  CANCEL CALIBRATION");
    lv_obj_set_style_text_font(btn_cancel_cal_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_cancel_cal_lbl, COLOR_ACCENT_RED, 0);
    lv_obj_center(btn_cancel_cal_lbl);

    /* Panel E: REAL-TIME OXYGEN READING CHART */
    lv_obj_t * card_chart = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_chart, 545, 250);
    lv_obj_set_pos(card_chart, 720, 410);
    lv_obj_set_style_bg_color(card_chart, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_chart, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_chart, 1, 0);
    lv_obj_set_style_radius(card_chart, 12, 0);
    lv_obj_set_style_pad_all(card_chart, 10, 0);

    lv_obj_t * chart_title = lv_label_create(card_chart);
    lv_label_set_text(chart_title, "REAL-TIME OXYGEN READING");
    lv_obj_set_style_text_font(chart_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chart_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(chart_title, 10, 5);

    /* Dropdown 5 min selector */
    lv_obj_t * dropdown_box = lv_obj_create(card_chart);
    lv_obj_set_size(dropdown_box, 80, 24);
    lv_obj_set_pos(dropdown_box, 450, 2);
    lv_obj_set_style_bg_opa(dropdown_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(dropdown_box, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(dropdown_box, 1, 0);
    lv_obj_set_style_radius(dropdown_box, 4, 0);
    lv_obj_set_style_pad_all(dropdown_box, 0, 0);

    lv_obj_t * dropdown_lbl = lv_label_create(dropdown_box);
    lv_label_set_text(dropdown_lbl, "5 min  v");
    lv_obj_set_style_text_font(dropdown_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(dropdown_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_center(dropdown_lbl);

    /* Line Chart */
    o2_chart = lv_chart_create(card_chart);
    lv_obj_set_size(o2_chart, 360, 160);
    lv_obj_set_pos(o2_chart, 40, 45);
    lv_chart_set_type(o2_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(o2_chart, 50);
    lv_chart_set_range(o2_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);

    /* Style the chart grid */
    lv_obj_set_style_bg_color(o2_chart, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(o2_chart, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(o2_chart, 1, 0);
    lv_obj_set_style_pad_all(o2_chart, 0, 0);

    /* Series color */
    o2_series = lv_chart_add_series(o2_chart, COLOR_ACCENT_BLUE, LV_CHART_AXIS_PRIMARY_Y);

    /* Populate historical data values starting around 40 */
    for (int i = 0; i < 50; i++) {
        lv_chart_set_next_value(o2_chart, o2_series, 39 + rand() % 3);
    }

    /* Grid lines setup manually if needed, standard tick markings */
    lv_chart_set_div_line_count(o2_chart, 5, 6);

    /* Y Axis Labels (0, 25, 50, 75, 100) */
    int y_labels_val[] = {100, 75, 50, 25, 0};
    int y_labels_pos[] = {45, 85, 125, 165, 200};
    for (int i = 0; i < 5; i++) {
        lv_obj_t * lbl = lv_label_create(card_chart);
        lv_label_set_text_fmt(lbl, "%d", y_labels_val[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_MUTED, 0);
        lv_obj_set_pos(lbl, 10, y_labels_pos[i]);
    }

    /* X Axis Labels (10:19, 10:20, 10:21, 10:22, 10:23, 10:24) */
    const char * x_labels[] = {"10:19", "10:20", "10:21", "10:22", "10:23", "10:24"};
    int x_labels_pos[] = {40, 100, 160, 220, 280, 340};
    for (int i = 0; i < 6; i++) {
        lv_obj_t * lbl = lv_label_create(card_chart);
        lv_label_set_text(lbl, x_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_MUTED, 0);
        lv_obj_set_pos(lbl, x_labels_pos[i] + 15, 210);
    }

    /* Overlay indicator box on the right */
    lv_obj_t * overlay_box = lv_obj_create(card_chart);
    lv_obj_set_size(overlay_box, 110, 56);
    lv_obj_set_pos(overlay_box, 420, 100);
    lv_obj_set_style_bg_color(overlay_box, lv_color_hex(0x0C2037), 0);
    lv_obj_set_style_border_color(overlay_box, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(overlay_box, 1, 0);
    lv_obj_set_style_radius(overlay_box, 6, 0);
    lv_obj_set_style_pad_all(overlay_box, 4, 0);

    chart_digits_lbl = lv_label_create(overlay_box);
    lv_label_set_text(chart_digits_lbl, "40.5 %");
    lv_obj_set_style_text_font(chart_digits_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chart_digits_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(chart_digits_lbl, LV_ALIGN_TOP_MID, 0, 4);

    chart_time_lbl = lv_label_create(overlay_box);
    lv_label_set_text(chart_time_lbl, "10:24:15 AM");
    lv_obj_set_style_text_font(chart_time_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chart_time_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(chart_time_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* ==================================================================== */
    /* 5. FOOTER DETAILS                                                    */
    /* ==================================================================== */
    lv_obj_t * footer_cont = lv_obj_create(main_screen_obj);
    lv_obj_set_size(footer_cont, 1020, 115);
    lv_obj_set_pos(footer_cont, 250, 675);
    lv_obj_set_style_bg_opa(footer_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer_cont, 0, 0);
    lv_obj_set_style_pad_all(footer_cont, 0, 0);

    /* Card 1: CALIBRATION INFORMATION */
    lv_obj_t * card_info = lv_obj_create(footer_cont);
    lv_obj_set_size(card_info, 195, 110);
    lv_obj_set_pos(card_info, 0, 0);
    lv_obj_set_style_bg_color(card_info, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_info, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_info, 1, 0);
    lv_obj_set_style_radius(card_info, 8, 0);
    lv_obj_set_style_pad_all(card_info, 8, 0);

    lv_obj_t * info_title_lbl = lv_label_create(card_info);
    lv_label_set_text(info_title_lbl, LV_SYMBOL_DIRECTORY "  CALIBRATION INFO");
    lv_obj_set_style_text_font(info_title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(info_title_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(info_title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * info_desc_lbl = lv_label_create(card_info);
    lv_label_set_text(info_desc_lbl, "Regular calibration ensures accuracy. Recommended daily or after replacement.");
    lv_obj_set_style_text_font(info_desc_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(info_desc_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(info_desc_lbl, LV_ALIGN_TOP_LEFT, 0, 20);

    /* Card 2: ENVIRONMENT CONDITIONS */
    lv_obj_t * card_env = lv_obj_create(footer_cont);
    lv_obj_set_size(card_env, 245, 110);
    lv_obj_set_pos(card_env, 205, 0);
    lv_obj_set_style_bg_color(card_env, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_env, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_env, 1, 0);
    lv_obj_set_style_radius(card_env, 8, 0);
    lv_obj_set_style_pad_all(card_env, 8, 0);

    lv_obj_t * env_title_lbl = lv_label_create(card_env);
    lv_label_set_text(env_title_lbl, LV_SYMBOL_SETTINGS "  ENVIRONMENT CONDITIONS");
    lv_obj_set_style_text_font(env_title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(env_title_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(env_title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Columns for Temperature, Humidity, Pressure */
    /* Temperature */
    lv_obj_t * env_temp_lbl = lv_label_create(card_env);
    lv_label_set_text(env_temp_lbl, "Temp\n#ffffff 24.5 °C#");
    lv_label_set_recolor(env_temp_lbl, true);
    lv_obj_set_style_text_font(env_temp_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(env_temp_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(env_temp_lbl, 5, 26);

    /* Humidity */
    lv_obj_t * env_humi_lbl = lv_label_create(card_env);
    lv_label_set_text(env_humi_lbl, "Humidity\n#ffffff 45 %#");
    lv_label_set_recolor(env_humi_lbl, true);
    lv_obj_set_style_text_font(env_humi_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(env_humi_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(env_humi_lbl, 85, 26);

    /* Pressure */
    lv_obj_t * env_pres_lbl = lv_label_create(card_env);
    lv_label_set_text(env_pres_lbl, "Pressure\n#ffffff 1013 hPa#");
    lv_label_set_recolor(env_pres_lbl, true);
    lv_obj_set_style_text_font(env_pres_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(env_pres_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(env_pres_lbl, 155, 26);

    /* Card 3: OXYGEN SOURCE */
    lv_obj_t * card_source = lv_obj_create(footer_cont);
    lv_obj_set_size(card_source, 290, 110);
    lv_obj_set_pos(card_source, 460, 0);
    lv_obj_set_style_bg_color(card_source, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_source, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_source, 1, 0);
    lv_obj_set_style_radius(card_source, 8, 0);
    lv_obj_set_style_pad_all(card_source, 8, 0);

    lv_obj_t * src_title_lbl = lv_label_create(card_source);
    lv_label_set_text(src_title_lbl, LV_SYMBOL_CHARGE "  OXYGEN SOURCE");
    lv_obj_set_style_text_font(src_title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(src_title_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(src_title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * src_desc_lbl = lv_label_create(card_source);
    lv_label_set_text(src_desc_lbl, "Source: Hospital Pipeline\nPurity: #00E676 99.5%#  " LV_SYMBOL_OK);
    lv_label_set_recolor(src_desc_lbl, true);
    lv_obj_set_style_text_font(src_desc_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(src_desc_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(src_desc_lbl, LV_ALIGN_TOP_LEFT, 0, 24);

    /* Card 4: NOTES */
    lv_obj_t * card_notes = lv_obj_create(footer_cont);
    lv_obj_set_size(card_notes, 255, 110);
    lv_obj_set_pos(card_notes, 760, 0);
    lv_obj_set_style_bg_color(card_notes, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_notes, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_notes, 1, 0);
    lv_obj_set_style_radius(card_notes, 8, 0);
    lv_obj_set_style_pad_all(card_notes, 8, 0);

    lv_obj_t * notes_title_lbl = lv_label_create(card_notes);
    lv_label_set_text(notes_title_lbl, LV_SYMBOL_EDIT "  NOTES");
    lv_obj_set_style_text_font(notes_title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(notes_title_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(notes_title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * notes_desc_lbl = lv_label_create(card_notes);
    lv_label_set_text(notes_desc_lbl, "Ensure sensor is exposed to 100% oxygen for span calibration.");
    lv_obj_set_style_text_font(notes_desc_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(notes_desc_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(notes_desc_lbl, LV_ALIGN_TOP_LEFT, 0, 20);

    /* Disable scrolling tree filter */
    disable_scroll_recursive(main_screen_obj);

    /* Dynamic clock sync timer initialization */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL); /* Run instantly to initialize clock label */

    /* Waveform simulation timer */
    simulation_timer = lv_timer_create(simulation_timer_cb, 200, NULL);

    /* Load Calibration screen */
    lv_screen_load_anim(main_screen_obj, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
