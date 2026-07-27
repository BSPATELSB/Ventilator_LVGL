#include "ventilator_diagnostics_screen.h"
#include "ventilator_main_screen.h"
#include "ventilator_settings_screen.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Colors matching reference UI screenshot and design tokens */
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

/* Static references */
static lv_timer_t * clock_timer = NULL;
static lv_timer_t * self_test_timer = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * main_screen_obj = NULL;

/* Interactive labels and widget references */
static lv_obj_t * log_list_cont = NULL;
static lv_obj_t * summary_status_val_lbl = NULL;
static lv_obj_t * summary_active_errors_lbl = NULL;
static lv_obj_t * run_test_btn = NULL;
static lv_obj_t * run_test_lbl = NULL;

/* Circular meters values */
static lv_obj_t * arc_cpu = NULL;
static lv_obj_t * lbl_cpu_val = NULL;
static lv_obj_t * arc_ram = NULL;
static lv_obj_t * lbl_ram_val = NULL;
static lv_obj_t * arc_temp = NULL;
static lv_obj_t * lbl_temp_val = NULL;
static lv_obj_t * arc_fan = NULL;
static lv_obj_t * lbl_fan_val = NULL;
static lv_obj_t * arc_battery = NULL;
static lv_obj_t * lbl_battery_val = NULL;

/* Dynamic Log structure */
typedef struct {
    const char * time;
    const char * level;
    const char * source;
    const char * message;
    uint32_t color_hex;
} log_entry_t;

/* Standard simulator log lines matching screenshot */
static log_entry_t standard_logs[] = {
    {"20 May 2024 10:24:01", "INFO", "System", "System startup completed successfully", 0x00A8FF},
    {"20 May 2024 10:23:59", "INFO", "Ventilator", "Ventilation module initialized", 0x00A8FF},
    {"20 May 2024 10:23:58", "INFO", "Sensors", "All sensors online", 0x00A8FF},
    {"20 May 2024 10:23:55", "WARN", "Battery", "Battery charge high", 0xFFD600},
    {"20 May 2024 10:23:50", "INFO", "Network", "Ethernet link up (1000 Mbps)", 0x00A8FF},
    {"20 May 2024 10:23:45", "ERROR", "CAN", "CAN node #3 not responding", 0xD50000},
    {"20 May 2024 10:23:40", "INFO", "System", "Self test completed", 0x00A8FF}
};

/* Forward declarations */
extern void disable_scroll_recursive(lv_obj_t * obj);
static void back_to_settings_cb(lv_event_t * e);
static void back_to_home_cb(lv_event_t * e);
static void run_self_test_cb(lv_event_t * e);

/* Dynamic clock timer callback */
static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if (lbl_clock) {
        time_t raw_time;
        struct tm * time_info;
        time(&raw_time);
        time_info = localtime(&raw_time);
        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%d %b %Y\n%I:%M %p", time_info);
        lv_label_set_text(lbl_clock, clock_buf);
    }
}

/* Helper to render a logs table row */
static void add_log_row(lv_obj_t * parent, const char * time, const char * level, const char * source, const char * message, lv_color_t level_color)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_size(row, 690, 24);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_margin_all(row, 0, 0);

    /* Time */
    lv_obj_t * lbl_time = lv_label_create(row);
    lv_label_set_text(lbl_time, time);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_time, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(lbl_time, 10, 3);

    /* Level */
    lv_obj_t * lbl_level = lv_label_create(row);
    lv_label_set_text(lbl_level, level);
    lv_obj_set_style_text_font(lbl_level, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_level, level_color, 0);
    lv_obj_set_pos(lbl_level, 160, 3);

    /* Source */
    lv_obj_t * lbl_source = lv_label_create(row);
    lv_label_set_text(lbl_source, source);
    lv_obj_set_style_text_font(lbl_source, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_source, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(lbl_source, 240, 3);

    /* Message */
    lv_obj_t * lbl_msg = lv_label_create(row);
    lv_label_set_text(lbl_msg, message);
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_msg, level_color, 0); /* Highlight messages matching level color as reference screenshot */
    if (level_color.blue == COLOR_TEXT_MAIN.blue && level_color.red == COLOR_TEXT_MAIN.red) {
        lv_obj_set_style_text_color(lbl_msg, COLOR_TEXT_MAIN, 0);
    } else if (level_color.red == COLOR_ACCENT_BLUE.red && level_color.blue == COLOR_ACCENT_BLUE.blue) {
        lv_obj_set_style_text_color(lbl_msg, COLOR_TEXT_MAIN, 0);
    }
    lv_obj_set_pos(lbl_msg, 320, 3);
}

