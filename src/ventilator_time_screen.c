#include "ventilator_time_screen.h"
#include "ventilator_settings_screen.h"
#include "ventilator_main_screen.h"
#include "battery_detect.h"
#include "theme_manager.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* Colors matching ventilator UI design system */
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

/* Global time offset in seconds */
time_t ventilator_time_offset = 0;

time_t ventilator_get_current_time(time_t * timer)
{
    time_t now = time(NULL) + ventilator_time_offset;
    if(timer) {
        *timer = now;
    }
    return now;
}

/* Static references for UI components */
static lv_timer_t * clock_timer = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * bat_lbl = NULL;
static lv_obj_t * lbl_live_preview = NULL;
static lv_obj_t * lbl_day_of_week = NULL;
static lv_obj_t * toast_banner = NULL;
static lv_obj_t * lbl_toast_msg = NULL;

/* Stepper Value Labels */
static lv_obj_t * lbl_val_year = NULL;
static lv_obj_t * lbl_val_month = NULL;
static lv_obj_t * lbl_val_day = NULL;
static lv_obj_t * lbl_val_hour = NULL;
static lv_obj_t * lbl_val_min = NULL;
static lv_obj_t * lbl_val_sec = NULL;

/* Buttons for AM/PM & Format */
static lv_obj_t * btn_am = NULL;
static lv_obj_t * btn_pm = NULL;
static lv_obj_t * btn_12h = NULL;
static lv_obj_t * btn_24h = NULL;
static lv_obj_t * day_btns[7] = {NULL};

/* Working draft date & time state */
static int edit_year = 2026;
static int edit_month = 7;   /* 1-12 */
static int edit_day = 30;    /* 1-31 */
static int edit_hour = 10;   /* 0-23 */
static int edit_min = 24;    /* 0-59 */
static int edit_sec = 0;     /* 0-59 */
static bool is_12h_mode = true;

/* Helper array for month names */
static const char * month_names[12] = {
    "Jan (01)", "Feb (02)", "Mar (03)", "Apr (04)",
    "May (05)", "Jun (06)", "Jul (07)", "Aug (08)",
    "Sep (09)", "Oct (10)", "Nov (11)", "Dec (12)"
};

static const char * full_month_names[12] = {
    "January", "February", "March", "April",
    "May", "June", "July", "August",
    "September", "October", "November", "December"
};

