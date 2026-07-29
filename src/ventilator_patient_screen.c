#include "ventilator_patient_screen.h"
#include "ventilator_main_screen.h"
#include "ventilator_settings_screen.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Colors matching reference UI screenshot and design system */
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
static lv_obj_t * main_screen_obj = NULL;
static lv_obj_t * lbl_clock = NULL;

/* Forward declarations */
extern void disable_scroll_recursive(lv_obj_t * obj);
static void back_to_settings_cb(lv_event_t * e);
static void back_to_home_cb(lv_event_t * e);

/* Dynamic clock updates */
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

/* Back navigation callbacks */
static void back_to_settings_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
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
    create_ventilator_main_screen();
}

/* Helper to render Sidebar buttons */
static lv_obj_t * create_sidebar_btn(lv_obj_t * parent, int y_pos, const char * symbol, const char * text, bool is_active)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_size(btn, 200, 42);
    lv_obj_set_pos(btn, 10, y_pos);

    if (is_active) {
        /* Highlight active tab with bright blue/purple color */
        lv_obj_set_style_bg_color(btn, COLOR_BTN_NAV_ACTIVE, 0);
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

/* Helper to build detailed cards inside the Personal Info grid */
static void create_personal_grid_card(lv_obj_t * parent, int col, int row, const char * title, const char * val, const char * symbol, const char * suffix)
{
    int x = col * 225;
    int y = row * 105;

    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, 210, 88);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    /* Left Icon badge */
    lv_obj_t * badge = lv_obj_create(card);
    lv_obj_set_size(badge, 42, 42);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x0C2037), 0);
    lv_obj_set_style_border_color(badge, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);

    lv_obj_t * badge_symbol = lv_label_create(badge);
    lv_label_set_text(badge_symbol, symbol);
    lv_obj_set_style_text_font(badge_symbol, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(badge_symbol, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(badge_symbol);

    /* Stacked text descriptions */
    lv_obj_t * title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 58, -12);

    lv_obj_t * val_lbl = lv_label_create(card);
    if (suffix) {
        lv_label_set_text_fmt(val_lbl, "%s #7097ba %s#", val, suffix);
        lv_label_set_recolor(val_lbl, true);
    } else {
        lv_label_set_text(val_lbl, val);
    }
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(val_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(val_lbl, LV_ALIGN_LEFT_MID, 58, 10);
}

/* Helper to render Attending Physician or Diagnosis Medical summary cards */
static void create_medical_detail_card(lv_obj_t * parent, int index, const char * title, const char * desc, const char * symbol, const char * badge_text, bool has_chevron)
{
    int y = index * 110;

    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, 260, 94);
    lv_obj_set_pos(card, 5, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x061528), 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    /* Left Icon Badge */
    lv_obj_t * badge = lv_obj_create(card);
    lv_obj_set_size(badge, 42, 42);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x0C2037), 0);
    lv_obj_set_style_border_color(badge, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);

    lv_obj_t * badge_symbol = lv_label_create(badge);
    lv_label_set_text(badge_symbol, symbol);
    lv_obj_set_style_text_font(badge_symbol, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(badge_symbol, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(badge_symbol);

    /* Stacked textual details */
    lv_obj_t * title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 58, -12);

    lv_obj_t * desc_lbl = lv_label_create(card);
    lv_label_set_text(desc_lbl, desc);
    lv_obj_set_style_text_font(desc_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(desc_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(desc_lbl, LV_ALIGN_LEFT_MID, 58, 8);

    /* Highlight badge (e.g. green "Active" tag) */
    if (badge_text) {
        lv_obj_t * status_lbl = lv_label_create(card);
        lv_label_set_text_fmt(status_lbl, "• #00E676 %s#", badge_text);
        lv_label_set_recolor(status_lbl, true);
        lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(status_lbl, LV_ALIGN_RIGHT_MID, -10, 8);
    }

    /* Right Chevron Indicator */
    if (has_chevron) {
        lv_obj_t * chev = lv_label_create(card);
        lv_label_set_text(chev, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(chev, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(chev, COLOR_TEXT_MUTED, 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -10, 0);
    }
}

/**
 * @brief Create and render the Patient Profile screen (1280x800).
 */
void create_ventilator_patient_screen(void)
{
    /* Clean clock sync timers to prevent memory leaks */
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }

    /* Screen base container */
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

    /* Lungs profile title box */
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
    lv_label_set_text(title_icon, LV_SYMBOL_IMAGE); /* lungs graphic placeholder */
    lv_obj_set_style_text_font(title_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_icon, lv_color_white(), 0);
    lv_obj_center(title_icon);

    lv_obj_t * title_lbl = lv_label_create(title_box);
    lv_label_set_text(title_lbl, "PATIENT PROFILE");
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * subtitle_lbl = lv_label_create(title_box);
    lv_label_set_text(subtitle_lbl, "Patient Information");
    lv_obj_set_style_text_font(subtitle_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle_lbl, COLOR_ACCENT_BLUE, 0);
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

    /* Battery & settings icon box (Right) */
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
    lv_obj_set_size(sidebar, 220, 715);
    lv_obj_set_pos(sidebar, 15, 65);
    lv_obj_set_style_bg_color(sidebar, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(sidebar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sidebar, 1, 0);
    lv_obj_set_style_radius(sidebar, 12, 0);
    lv_obj_set_style_pad_all(sidebar, 0, 0);

    /* Sidebar items vertically alignment */
    create_sidebar_btn(sidebar, 15, LV_SYMBOL_HOME, "DASHBOARD", false);
    create_sidebar_btn(sidebar, 65, LV_SYMBOL_CHARGE, "MONITORING", false);
    create_sidebar_btn(sidebar, 115, LV_SYMBOL_FILE, "TRENDS", false);
    create_sidebar_btn(sidebar, 165, LV_SYMBOL_BELL, "ALARMS", false);
    create_sidebar_btn(sidebar, 215, LV_SYMBOL_IMAGE, "VENTILATOR", false);
    create_sidebar_btn(sidebar, 265, LV_SYMBOL_LIST, "EVENTS", false);
    create_sidebar_btn(sidebar, 315, LV_SYMBOL_DIRECTORY, "PATIENT", true); /* Active */
    create_sidebar_btn(sidebar, 365, LV_SYMBOL_SETTINGS, "SYSTEM", false);

    /* BACK TO HOME button at sidebar footer */
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
    /* 3. MIDDLE COLUMN DETAILS                                             */
    /* ==================================================================== */

    /* Panel A: PERSONAL INFORMATION */
    lv_obj_t * card_personal = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_personal, 710, 370);
    lv_obj_set_pos(card_personal, 250, 65);
    lv_obj_set_style_bg_color(card_personal, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_personal, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_personal, 1, 0);
    lv_obj_set_style_radius(card_personal, 12, 0);
    lv_obj_set_style_pad_all(card_personal, 12, 0);

    lv_obj_t * personal_title = lv_label_create(card_personal);
    lv_label_set_text(personal_title, "PERSONAL INFORMATION");
    lv_obj_set_style_text_font(personal_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(personal_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(personal_title, 10, 5);

    /* Silhouette frame (Left side of card) */
    lv_obj_t * avatar_box = lv_obj_create(card_personal);
    lv_obj_set_size(avatar_box, 200, 320);
    lv_obj_set_pos(avatar_box, 10, 30);
    lv_obj_set_style_bg_color(avatar_box, lv_color_hex(0x061933), 0);
    lv_obj_set_style_border_color(avatar_box, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(avatar_box, 1, 0);
    lv_obj_set_style_radius(avatar_box, 10, 0);
    lv_obj_set_style_pad_all(avatar_box, 0, 0);

    /* Head */
    lv_obj_t * head = lv_obj_create(avatar_box);
    lv_obj_set_size(head, 64, 64);
    lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(head, lv_color_hex(0x007CFF), 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_radius(head, LV_RADIUS_CIRCLE, 0);

    /* Neck */
    lv_obj_t * neck = lv_obj_create(avatar_box);
    lv_obj_set_size(neck, 16, 26);
    lv_obj_align(neck, LV_ALIGN_TOP_MID, 0, 102);
    lv_obj_set_style_bg_color(neck, lv_color_hex(0x006ED0), 0);
    lv_obj_set_style_border_width(neck, 0, 0);
    lv_obj_set_style_radius(neck, 0, 0);

    /* Shoulders outline shape */
    lv_obj_t * shoulders = lv_obj_create(avatar_box);
    lv_obj_set_size(shoulders, 160, 120);
    lv_obj_align(shoulders, LV_ALIGN_BOTTOM_MID, 0, 50);
    lv_obj_set_style_bg_color(shoulders, lv_color_hex(0x005BB7), 0);
    lv_obj_set_style_border_width(shoulders, 0, 0);
    lv_obj_set_style_radius(shoulders, 45, 0);

    /* Edit Profile overlay button */
    lv_obj_t * btn_edit_prof = lv_button_create(avatar_box);
    lv_obj_set_size(btn_edit_prof, 150, 36);
    lv_obj_align(btn_edit_prof, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(btn_edit_prof, lv_color_hex(0x081A36), 0);
    lv_obj_set_style_border_color(btn_edit_prof, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_edit_prof, 1, 0);
    lv_obj_set_style_radius(btn_edit_prof, 6, 0);

    lv_obj_t * btn_edit_prof_lbl = lv_label_create(btn_edit_prof);
    lv_label_set_text(btn_edit_prof_lbl, LV_SYMBOL_EDIT "  EDIT PROFILE");
    lv_obj_set_style_text_font(btn_edit_prof_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_edit_prof_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(btn_edit_prof_lbl);

    /* Grid layout of details (Right side of card) */
    lv_obj_t * grid_cont = lv_obj_create(card_personal);
    lv_obj_set_size(grid_cont, 460, 320);
    lv_obj_set_pos(grid_cont, 225, 30);
    lv_obj_set_style_bg_opa(grid_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_cont, 0, 0);
    lv_obj_set_style_pad_all(grid_cont, 0, 0);

    create_personal_grid_card(grid_cont, 0, 0, "Patient Name", "John Doe", LV_SYMBOL_DIRECTORY, NULL);
    create_personal_grid_card(grid_cont, 1, 0, "Age", "45", LV_SYMBOL_LIST, "Years");
    create_personal_grid_card(grid_cont, 0, 1, "Weight", "70.0", LV_SYMBOL_SETTINGS, "kg");
    create_personal_grid_card(grid_cont, 1, 1, "Height", "175", LV_SYMBOL_SETTINGS, "cm");
    create_personal_grid_card(grid_cont, 0, 2, "Gender", "Male", LV_SYMBOL_IMAGE, NULL);
    
    /* Special BMI card with colored health tag */
    create_personal_grid_card(grid_cont, 1, 2, "BMI", "22.9", LV_SYMBOL_KEYBOARD, "kg/m²");
    /* Overlay BMI status text inside the card */
    lv_obj_t * bmi_stat_lbl = lv_label_create(grid_cont);
    lv_label_set_text(bmi_stat_lbl, "Normal");
    lv_obj_set_style_text_font(bmi_stat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bmi_stat_lbl, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_pos(bmi_stat_lbl, 283, 274);

    /* Panel B: NOTES */
    lv_obj_t * card_notes = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_notes, 710, 210);
    lv_obj_set_pos(card_notes, 250, 450);
    lv_obj_set_style_bg_color(card_notes, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_notes, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_notes, 1, 0);
    lv_obj_set_style_radius(card_notes, 12, 0);
    lv_obj_set_style_pad_all(card_notes, 12, 0);

    lv_obj_t * notes_title = lv_label_create(card_notes);
    lv_label_set_text(notes_title, "NOTES");
    lv_obj_set_style_text_font(notes_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(notes_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(notes_title, 10, 5);

    /* Clipboard Icon Badge */
    lv_obj_t * notes_badge = lv_obj_create(card_notes);
    lv_obj_set_size(notes_badge, 44, 44);
    lv_obj_set_pos(notes_badge, 15, 35);
    lv_obj_set_style_bg_color(notes_badge, lv_color_hex(0x061D3B), 0);
    lv_obj_set_style_border_color(notes_badge, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(notes_badge, 1, 0);
    lv_obj_set_style_radius(notes_badge, 8, 0);
    lv_obj_set_style_pad_all(notes_badge, 0, 0);

    lv_obj_t * notes_badge_icon = lv_label_create(notes_badge);
    lv_label_set_text(notes_badge_icon, LV_SYMBOL_FILE);
    lv_obj_set_style_text_font(notes_badge_icon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(notes_badge_icon, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(notes_badge_icon);

    /* Clinical notes description paragraphs */
    lv_obj_t * notes_content_lbl = lv_label_create(card_notes);
    lv_label_set_text(notes_content_lbl, 
        "• Patient is stable on current ventilator settings.\n"
        "• Monitor for signs of improvement in oxygenation.\n"
        "• Plan: Weaning assessment in next 24-48 hours.\n"
        "• Family updated.");
    lv_obj_set_style_text_font(notes_content_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(notes_content_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_set_style_pad_row(notes_content_lbl, 4, 0);
    lv_obj_set_pos(notes_content_lbl, 75, 35);

    /* Notes timestamp footer */
    lv_obj_t * notes_footer_lbl = lv_label_create(card_notes);
    lv_label_set_text(notes_footer_lbl, "Last Updated: 20 May 2024 09:45 AM");
    lv_obj_set_style_text_font(notes_footer_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(notes_footer_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(notes_footer_lbl, LV_ALIGN_BOTTOM_RIGHT, -10, -5);

    /* ==================================================================== */
    /* 4. RIGHT COLUMN SUMMARY                                              */
    /* ==================================================================== */

    /* Panel C: MEDICAL INFORMATION */
    lv_obj_t * card_medical = lv_obj_create(main_screen_obj);
    lv_obj_set_size(card_medical, 290, 595);
    lv_obj_set_pos(card_medical, 975, 65);
    lv_obj_set_style_bg_color(card_medical, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_medical, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_medical, 1, 0);
    lv_obj_set_style_radius(card_medical, 12, 0);
    lv_obj_set_style_pad_all(card_medical, 10, 0);

    lv_obj_t * medical_title = lv_label_create(card_medical);
    lv_label_set_text(medical_title, "MEDICAL INFORMATION");
    lv_obj_set_style_text_font(medical_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(medical_title, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(medical_title, 10, 5);

    /* Vertical summaries list */
    create_medical_detail_card(card_medical, 0, "Diagnosis", "Acute Respiratory Failure", LV_SYMBOL_FILE, NULL, true);
    create_medical_detail_card(card_medical, 1, "Hospital ID", "HOSP-2024-05876", LV_SYMBOL_SETTINGS, NULL, false);
    create_medical_detail_card(card_medical, 2, "Current Ventilation Mode", "VC-AC", LV_SYMBOL_IMAGE, "Active", false);
    
    /* Modify mode text subdescription in card 2 */
    lv_obj_t * mode_card_desc = lv_label_create(card_medical);
    lv_label_set_text(mode_card_desc, "Volume Control");
    lv_obj_set_style_text_font(mode_card_desc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_card_desc, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(mode_card_desc, 68, 280);

    create_medical_detail_card(card_medical, 3, "Admission Date", "19 May 2024 \n08:15 AM", LV_SYMBOL_LIST, NULL, false);
    create_medical_detail_card(card_medical, 4, "Attending Physician", "Dr. Sarah Johnson", LV_SYMBOL_DIRECTORY, NULL, true);

    /* ==================================================================== */
    /* 5. BOTTOM FOOTER NAVIGATION                                          */
    /* ==================================================================== */
    lv_obj_t * footer_cont = lv_obj_create(main_screen_obj);
    lv_obj_set_size(footer_cont, 1280, 100);
    lv_obj_set_pos(footer_cont, 0, 690);
    lv_obj_set_style_bg_opa(footer_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer_cont, 0, 0);
    lv_obj_set_style_pad_all(footer_cont, 0, 0);

    /* Footer Button 1: EDIT PATIENT INFORMATION */
    lv_obj_t * btn_edit_pat = lv_button_create(footer_cont);
    lv_obj_set_size(btn_edit_pat, 240, 48);
    lv_obj_set_pos(btn_edit_pat, 15, 10);
    lv_obj_set_style_bg_color(btn_edit_pat, COLOR_BTN_NAV_ACTIVE, 0);
    lv_obj_set_style_border_color(btn_edit_pat, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(btn_edit_pat, 1, 0);
    lv_obj_set_style_radius(btn_edit_pat, 8, 0);

    lv_obj_t * btn_edit_pat_lbl = lv_label_create(btn_edit_pat);
    lv_label_set_text(btn_edit_pat_lbl, LV_SYMBOL_EDIT "  EDIT PATIENT INFORMATION");
    lv_obj_set_style_text_font(btn_edit_pat_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_edit_pat_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(btn_edit_pat_lbl);

    /* Footer Button 2: VIEW MEDICAL HISTORY */
    lv_obj_t * btn_history = lv_button_create(footer_cont);
    lv_obj_set_size(btn_history, 240, 48);
    lv_obj_set_pos(btn_history, 270, 10);
    lv_obj_set_style_bg_color(btn_history, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_history, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_history, 1, 0);
    lv_obj_set_style_radius(btn_history, 8, 0);

    lv_obj_t * btn_history_lbl = lv_label_create(btn_history);
    lv_label_set_text(btn_history_lbl, LV_SYMBOL_FILE "  VIEW MEDICAL HISTORY");
    lv_obj_set_style_text_font(btn_history_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_history_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(btn_history_lbl);

    /* Footer Button 3: EXPORT PROFILE */
    lv_obj_t * btn_export = lv_button_create(footer_cont);
    lv_obj_set_size(btn_export, 210, 48);
    lv_obj_set_pos(btn_export, 525, 10);
    lv_obj_set_style_bg_color(btn_export, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_export, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_export, 1, 0);
    lv_obj_set_style_radius(btn_export, 8, 0);

    lv_obj_t * btn_export_lbl = lv_label_create(btn_export);
    lv_label_set_text(btn_export_lbl, LV_SYMBOL_DOWNLOAD "  EXPORT PROFILE");
    lv_obj_set_style_text_font(btn_export_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_export_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(btn_export_lbl);

    /* Footer Button 4: BACK TO DASHBOARD (Blue highlight) */
    lv_obj_t * btn_home = lv_button_create(footer_cont);
    lv_obj_set_size(btn_home, 290, 48);
    lv_obj_set_pos(btn_home, 975, 10);
    lv_obj_set_style_bg_color(btn_home, COLOR_BTN_NAV_ACTIVE, 0);
    lv_obj_set_style_border_color(btn_home, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(btn_home, 1, 0);
    lv_obj_set_style_radius(btn_home, 8, 0);
    lv_obj_add_event_cb(btn_home, back_to_home_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_home_lbl = lv_label_create(btn_home);
    lv_label_set_text(btn_home_lbl, LV_SYMBOL_HOME "  BACK TO DASHBOARD");
    lv_obj_set_style_text_font(btn_home_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btn_home_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(btn_home_lbl);

    /* Disable scrolling tree filter */
    disable_scroll_recursive(main_screen_obj);

    /* Dynamic clock sync timer */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL); /* Run instantly to populate clocks */

    /* Load Patient Profile screen */
    lv_screen_load_anim(main_screen_obj, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