/* Redraw logs container */
static void populate_logs(void)
{
    if (!log_list_cont) return;

    /* Clean all previous log entries */
    lv_obj_clean(log_list_cont);

    /* Add headers row */
    lv_obj_t * header_row = lv_obj_create(log_list_cont);
    lv_obj_set_size(header_row, 690, 20);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_style_pad_all(header_row, 0, 0);

    const char * headers[] = {"TIME", "LEVEL", "SOURCE", "MESSAGE"};
    int pos_x[] = {10, 160, 240, 320};
    for (int i = 0; i < 4; i++) {
        lv_obj_t * lbl = lv_label_create(header_row);
        lv_label_set_text(lbl, headers[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_MUTED, 0);
        lv_obj_set_pos(lbl, pos_x[i], 0);
    }

    /* Add actual log rows */
    int total_logs = sizeof(standard_logs) / sizeof(standard_logs[0]);
    for (int i = 0; i < total_logs; i++) {
        add_log_row(log_list_cont, standard_logs[i].time, standard_logs[i].level, standard_logs[i].source, standard_logs[i].message, lv_color_hex(standard_logs[i].color_hex));
    }
}

/* Timer callback to finish running self test simulation */
static void self_test_done_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    /* Stop and delete self test timer */
    if (self_test_timer) {
        lv_timer_delete(self_test_timer);
        self_test_timer = NULL;
    }

    /* Restore button status */
    if (run_test_btn && run_test_lbl) {
        lv_obj_clear_state(run_test_btn, LV_STATE_DISABLED);
        lv_label_set_text(run_test_lbl, LV_SYMBOL_CHARGE "  RUN SELF TEST");
        lv_obj_set_style_bg_color(run_test_btn, COLOR_BTN_NAV_BG, 0);
    }

    /* Update status to healthy and clear errors */
    if (summary_status_val_lbl && summary_active_errors_lbl) {
        lv_label_set_text(summary_status_val_lbl, LV_SYMBOL_OK " HEALTHY");
        lv_obj_set_style_text_color(summary_status_val_lbl, COLOR_ACCENT_GREEN, 0);
        lv_label_set_text(summary_active_errors_lbl, "0");
        lv_obj_set_style_text_color(summary_active_errors_lbl, COLOR_TEXT_MUTED, 0);
    }

    /* Modify RAM & CPU arc load simulation back to normal */
    if (arc_cpu && lbl_cpu_val && arc_ram && lbl_ram_val) {
        lv_arc_set_value(arc_cpu, 24);
        lv_label_set_text(lbl_cpu_val, "24%");
        lv_arc_set_value(arc_ram, 52);
        lv_label_set_text(lbl_ram_val, "52%");
    }

    /* Prepend success log message into log history */
    time_t raw_time;
    struct tm * tm_info;
    time(&raw_time);
    tm_info = localtime(&raw_time);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%d %b %Y %H:%M:%S", tm_info);

    add_log_row(log_list_cont, time_str, "INFO", "System", "Full diagnostics diagnostic loop succeeded.", COLOR_ACCENT_GREEN);
}