static const char * day_names[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char * short_day_names[7] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

/* External disable scroll helper */
extern void disable_scroll_recursive(lv_obj_t * obj);

/* Helper to check leap year */
static bool is_leap_year(int y)
{
    return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

/* Helper to get max days in current month */
static int get_max_days_in_month(int y, int m)
{
    static const int days_in_m[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && is_leap_year(y)) return 29;
    if (m >= 1 && m <= 12) return days_in_m[m - 1];
    return 31;
}

/* Calculate day of week index (0=Sunday, 1=Monday... 6=Saturday) */
static int get_day_of_week_index(int y, int m, int d)
{
    struct tm tm_info;
    memset(&tm_info, 0, sizeof(struct tm));
    tm_info.tm_year = y - 1900;
    tm_info.tm_mon = m - 1;
    tm_info.tm_mday = d;
    tm_info.tm_hour = 12;
    tm_info.tm_isdst = -1;
    mktime(&tm_info);
    return tm_info.tm_wday;
}

/* Forward declaration */
static void update_all_ui_displays(void);

/* Clock timer callback */
static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(lbl_clock) {
        time_t current_t = ventilator_get_current_time(NULL);
        struct tm * time_info = localtime(&current_t);
        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%d %b %Y\n%I:%M %p", time_info);
        lv_label_set_text(lbl_clock, clock_buf);
    }
    if(bat_lbl) {
        battery_update_label(bat_lbl);
    }
}

/* Update all labels and active states in the UI */
static void update_all_ui_displays(void)
{
    /* Clamp day */
    int max_d = get_max_days_in_month(edit_year, edit_month);
    if(edit_day > max_d) edit_day = max_d;
    if(edit_day < 1) edit_day = 1;

    /* Update Stepper Text Labels */
    if(lbl_val_year) lv_label_set_text_fmt(lbl_val_year, "%04d", edit_year);
    if(lbl_val_month) lv_label_set_text(lbl_val_month, month_names[edit_month - 1]);
    if(lbl_val_day) lv_label_set_text_fmt(lbl_val_day, "%02d", edit_day);

    if(is_12h_mode) {
        int display_hour = edit_hour % 12;
        if(display_hour == 0) display_hour = 12;
        if(lbl_val_hour) lv_label_set_text_fmt(lbl_val_hour, "%02d", display_hour);
    } else {
        if(lbl_val_hour) lv_label_set_text_fmt(lbl_val_hour, "%02d", edit_hour);
    }

    if(lbl_val_min) lv_label_set_text_fmt(lbl_val_min, "%02d", edit_min);
    if(lbl_val_sec) lv_label_set_text_fmt(lbl_val_sec, "%02d", edit_sec);

    /* Day of Week calculation */
    int wday = get_day_of_week_index(edit_year, edit_month, edit_day);
    if(lbl_day_of_week) {
        lv_label_set_text_fmt(lbl_day_of_week, "%s", day_names[wday]);
    }

    /* Highlight matching day of week button */
    for(int i = 0; i < 7; i++) {
        if(day_btns[i]) {
            if(i == wday) {
                lv_obj_set_style_bg_color(day_btns[i], COLOR_ACCENT_BLUE, 0);
                lv_obj_set_style_border_color(day_btns[i], COLOR_TEXT_MAIN, 0);
            } else {
                lv_obj_set_style_bg_color(day_btns[i], COLOR_BTN_NAV_BG, 0);
                lv_obj_set_style_border_color(day_btns[i], COLOR_CARD_BORDER, 0);
            }
        }
    }

    /* AM/PM Button styles */
    bool is_pm = (edit_hour >= 12);
    if(btn_am && btn_pm) {
        if(is_12h_mode) {
            lv_obj_clear_state(btn_am, LV_STATE_DISABLED);
            lv_obj_clear_state(btn_pm, LV_STATE_DISABLED);
            if(!is_pm) {
                lv_obj_set_style_bg_color(btn_am, COLOR_ACCENT_BLUE, 0);
                lv_obj_set_style_bg_color(btn_pm, COLOR_BTN_NAV_BG, 0);
            } else {
                lv_obj_set_style_bg_color(btn_am, COLOR_BTN_NAV_BG, 0);
                lv_obj_set_style_bg_color(btn_pm, COLOR_ACCENT_BLUE, 0);
            }
        } else {
            lv_obj_add_state(btn_am, LV_STATE_DISABLED);
            lv_obj_add_state(btn_pm, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(btn_am, COLOR_BTN_NAV_BG, 0);
            lv_obj_set_style_bg_color(btn_pm, COLOR_BTN_NAV_BG, 0);
        }
    }

    /* 12H vs 24H Format button styles */
    if(btn_12h && btn_24h) {
        if(is_12h_mode) {
            lv_obj_set_style_bg_color(btn_12h, COLOR_BTN_NAV_ACTIVE, 0);
            lv_obj_set_style_border_color(btn_12h, COLOR_ACCENT_BLUE, 0);
            lv_obj_set_style_bg_color(btn_24h, COLOR_BTN_NAV_BG, 0);
            lv_obj_set_style_border_color(btn_24h, COLOR_CARD_BORDER, 0);
        } else {
            lv_obj_set_style_bg_color(btn_12h, COLOR_BTN_NAV_BG, 0);
            lv_obj_set_style_border_color(btn_12h, COLOR_CARD_BORDER, 0);
            lv_obj_set_style_bg_color(btn_24h, COLOR_BTN_NAV_ACTIVE, 0);
            lv_obj_set_style_border_color(btn_24h, COLOR_ACCENT_BLUE, 0);
        }
    }

    /* Live Preview Banner Text */
    if(lbl_live_preview) {
        char preview_buf[128];
        if(is_12h_mode) {
            int disp_h = edit_hour % 12;
            if(disp_h == 0) disp_h = 12;
            snprintf(preview_buf, sizeof(preview_buf),
                     "PREVIEW: %s, %02d %s %04d - %02d:%02d:%02d %s",
                     day_names[wday], edit_day, full_month_names[edit_month - 1], edit_year,
                     disp_h, edit_min, edit_sec, is_pm ? "PM" : "AM");
        } else {
            snprintf(preview_buf, sizeof(preview_buf),
                     "PREVIEW: %s, %02d %s %04d - %02d:%02d:%02d",
                     day_names[wday], edit_day, full_month_names[edit_month - 1], edit_year,
                     edit_hour, edit_min, edit_sec);
        }
        lv_label_set_text(lbl_live_preview, preview_buf);
    }
}

/* Stepper Adjust Event Handlers */
static void year_step_cb(lv_event_t * e)
{
    int delta = (intptr_t)lv_event_get_user_data(e);
    edit_year += delta;
    if(edit_year < 2020) edit_year = 2020;
    if(edit_year > 2050) edit_year = 2050;
    update_all_ui_displays();
}

static void month_step_cb(lv_event_t * e)
{
    int delta = (intptr_t)lv_event_get_user_data(e);
    edit_month += delta;
    if(edit_month > 12) edit_month = 1;
    if(edit_month < 1) edit_month = 12;
    update_all_ui_displays();
}

static void day_step_cb(lv_event_t * e)
{
    int delta = (intptr_t)lv_event_get_user_data(e);
    int max_d = get_max_days_in_month(edit_year, edit_month);
    edit_day += delta;
    if(edit_day > max_d) edit_day = 1;
    if(edit_day < 1) edit_day = max_d;
    update_all_ui_displays();
}

static void hour_step_cb(lv_event_t * e)
{
    int delta = (intptr_t)lv_event_get_user_data(e);
    edit_hour = (edit_hour + delta + 24) % 24;
    update_all_ui_displays();
}

static void min_step_cb(lv_event_t * e)
{
    int delta = (intptr_t)lv_event_get_user_data(e);
    edit_min = (edit_min + delta + 60) % 60;
    update_all_ui_displays();
}

static void sec_step_cb(lv_event_t * e)
{
    int delta = (intptr_t)lv_event_get_user_data(e);
    edit_sec = (edit_sec + delta + 60) % 60;
    update_all_ui_displays();
}

/* Format and AM/PM Callbacks */
static void am_pm_cb(lv_event_t * e)
{
    bool target_pm = (bool)(intptr_t)lv_event_get_user_data(e);
    if(target_pm && edit_hour < 12) {
        edit_hour += 12;
    } else if(!target_pm && edit_hour >= 12) {
        edit_hour -= 12;
    }
    update_all_ui_displays();
}

static void format_toggle_cb(lv_event_t * e)
{
    bool target_12h = (bool)(intptr_t)lv_event_get_user_data(e);
    is_12h_mode = target_12h;
    update_all_ui_displays();
}

/* Day of week quick-select callback */
static void day_select_cb(lv_event_t * e)
{
    int target_wday = (intptr_t)lv_event_get_user_data(e);
    int current_wday = get_day_of_week_index(edit_year, edit_month, edit_day);
    int diff = target_wday - current_wday;
    edit_day += diff;
    
    int max_d = get_max_days_in_month(edit_year, edit_month);
    if(edit_day > max_d) {
        edit_day -= max_d;
        edit_month++;
        if(edit_month > 12) {
            edit_month = 1;
            edit_year++;
        }
    } else if(edit_day < 1) {
        edit_month--;
        if(edit_month < 1) {
            edit_month = 12;
            edit_year--;
        }
        edit_day += get_max_days_in_month(edit_year, edit_month);
    }
    update_all_ui_displays();
}

/* Reset to real system clock */
static void reset_to_now_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    time_t raw_t = time(NULL);
    struct tm * tm_i = localtime(&raw_t);
    edit_year = tm_i->tm_year + 1900;
    edit_month = tm_i->tm_mon + 1;
    edit_day = tm_i->tm_mday;
    edit_hour = tm_i->tm_hour;
    edit_min = tm_i->tm_min;
    edit_sec = tm_i->tm_sec;
    update_all_ui_displays();

    if(toast_banner && lbl_toast_msg) {
        lv_obj_clear_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(toast_banner, lv_color_hex(0x0A3B73), 0);
        lv_label_set_text(lbl_toast_msg, LV_SYMBOL_REFRESH " Reset form to system clock!");
    }
}

static void quick_adjust_cb(lv_event_t * e)
{
    int opt_type = (intptr_t)lv_event_get_user_data(e);
    if(opt_type == 0) { edit_min = (edit_min + 15) % 60; }
    else if(opt_type == 1) { edit_min = (edit_min - 15 + 60) % 60; }
    else if(opt_type == 2) { edit_hour = (edit_hour + 1) % 24; }
    else if(opt_type == 3) { edit_hour = (edit_hour - 1 + 24) % 24; }
    update_all_ui_displays();
}

/* Save & Apply callback */
static void save_apply_cb(lv_event_t * e)
{
    LV_UNUSED(e);

    /* Construct target time struct */
    struct tm target_tm;
    memset(&target_tm, 0, sizeof(struct tm));
    target_tm.tm_year = edit_year - 1900;
    target_tm.tm_mon = edit_month - 1;
    target_tm.tm_mday = edit_day;
    target_tm.tm_hour = edit_hour;
    target_tm.tm_min = edit_min;
    target_tm.tm_sec = edit_sec;
    target_tm.tm_isdst = -1;

    time_t target_t = mktime(&target_tm);
    time_t real_now = time(NULL);

    /* Compute offset between target time and real system time */
    ventilator_time_offset = target_t - real_now;

    /* Attempt system clock set using settimeofday (if permitted) */
    struct timeval tv;
    tv.tv_sec = target_t;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    /* Force clock timer update */
    clock_timer_cb(NULL);

    /* Show success toast message */
    if(toast_banner && lbl_toast_msg) {
        lv_obj_clear_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(toast_banner, lv_color_hex(0x0F5132), 0);
        lv_label_set_text(lbl_toast_msg, LV_SYMBOL_OK " Date & Time updated and saved successfully!");
    }
}

/* Back Navigation Callbacks */
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

/* Helper to build a stepper card component */
static lv_obj_t * create_stepper_card(lv_obj_t * parent, int x, int y, int w, int h,
                                       const char * title, lv_event_cb_t dec_cb,
                                       lv_event_cb_t inc_cb, lv_obj_t ** val_label_out)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 6, 0);

    lv_obj_t * t_lbl = lv_label_create(card);
    lv_label_set_text(t_lbl, title);
    lv_obj_set_style_text_font(t_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(t_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(t_lbl, LV_ALIGN_TOP_MID, 0, 2);

    /* Decrement Button [-] */
    lv_obj_t * btn_dec = lv_button_create(card);
    lv_obj_set_size(btn_dec, 44, 42);
    lv_obj_align(btn_dec, LV_ALIGN_LEFT_MID, 4, 10);
    lv_obj_set_style_bg_color(btn_dec, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_dec, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_dec, 1, 0);
    lv_obj_set_style_radius(btn_dec, 8, 0);
    lv_obj_add_event_cb(btn_dec, dec_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);

    lv_obj_t * lbl_minus = lv_label_create(btn_dec);
    lv_label_set_text(lbl_minus, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lbl_minus, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_minus, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_minus);

    /* Value Display */
    lv_obj_t * val_box = lv_obj_create(card);
    lv_obj_set_size(val_box, w - 114, 42);
    lv_obj_align(val_box, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(val_box, lv_color_hex(0x051326), 0);
    lv_obj_set_style_border_color(val_box, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(val_box, 1, 0);
    lv_obj_set_style_radius(val_box, 6, 0);

    lv_obj_t * val_lbl = lv_label_create(val_box);
    lv_label_set_text(val_lbl, "00");
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(val_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(val_lbl);
    if(val_label_out) *val_label_out = val_lbl;

    /* Increment Button [+] */
    lv_obj_t * btn_inc = lv_button_create(card);
    lv_obj_set_size(btn_inc, 44, 42);
    lv_obj_align(btn_inc, LV_ALIGN_RIGHT_MID, -4, 10);
    lv_obj_set_style_bg_color(btn_inc, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_inc, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_inc, 1, 0);
    lv_obj_set_style_radius(btn_inc, 8, 0);
    lv_obj_add_event_cb(btn_inc, inc_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t * lbl_plus = lv_label_create(btn_inc);
    lv_label_set_text(lbl_plus, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_plus, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_plus);

    return card;
}

/**
 * @brief Initialize and display Date & Time Adjustment Screen (1280x800)
 */
void create_ventilator_time_screen(void)
{
    /* Clean up old clock timer */
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }

    /* Initialize draft state from current adjusted system time */
    time_t curr_t = ventilator_get_current_time(NULL);
    struct tm * tm_info = localtime(&curr_t);
    edit_year = tm_info->tm_year + 1900;
    edit_month = tm_info->tm_mon + 1;
    edit_day = tm_info->tm_mday;
    edit_hour = tm_info->tm_hour;
    edit_min = tm_info->tm_min;
    edit_sec = tm_info->tm_sec;

    /* Base Screen */
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ==================================================================== */
    /* 1. TOP HEADER BAR (1280x55)                                         */
    /* ==================================================================== */
    lv_obj_t * top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, 1280, 55);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(top_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_hor(top_bar, 12, 0);

    /* Title Badge (Left) */
    lv_obj_t * mode_box = lv_obj_create(top_bar);
    lv_obj_set_size(mode_box, 260, 42);
    lv_obj_align(mode_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(mode_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(mode_box, 0, 0);
    lv_obj_set_style_radius(mode_box, 6, 0);
    lv_obj_set_style_pad_all(mode_box, 4, 0);

    lv_obj_t * icon_box = lv_obj_create(mode_box);
    lv_obj_set_size(icon_box, 34, 34);
    lv_obj_align(icon_box, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(icon_box, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(icon_box, 6, 0);
    lv_obj_set_style_border_width(icon_box, 0, 0);

    lv_obj_t * clock_icon = lv_label_create(icon_box);
    lv_label_set_text(clock_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(clock_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(clock_icon, lv_color_white(), 0);
    lv_obj_center(clock_icon);

    lv_obj_t * mode_title = lv_label_create(mode_box);
    lv_label_set_text(mode_title, "DATE & TIME SETTINGS");
    lv_obj_set_style_text_font(mode_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mode_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(mode_title, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * mode_sub = lv_label_create(mode_box);
    lv_label_set_text(mode_sub, "Adjust clock, date and day");
    lv_obj_set_style_text_font(mode_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_sub, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(mode_sub, LV_ALIGN_LEFT_MID, 46, 10);

    /* Patient Badge (Middle Left) */
    lv_obj_t * pat_box = lv_obj_create(top_bar);
    lv_obj_set_size(pat_box, 230, 42);
    lv_obj_align(pat_box, LV_ALIGN_LEFT_MID, 272, 0);
    lv_obj_set_style_bg_color(pat_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(pat_box, 0, 0);
    lv_obj_set_style_radius(pat_box, 6, 0);

    lv_obj_t * pat_lbl = lv_label_create(pat_box);
    lv_label_set_text(pat_lbl, LV_SYMBOL_DIRECTORY " John Doe\n#7097ba ID: 12345678 | Male | 45 yrs#");
    lv_label_set_recolor(pat_lbl, true);
    lv_obj_set_style_text_font(pat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pat_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(pat_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* Alarm Banner (Middle) */
    lv_obj_t * alarm_box = lv_obj_create(top_bar);
    lv_obj_set_size(alarm_box, 310, 42);
    lv_obj_align(alarm_box, LV_ALIGN_CENTER, 80, 0);
    lv_obj_set_style_bg_color(alarm_box, lv_color_hex(0x5A0C0C), 0);
    lv_obj_set_style_border_width(alarm_box, 0, 0);
    lv_obj_set_style_radius(alarm_box, 6, 0);

    lv_obj_t * alarm_lbl = lv_label_create(alarm_box);
    lv_label_set_text(alarm_lbl, LV_SYMBOL_BELL "  SYSTEM CLOCK MODE\n   Changes apply immediately");
    lv_obj_set_style_text_font(alarm_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(alarm_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    /* Header Date/Clock Widget */
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
    lv_label_set_text(lbl_clock, "-- --- ----\n--:-- --");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 30, 0);

    /* Battery & Status Box */
    lv_obj_t * right_hdr = lv_obj_create(top_bar);
    lv_obj_set_size(right_hdr, 120, 42);
    lv_obj_align(right_hdr, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(right_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_hdr, 0, 0);

    bat_lbl = lv_label_create(right_hdr);
    battery_update_label(bat_lbl);
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(bat_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    /* ==================================================================== */
    /* 2. LIVE PREVIEW BANNER (Top of Content)                              */
    /* ==================================================================== */
    lv_obj_t * prev_card = lv_obj_create(scr);
    lv_obj_set_size(prev_card, 1240, 48);
    lv_obj_set_pos(prev_card, 20, 65);
    lv_obj_set_style_bg_color(prev_card, lv_color_hex(0x081D38), 0);
    lv_obj_set_style_border_color(prev_card, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(prev_card, 1, 0);
    lv_obj_set_style_radius(prev_card, 8, 0);

    lbl_live_preview = lv_label_create(prev_card);
    lv_label_set_text(lbl_live_preview, "PREVIEW: Loading time...");
    lv_obj_set_style_text_font(lbl_live_preview, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_live_preview, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(lbl_live_preview, LV_ALIGN_LEFT_MID, 16, 0);

    /* Toast Notification Banner (Initially hidden) */
    toast_banner = lv_obj_create(scr);
    lv_obj_set_size(toast_banner, 1240, 36);
    lv_obj_set_pos(toast_banner, 20, 118);
    lv_obj_set_style_bg_color(toast_banner, lv_color_hex(0x0F5132), 0);
    lv_obj_set_style_border_width(toast_banner, 0, 0);
    lv_obj_set_style_radius(toast_banner, 6, 0);
    lv_obj_add_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);

    lbl_toast_msg = lv_label_create(toast_banner);
    lv_label_set_text(lbl_toast_msg, "Notification");
    lv_obj_set_style_text_font(lbl_toast_msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_toast_msg, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_toast_msg, LV_ALIGN_LEFT_MID, 16, 0);

    /* ==================================================================== */
    /* 3. TIME ADJUSTMENT PANEL (Left Panel)                                */
    /* ==================================================================== */
    lv_obj_t * panel_time = lv_obj_create(scr);
    lv_obj_set_size(panel_time, 595, 575);
    lv_obj_set_pos(panel_time, 20, 160);
    lv_obj_set_style_bg_color(panel_time, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(panel_time, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(panel_time, 1, 0);
    lv_obj_set_style_radius(panel_time, 12, 0);
    lv_obj_set_style_pad_all(panel_time, 16, 0);

    /* Title */
    lv_obj_t * t_hdr = lv_label_create(panel_time);
    lv_label_set_text(t_hdr, LV_SYMBOL_SETTINGS "  TIME ADJUSTMENT");
    lv_obj_set_style_text_font(t_hdr, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(t_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Format Selector Buttons (12H / 24H) */
    lv_obj_t * fmt_cont = lv_obj_create(panel_time);
    lv_obj_set_size(fmt_cont, 220, 36);
    lv_obj_align(fmt_cont, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_set_style_bg_opa(fmt_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fmt_cont, 0, 0);
    lv_obj_set_style_pad_all(fmt_cont, 0, 0);

    btn_12h = lv_button_create(fmt_cont);
    lv_obj_set_size(btn_12h, 100, 34);
    lv_obj_align(btn_12h, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_12h, COLOR_BTN_NAV_ACTIVE, 0);
    lv_obj_set_style_border_color(btn_12h, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(btn_12h, 1, 0);
    lv_obj_set_style_radius(btn_12h, 6, 0);
    lv_obj_add_event_cb(btn_12h, format_toggle_cb, LV_EVENT_CLICKED, (void*)(intptr_t)true);

    lv_obj_t * l_12 = lv_label_create(btn_12h);
    lv_label_set_text(l_12, "12-HOUR");
    lv_obj_set_style_text_font(l_12, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(l_12, COLOR_TEXT_MAIN, 0);
    lv_obj_center(l_12);

    btn_24h = lv_button_create(fmt_cont);
    lv_obj_set_size(btn_24h, 100, 34);
    lv_obj_align(btn_24h, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_24h, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_24h, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_24h, 1, 0);
    lv_obj_set_style_radius(btn_24h, 6, 0);
    lv_obj_add_event_cb(btn_24h, format_toggle_cb, LV_EVENT_CLICKED, (void*)(intptr_t)false);

    lv_obj_t * l_24 = lv_label_create(btn_24h);
    lv_label_set_text(l_24, "24-HOUR");
    lv_obj_set_style_text_font(l_24, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(l_24, COLOR_TEXT_MAIN, 0);
    lv_obj_center(l_24);

    /* Stepper Controls for Hour, Min, Sec */
    create_stepper_card(panel_time, 0, 45, 175, 95, "HOURS", hour_step_cb, hour_step_cb, &lbl_val_hour);
    create_stepper_card(panel_time, 190, 45, 175, 95, "MINUTES", min_step_cb, min_step_cb, &lbl_val_min);
    create_stepper_card(panel_time, 380, 45, 175, 95, "SECONDS", sec_step_cb, sec_step_cb, &lbl_val_sec);

    /* AM / PM Segment Buttons */
    lv_obj_t * ampm_card = lv_obj_create(panel_time);
    lv_obj_set_size(ampm_card, 555, 75);
    lv_obj_set_pos(ampm_card, 0, 155);
    lv_obj_set_style_bg_color(ampm_card, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(ampm_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(ampm_card, 1, 0);
    lv_obj_set_style_radius(ampm_card, 8, 0);

    lv_obj_t * ampm_hdr = lv_label_create(ampm_card);
    lv_label_set_text(ampm_hdr, "PERIOD (AM / PM)");
    lv_obj_set_style_text_font(ampm_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ampm_hdr, COLOR_TEXT_MUTED, 0);
    lv_obj_align(ampm_hdr, LV_ALIGN_LEFT_MID, 16, 0);

    btn_am = lv_button_create(ampm_card);
    lv_obj_set_size(btn_am, 160, 48);
    lv_obj_align(btn_am, LV_ALIGN_RIGHT_MID, -180, 0);
    lv_obj_set_style_bg_color(btn_am, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(btn_am, 8, 0);
    lv_obj_add_event_cb(btn_am, am_pm_cb, LV_EVENT_CLICKED, (void*)(intptr_t)false);

    lv_obj_t * lbl_am = lv_label_create(btn_am);
    lv_label_set_text(lbl_am, "AM  (Morning)");
    lv_obj_set_style_text_font(lbl_am, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_am, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_am);

    btn_pm = lv_button_create(ampm_card);
    lv_obj_set_size(btn_pm, 160, 48);
    lv_obj_align(btn_pm, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_pm, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_radius(btn_pm, 8, 0);
    lv_obj_add_event_cb(btn_pm, am_pm_cb, LV_EVENT_CLICKED, (void*)(intptr_t)true);

    lv_obj_t * lbl_pm = lv_label_create(btn_pm);
    lv_label_set_text(lbl_pm, "PM  (Afternoon)");
    lv_obj_set_style_text_font(lbl_pm, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_pm, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_pm);

    /* Quick Time Adjust Shortcuts */
    lv_obj_t * quick_card = lv_obj_create(panel_time);
    lv_obj_set_size(quick_card, 555, 275);
    lv_obj_set_pos(quick_card, 0, 245);
    lv_obj_set_style_bg_color(quick_card, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(quick_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(quick_card, 1, 0);
    lv_obj_set_style_radius(quick_card, 8, 0);

    lv_obj_t * q_hdr = lv_label_create(quick_card);
    lv_label_set_text(q_hdr, "QUICK TIME ADJUSTMENTS");
    lv_obj_set_style_text_font(q_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(q_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(q_hdr, LV_ALIGN_TOP_LEFT, 12, 10);

    /* +15 min, -15 min, +1 hour, -1 hour quick buttons */
    const char * q_txts[4] = {"+15 MIN", "-15 MIN", "+1 HOUR", "-1 HOUR"};

    for(int i = 0; i < 4; i++) {
        lv_obj_t * q_btn = lv_button_create(quick_card);
        lv_obj_set_size(q_btn, 120, 44);
        lv_obj_set_pos(q_btn, 12 + i * 132, 45);
        lv_obj_set_style_bg_color(q_btn, COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(q_btn, COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(q_btn, 1, 0);
        lv_obj_set_style_radius(q_btn, 6, 0);

        lv_obj_t * q_lbl = lv_label_create(q_btn);
        lv_label_set_text(q_lbl, q_txts[i]);
        lv_obj_set_style_text_font(q_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(q_lbl, COLOR_TEXT_MAIN, 0);
        lv_obj_center(q_lbl);

        lv_obj_add_event_cb(q_btn, quick_adjust_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    /* Reset to Current System Clock Button */
    lv_obj_t * btn_reset_now = lv_button_create(quick_card);
    lv_obj_set_size(btn_reset_now, 530, 48);
    lv_obj_set_pos(btn_reset_now, 12, 110);
    lv_obj_set_style_bg_color(btn_reset_now, COLOR_BTN_NAV_ACTIVE, 0);
    lv_obj_set_style_border_color(btn_reset_now, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(btn_reset_now, 1, 0);
    lv_obj_set_style_radius(btn_reset_now, 8, 0);
    lv_obj_add_event_cb(btn_reset_now, reset_to_now_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_r_now = lv_label_create(btn_reset_now);
    lv_label_set_text(lbl_r_now, LV_SYMBOL_REFRESH "  SYNC / RESET TO CURRENT REAL SYSTEM CLOCK");
    lv_obj_set_style_text_font(lbl_r_now, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_r_now, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_r_now);

    /* Info note */
    lv_obj_t * t_info = lv_label_create(quick_card);
    lv_label_set_text(t_info, LV_SYMBOL_BELL " Adjustments affect system time, logs & waveforms across all screens.");
    lv_obj_set_style_text_font(t_info, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(t_info, COLOR_TEXT_MUTED, 0);
    lv_obj_align(t_info, LV_ALIGN_BOTTOM_LEFT, 12, -12);


    /* ==================================================================== */
    /* 4. DATE & DAY ADJUSTMENT PANEL (Right Panel)                         */
    /* ==================================================================== */
    lv_obj_t * panel_date = lv_obj_create(scr);
    lv_obj_set_size(panel_date, 625, 575);
    lv_obj_set_pos(panel_date, 635, 160);
    lv_obj_set_style_bg_color(panel_date, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(panel_date, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(panel_date, 1, 0);
    lv_obj_set_style_radius(panel_date, 12, 0);
    lv_obj_set_style_pad_all(panel_date, 16, 0);

    /* Title */
    lv_obj_t * d_hdr = lv_label_create(panel_date);
    lv_label_set_text(d_hdr, LV_SYMBOL_LIST "  DATE & DAY ADJUSTMENT");
    lv_obj_set_style_text_font(d_hdr, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(d_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(d_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Steppers for Day, Month, Year */
    create_stepper_card(panel_date, 0, 45, 185, 95, "DAY (01 - 31)", day_step_cb, day_step_cb, &lbl_val_day);
    create_stepper_card(panel_date, 200, 45, 195, 95, "MONTH", month_step_cb, month_step_cb, &lbl_val_month);
    create_stepper_card(panel_date, 410, 45, 180, 95, "YEAR", year_step_cb, year_step_cb, &lbl_val_year);

    /* Day of Week Display Card */
    lv_obj_t * dow_card = lv_obj_create(panel_date);
    lv_obj_set_size(dow_card, 590, 80);
    lv_obj_set_pos(dow_card, 0, 155);
    lv_obj_set_style_bg_color(dow_card, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(dow_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(dow_card, 1, 0);
    lv_obj_set_style_radius(dow_card, 8, 0);

    lv_obj_t * dow_title = lv_label_create(dow_card);
    lv_label_set_text(dow_title, "DYNAMIC DAY OF WEEK");
    lv_obj_set_style_text_font(dow_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(dow_title, COLOR_TEXT_MUTED, 0);
    lv_obj_align(dow_title, LV_ALIGN_LEFT_MID, 16, 0);

    lbl_day_of_week = lv_label_create(dow_card);
    lv_label_set_text(lbl_day_of_week, "Thursday");
    lv_obj_set_style_text_font(lbl_day_of_week, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_day_of_week, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(lbl_day_of_week, LV_ALIGN_RIGHT_MID, -24, 0);

    /* Quick Day Selector Buttons (Sun, Mon, Tue, Wed, Thu, Fri, Sat) */
    lv_obj_t * days_card = lv_obj_create(panel_date);
    lv_obj_set_size(days_card, 590, 100);
    lv_obj_set_pos(days_card, 0, 245);
    lv_obj_set_style_bg_color(days_card, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(days_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(days_card, 1, 0);
    lv_obj_set_style_radius(days_card, 8, 0);

    lv_obj_t * ds_hdr = lv_label_create(days_card);
    lv_label_set_text(ds_hdr, "QUICK DAY SELECTOR");
    lv_obj_set_style_text_font(ds_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ds_hdr, COLOR_TEXT_MUTED, 0);
    lv_obj_align(ds_hdr, LV_ALIGN_TOP_LEFT, 12, 8);

    for(int i = 0; i < 7; i++) {
        day_btns[i] = lv_button_create(days_card);
        lv_obj_set_size(day_btns[i], 74, 44);
        lv_obj_set_pos(day_btns[i], 10 + i * 81, 36);
        lv_obj_set_style_bg_color(day_btns[i], COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(day_btns[i], COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(day_btns[i], 1, 0);
        lv_obj_set_style_radius(day_btns[i], 6, 0);
        lv_obj_add_event_cb(day_btns[i], day_select_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t * d_lbl = lv_label_create(day_btns[i]);
        lv_label_set_text(d_lbl, short_day_names[i]);
        lv_obj_set_style_text_font(d_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(d_lbl, COLOR_TEXT_MAIN, 0);
        lv_obj_center(d_lbl);
    }

    /* Save & Apply Big Button Card */
    lv_obj_t * action_card = lv_obj_create(panel_date);
    lv_obj_set_size(action_card, 590, 160);
    lv_obj_set_pos(action_card, 0, 355);
    lv_obj_set_style_bg_color(action_card, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(action_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(action_card, 1, 0);
    lv_obj_set_style_radius(action_card, 8, 0);

    lv_obj_t * btn_save = lv_button_create(action_card);
    lv_obj_set_size(btn_save, 560, 56);
    lv_obj_set_pos(btn_save, 14, 20);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x0F5132), 0);
    lv_obj_set_style_border_color(btn_save, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_border_width(btn_save, 1, 0);
    lv_obj_set_style_radius(btn_save, 8, 0);
    lv_obj_add_event_cb(btn_save, save_apply_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, LV_SYMBOL_OK "  SAVE & APPLY NEW DATE & TIME");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_save, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_save);

    lv_obj_t * btn_cancel = lv_button_create(action_card);
    lv_obj_set_size(btn_cancel, 560, 44);
    lv_obj_set_pos(btn_cancel, 14, 90);
    lv_obj_set_style_bg_color(btn_cancel, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_cancel, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, back_to_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE "  CANCEL / DISCARD CHANGES");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_cancel, COLOR_TEXT_MUTED, 0);
    lv_obj_center(lbl_cancel);


    /* ==================================================================== */
    /* 5. BOTTOM FOOTER NAVIGATION BAR                                      */
    /* ==================================================================== */
    lv_obj_t * bot_bar = lv_obj_create(scr);
    lv_obj_set_size(bot_bar, 1260, 55);
    lv_obj_set_pos(bot_bar, 10, 740);
    lv_obj_set_style_bg_opa(bot_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bot_bar, 0, 0);
    lv_obj_set_style_pad_all(bot_bar, 0, 0);

    /* BACK TO SETTINGS Button */
    lv_obj_t * btn_back_set = lv_button_create(bot_bar);
    lv_obj_set_size(btn_back_set, 210, 48);
    lv_obj_align(btn_back_set, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_back_set, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_back_set, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_back_set, 1, 0);
    lv_obj_set_style_radius(btn_back_set, 8, 0);
    lv_obj_add_event_cb(btn_back_set, back_to_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_b_set = lv_label_create(btn_back_set);
    lv_label_set_text(lbl_b_set, LV_SYMBOL_LEFT "  BACK TO SETTINGS");
    lv_obj_set_style_text_font(lbl_b_set, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_b_set, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_b_set);

    /* HOME Button */
    lv_obj_t * home_btn = lv_button_create(bot_bar);
    lv_obj_set_size(home_btn, 140, 48);
    lv_obj_align(home_btn, LV_ALIGN_LEFT_MID, 225, 0);
    lv_obj_set_style_bg_color(home_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(home_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(home_btn, 1, 0);
    lv_obj_set_style_radius(home_btn, 8, 0);
    lv_obj_add_event_cb(home_btn, back_to_home_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, LV_SYMBOL_HOME "  HOME");
    lv_obj_set_style_text_font(home_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(home_lbl);

    /* Disable scrolling tree filter */
    disable_scroll_recursive(scr);

    /* Update all UI displays with initial values */
    update_all_ui_displays();

    /* Clock dynamic sync timer */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);

    /* Load Time Screen */
    lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