/* Event handler for self test click */
static void run_self_test_cb(lv_event_t * e)
{
    LV_UNUSED(e);

    /* Disable button to prevent double triggering */
    lv_obj_add_state(run_test_btn, LV_STATE_DISABLED);
    lv_label_set_text(run_test_lbl, "TESTING...");
    lv_obj_set_style_bg_color(run_test_btn, COLOR_BTN_NAV_ACTIVE, 0);

    /* Simulate test CPU load increase */
    if (arc_cpu && lbl_cpu_val && arc_ram && lbl_ram_val) {
        lv_arc_set_value(arc_cpu, 95);
        lv_label_set_text(lbl_cpu_val, "95%");
        lv_arc_set_value(arc_ram, 84);
        lv_label_set_text(lbl_ram_val, "84%");
    }

    /* Prepend self test start log to system logs table */
    time_t raw_time;
    struct tm * tm_info;
    time(&raw_time);
    tm_info = localtime(&raw_time);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%d %b %Y %H:%M:%S", tm_info);

    add_log_row(log_list_cont, time_str, "WARN", "System", "Manual hardware verification self-test initiated", COLOR_ACCENT_YELLOW);

    /* Create one-shot timer to conclude test in 2.5 seconds */
    self_test_timer = lv_timer_create(self_test_done_timer_cb, 2500, NULL);
}

/* Back button callbacks */
static void back_to_settings_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if (self_test_timer) {
        lv_timer_delete(self_test_timer);
        self_test_timer = NULL;
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
    if (self_test_timer) {
        lv_timer_delete(self_test_timer);
        self_test_timer = NULL;
    }
    create_ventilator_main_screen();
}

/* Helper to construct custom styled gauges matching screenshot design */
static void create_health_gauge(lv_obj_t * parent, int index, const char * title, int val, const char * unit, const char * status, lv_color_t color, const char * subtext, lv_obj_t ** out_arc, lv_obj_t ** out_val_lbl)
{
    int x = 10 + index * 138;
    int y = 30;

    lv_obj_t * panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 132, 172);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_margin_all(panel, 0, 0);

    /* Card Title (e.g. CPU LOAD) */
    lv_obj_t * title_lbl = lv_label_create(panel);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 0);

    /* Circular Gauge Arc */
    lv_obj_t * arc = lv_arc_create(panel);
    lv_obj_set_size(arc, 84, 84);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 20);
    lv_arc_set_bg_angles(arc, 135, 45); /* 270 degree gauge matching references */
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, val);

    /* Style the progress arc */
    lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_CARD_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB); /* hide knob */
    lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);

    if (out_arc) {
        *out_arc = arc;
    }

    /* Inner Value Text (e.g. 32%) */
    lv_obj_t * val_lbl = lv_label_create(arc);
    char buf[16];
    if (strcmp(unit, "°C") == 0) {
        snprintf(buf, sizeof(buf), "%d°C", val);
    } else if (strcmp(unit, "%") == 0) {
        snprintf(buf, sizeof(buf), "%d%%", val);
    } else {
        snprintf(buf, sizeof(buf), "%d", val);
    }
    lv_label_set_text(val_lbl, buf);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(val_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(val_lbl);

    if (out_val_lbl) {
        *out_val_lbl = val_lbl;
    }

    /* Normal / Good Status Label */
    lv_obj_t * stat_lbl = lv_label_create(panel);
    lv_label_set_text(stat_lbl, status);
    lv_obj_set_style_text_font(stat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(stat_lbl, color, 0);
    lv_obj_align(stat_lbl, LV_ALIGN_TOP_MID, 0, 112);

    /* Detailed subtext description (e.g. 1.2 GHz / 4 Core) */
    lv_obj_t * sub_lbl = lv_label_create(panel);
    lv_label_set_text(sub_lbl, subtext);
    lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sub_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(sub_lbl, LV_ALIGN_TOP_MID, 0, 134);
}

/* Helper to construct Communication status cards */
static void create_comm_card(lv_obj_t * parent, int index, const char * name, const char * val)
{
    int x = 10 + index * 138;
    int y = 30;

    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, 132, 80);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_margin_all(card, 0, 0);

    /* Title / Icon Row */
    lv_obj_t * name_lbl = lv_label_create(card);
    lv_label_set_text_fmt(name_lbl, "%s", name);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(name_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Green check circle badge */
    lv_obj_t * badge = lv_label_create(card);
    lv_label_set_text(badge, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(badge, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* "Status OK" */
    lv_obj_t * stat_lbl = lv_label_create(card);
    lv_label_set_text(stat_lbl, "Status  #00E676 OK#");
    lv_label_set_recolor(stat_lbl, true);
    lv_obj_set_style_text_font(stat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(stat_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(stat_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -20);

    /* Dynamic sub-measurement (e.g. Bus Load 28%) */
    lv_obj_t * val_lbl = lv_label_create(card);
    lv_label_set_text(val_lbl, val);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(val_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(val_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
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

/* Helper to create Sensor checklist row */
static void create_sensor_row(lv_obj_t * parent, int y_pos, const char * name)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_size(row, 260, 40);
    lv_obj_set_pos(row, 10, y_pos);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    /* Sensor Name label */
    lv_obj_t * name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, name);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(name_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 10, -6);

    /* "OK" status label */
    lv_obj_t * ok_lbl = lv_label_create(row);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ok_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(ok_lbl, LV_ALIGN_LEFT_MID, 10, 8);

    /* Checkmark icon badge */
    lv_obj_t * badge = lv_label_create(row);
    lv_label_set_text(badge, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(badge, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -10, 0);
}

/* Helper to create Diagnostic Summary details */
static void create_summary_row(lv_obj_t * parent, int y_pos, const char * title, const char * val, lv_color_t color, bool is_status)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_size(row, 260, 26);
    lv_obj_set_pos(row, 10, y_pos);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    lv_obj_t * title_lbl = lv_label_create(row);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * val_lbl = lv_label_create(row);
    if (is_status) {
        summary_status_val_lbl = val_lbl;
        lv_label_set_text_fmt(val_lbl, "%s %s", LV_SYMBOL_OK, val);
    } else {
        lv_label_set_text(val_lbl, val);
    }
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(val_lbl, color, 0);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -10, 0);

    /* Stash error label ref to update dynamically on test */
    if (strcmp(title, "Active Errors") == 0) {
        summary_active_errors_lbl = val_lbl;
    }
}

/**
 * @brief Create and render the Service Diagnostics screen (1280x800).
 */
void create_ventilator_diagnostics_screen(void)
{
    /* Clean up old clock timer to prevent resource leak */
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if (self_test_timer) {
        lv_timer_delete(self_test_timer);
        self_test_timer = NULL;
    }

    /* Base screen object */
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

    /* Header title info box */
    lv_obj_t * title_box = lv_obj_create(top_bar);
    lv_obj_set_size(title_box, 250, 42);
    lv_obj_align(title_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(title_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(title_box, 0, 0);
    lv_obj_set_style_radius(title_box, 6, 0);
    lv_obj_set_style_pad_all(title_box, 4, 0);

    /* Icon Badge Container */
    lv_obj_t * title_badge = lv_obj_create(title_box);
    lv_obj_set_size(title_badge, 34, 34);
    lv_obj_align(title_badge, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(title_badge, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(title_badge, 6, 0);
    lv_obj_set_style_border_width(title_badge, 0, 0);

    lv_obj_t * title_icon = lv_label_create(title_badge);
    lv_label_set_text(title_icon, LV_SYMBOL_SETTINGS); /* wrench/gear symbol */
    lv_obj_set_style_text_font(title_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_icon, lv_color_white(), 0);
    lv_obj_center(title_icon);

    lv_obj_t * title_lbl = lv_label_create(title_box);
    lv_label_set_text(title_lbl, "SERVICE DIAGNOSTICS");
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * subtitle_lbl = lv_label_create(title_box);
    lv_label_set_text(subtitle_lbl, "Monitor system health and performance");
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

    lv_obj_t * bat_lbl = lv_label_create(right_hdr);
    lv_label_set_text(bat_lbl, LV_SYMBOL_BATTERY_FULL " 100%");
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bat_lbl, COLOR_ACCENT_GREEN, 0);
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
    lv_obj_set_size(sidebar, 220, 615);
    lv_obj_set_pos(sidebar, 15, 65);
    lv_obj_set_style_bg_color(sidebar, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(sidebar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sidebar, 1, 0);
    lv_obj_set_style_radius(sidebar, 12, 0);
    lv_obj_set_style_pad_all(sidebar, 0, 0);

    /* Sidebar items vertically */
    create_sidebar_btn(sidebar, 15, LV_SYMBOL_IMAGE, "OVERVIEW", true);
    create_sidebar_btn(sidebar, 65, LV_SYMBOL_DIRECTORY, "SYSTEM INFO", false);
    create_sidebar_btn(sidebar, 115, LV_SYMBOL_GPS, "SENSORS", false);
    create_sidebar_btn(sidebar, 165, LV_SYMBOL_WIFI, "COMMUNICATION", false);
    create_sidebar_btn(sidebar, 215, LV_SYMBOL_FILE, "LOGS", false);
    create_sidebar_btn(sidebar, 265, LV_SYMBOL_CHARGE, "TESTS", false);
    create_sidebar_btn(sidebar, 315, LV_SYMBOL_SETTINGS, "MAINTENANCE", false);

    /* Export Report at the bottom of the sidebar */
    create_sidebar_btn(sidebar, 550, LV_SYMBOL_UPLOAD, "EXPORT REPORT", false);

    /* ==================================================================== */
    /* 3. MIDDLE COLUMN PANELS                                              */
    /* ==================================================================== */

    /* Panel A: SYSTEM HEALTH OVERVIEW */
    lv_obj_t * card_health = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_health, 710, 210);
    lv_obj_set_pos(card_health, 250, 65);
    lv_obj_set_style_bg_color(card_health, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_health, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_health, 1, 0);
    lv_obj_set_style_radius(card_health, 12, 0);
    lv_obj_set_style_pad_all(card_health, 10, 0);

    lv_obj_t * health_title = lv_label_create(card_health);
    lv_label_set_text(health_title, "SYSTEM HEALTH OVERVIEW");
    lv_obj_set_style_text_font(health_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(health_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(health_title, 10, 5);

    /* Create 5 progress rings */
    create_health_gauge(card_health, 0, "CPU LOAD", 32, "%", "Normal", COLOR_ACCENT_BLUE, "1.2 GHz / 4 Core", &arc_cpu, &lbl_cpu_val);
    create_health_gauge(card_health, 1, "RAM USAGE", 58, "%", "Normal", COLOR_ACCENT_BLUE, "2.3 GB / 4.0 GB", &arc_ram, &lbl_ram_val);
    create_health_gauge(card_health, 2, "TEMPERATURE", 41, "°C", "Normal", COLOR_ACCENT_BLUE, "Safe Range: < 70°C", &arc_temp, &lbl_temp_val);
    create_health_gauge(card_health, 3, "FAN SPEED", 23, "RPM", "Normal", COLOR_ACCENT_GREEN, "2300 RPM", &arc_fan, &lbl_fan_val);
    if (lbl_fan_val) lv_label_set_text(lbl_fan_val, "2300\n#7097ba RPM#"); /* Custom split text */
    if (lbl_fan_val) lv_label_set_recolor(lbl_fan_val, true);
    create_health_gauge(card_health, 4, "BATTERY HEALTH", 98, "%", "Good", COLOR_ACCENT_GREEN, "Cycle Count: 45", &arc_battery, &lbl_battery_val);

    /* Panel B: COMMUNICATION STATUS */
    lv_obj_t * card_comm = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_comm, 710, 120);
    lv_obj_set_pos(card_comm, 250, 285);
    lv_obj_set_style_bg_color(card_comm, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_comm, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_comm, 1, 0);
    lv_obj_set_style_radius(card_comm, 12, 0);
    lv_obj_set_style_pad_all(card_health, 10, 0);

    lv_obj_t * comm_title = lv_label_create(card_comm);
    lv_label_set_text(comm_title, "COMMUNICATION STATUS");
    lv_obj_set_style_text_font(comm_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(comm_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(comm_title, 10, 5);

    /* Create 5 communication channels */
    create_comm_card(card_comm, 0, "CAN BUS", "Bus Load: 28%");
    create_comm_card(card_comm, 1, "UART", "Baud Rate: 115200");
    create_comm_card(card_comm, 2, "USB", "Devices: 2");
    create_comm_card(card_comm, 3, "ETHERNET", "Link Speed: 1000 Mbps");
    create_comm_card(card_comm, 4, "Wi-Fi", "Signal Strength: -42 dBm");

    /* Panel C: SYSTEM LOGS */
    lv_obj_t * card_logs = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_logs, 710, 260);
    lv_obj_set_pos(card_logs, 250, 420);
    lv_obj_set_style_bg_color(card_logs, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_logs, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_logs, 1, 0);
    lv_obj_set_style_radius(card_logs, 12, 0);
    lv_obj_set_style_pad_all(card_logs, 10, 0);

    lv_obj_t * logs_title = lv_label_create(card_logs);
    lv_label_set_text(logs_title, "SYSTEM LOGS");
    lv_obj_set_style_text_font(logs_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(logs_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(logs_title, 10, 5);

    /* Inner Log items container (scrollable list) */
    log_list_cont = lv_obj_create(card_logs);
    lv_obj_set_size(log_list_cont, 690, 180);
    lv_obj_set_pos(log_list_cont, 0, 30);
    lv_obj_set_style_bg_opa(log_list_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(log_list_cont, 0, 0);
    lv_obj_set_style_pad_all(log_list_cont, 0, 0);
    lv_obj_set_flex_flow(log_list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(log_list_cont, 4, 0);

    populate_logs();

    /* View all logs button */
    lv_obj_t * view_logs_btn = lv_button_create(card_logs);
    lv_obj_set_size(view_logs_btn, 130, 28);
    lv_obj_set_pos(view_logs_btn, 560, 215);
    lv_obj_set_style_bg_opa(view_logs_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(view_logs_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(view_logs_btn, 1, 0);
    lv_obj_set_style_radius(view_logs_btn, 4, 0);

    lv_obj_t * view_logs_lbl = lv_label_create(view_logs_btn);
    lv_label_set_text(view_logs_lbl, "VIEW ALL LOGS");
    lv_obj_set_style_text_font(view_logs_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(view_logs_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(view_logs_lbl);

    /* ==================================================================== */
    /* 4. RIGHT COLUMN PANELS                                               */
    /* ==================================================================== */

    /* Panel D: SENSOR STATUS CHECKLIST */
    lv_obj_t * card_sensors = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_sensors, 280, 335);
    lv_obj_set_pos(card_sensors, 975, 65);
    lv_obj_set_style_bg_color(card_sensors, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_sensors, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_sensors, 1, 0);
    lv_obj_set_style_radius(card_sensors, 12, 0);
    lv_obj_set_style_pad_all(card_sensors, 10, 0);

    lv_obj_t * sensors_title = lv_label_create(card_sensors);
    lv_label_set_text(sensors_title, "SENSOR STATUS");
    lv_obj_set_style_text_font(sensors_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sensors_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(sensors_title, 10, 5);

    /* 6 Sensor Checklist items */
    create_sensor_row(card_sensors, 30, "Pressure Sensor");
    create_sensor_row(card_sensors, 78, "Flow Sensor");
    create_sensor_row(card_sensors, 126, "Oxygen Sensor");
    create_sensor_row(card_sensors, 174, "Temperature Sensor");
    create_sensor_row(card_sensors, 222, "Ambient Sensor");
    create_sensor_row(card_sensors, 270, "Circuit Pressure Sensor");

    /* Panel E: DIAGNOSTIC SUMMARY */
    lv_obj_t * card_summary = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_summary, 280, 260);
    lv_obj_set_pos(card_summary, 975, 420);
    lv_obj_set_style_bg_color(card_summary, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_summary, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_summary, 1, 0);
    lv_obj_set_style_radius(card_summary, 12, 0);
    lv_obj_set_style_pad_all(card_summary, 10, 0);

    lv_obj_t * summary_title = lv_label_create(card_summary);
    lv_label_set_text(summary_title, "DIAGNOSTIC SUMMARY");
    lv_obj_set_style_text_font(summary_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(summary_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(summary_title, 10, 5);

    /* Detail summary fields */
    create_summary_row(card_summary, 30, "Overall System Status", "HEALTHY", COLOR_ACCENT_GREEN, true);
    create_summary_row(card_summary, 65, "Active Errors", "1", COLOR_ACCENT_RED, false);
    create_summary_row(card_summary, 100, "Warnings", "1", COLOR_ACCENT_YELLOW, false);
    create_summary_row(card_summary, 135, "Last Self Test", "20 May 2024 10:23 AM", COLOR_TEXT_MAIN, false);
    create_summary_row(card_summary, 170, "Uptime", "2d 14h 32m", COLOR_TEXT_MAIN, false);
    create_summary_row(card_summary, 205, "Next Maintenance Due", "15 Jun 2024", COLOR_TEXT_MAIN, false);

    /* ==================================================================== */
    /* 5. BOTTOM FOOTER NAVIGATION                                          */
    /* ==================================================================== */
    lv_obj_t * footer_cont = lv_obj_create(main_screen_obj);
    lv_obj_set_size(footer_cont, 1280, 100);
    lv_obj_set_pos(footer_cont, 0, 690);
    lv_obj_set_style_bg_opa(footer_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer_cont, 0, 0);
    lv_obj_set_style_pad_all(footer_cont, 0, 0);

    /* Footer Button A: RUN SELF TEST */
    run_test_btn = lv_button_create(footer_cont);
    lv_obj_set_size(run_test_btn, 220, 48);
    lv_obj_set_pos(run_test_btn, 15, 10);
    lv_obj_set_style_bg_color(run_test_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(run_test_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(run_test_btn, 1, 0);
    lv_obj_set_style_radius(run_test_btn, 8, 0);
    lv_obj_set_style_bg_color(run_test_btn, COLOR_BTN_NAV_ACTIVE, LV_STATE_PRESSED);
    lv_obj_add_event_cb(run_test_btn, run_self_test_cb, LV_EVENT_CLICKED, NULL);

    run_test_lbl = lv_label_create(run_test_btn);
    lv_label_set_text(run_test_lbl, LV_SYMBOL_CHARGE "  RUN SELF TEST");
    lv_obj_set_style_text_font(run_test_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(run_test_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(run_test_lbl);

    /* Footer Button B: SYSTEM INFORMATION */
    lv_obj_t * info_btn = lv_button_create(footer_cont);
    lv_obj_set_size(info_btn, 220, 48);
    lv_obj_set_pos(info_btn, 250, 10);
    lv_obj_set_style_bg_color(info_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(info_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(info_btn, 1, 0);
    lv_obj_set_style_radius(info_btn, 8, 0);
    lv_obj_set_style_bg_color(info_btn, COLOR_BTN_NAV_ACTIVE, LV_STATE_PRESSED);

    lv_obj_t * info_lbl = lv_label_create(info_btn);
    lv_label_set_text(info_lbl, LV_SYMBOL_DIRECTORY "  SYSTEM INFORMATION");
    lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(info_lbl);

    /* Footer Button C: SAVE DIAGNOSTIC DATA */
    lv_obj_t * save_btn = lv_button_create(footer_cont);
    lv_obj_set_size(save_btn, 220, 48);
    lv_obj_set_pos(save_btn, 485, 10);
    lv_obj_set_style_bg_color(save_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(save_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(save_btn, 1, 0);
    lv_obj_set_style_radius(save_btn, 8, 0);
    lv_obj_set_style_bg_color(save_btn, COLOR_BTN_NAV_ACTIVE, LV_STATE_PRESSED);

    lv_obj_t * save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, LV_SYMBOL_SAVE "  SAVE DIAGNOSTIC DATA");
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(save_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(save_lbl);

    /* Footer Button D: BACK TO HOME (Purple action style) */
    lv_obj_t * home_btn = lv_button_create(footer_cont);
    lv_obj_set_size(home_btn, 290, 48);
    lv_obj_set_pos(home_btn, 975, 10);
    lv_obj_set_style_bg_color(home_btn, COLOR_SIDEBAR_ACTIVE, 0);
    lv_obj_set_style_border_color(home_btn, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(home_btn, 1, 0);
    lv_obj_set_style_radius(home_btn, 8, 0);
    lv_obj_set_style_bg_color(home_btn, COLOR_BTN_NAV_ACTIVE, LV_STATE_PRESSED);
    lv_obj_add_event_cb(home_btn, back_to_home_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, LV_SYMBOL_HOME "  BACK TO HOME");
    lv_obj_set_style_text_font(home_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(home_lbl);

    /* Clean scrolling layout tree */
    disable_scroll_recursive(main_screen_obj);

    /* Dynamic clock sync */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL); /* Run instantly to initialize values */

    /* Load Service Diagnostics Screen */
    lv_screen_load_anim(main_screen_obj, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
