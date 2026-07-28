#include "ventilator_settings_screen.h"
#include "ventilator_main_screen.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

/* Static references */
static lv_timer_t * clock_timer = NULL;
static lv_obj_t * lbl_clock = NULL;

/* Clock update callback */
static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(lbl_clock) {
        time_t raw_time;
        struct tm * time_info;
        time(&raw_time);
        time_info = localtime(&raw_time);
        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%d %b %Y\n%I:%M %p", time_info);
        lv_label_set_text(lbl_clock, clock_buf);
    }
}

/* Home button transition */
static void home_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    create_ventilator_main_screen();
}

/* Dummy setting card click event */
static void setting_card_cb(lv_event_t * e)
{
    const char * title = (const char *)lv_event_get_user_data(e);
    LV_LOG_USER("Clicked system setting: %s\n", title);
    if(strcmp(title, "Diagnosis") == 0) {
        if(clock_timer) {
            lv_timer_delete(clock_timer);
            clock_timer = NULL;
        }
        extern void create_ventilator_diagnostics_screen(void);
        create_ventilator_diagnostics_screen();
    }
    else if(strcmp(title, "Calibration") == 0) {
        if(clock_timer) {
            lv_timer_delete(clock_timer);
            clock_timer = NULL;
        }
        extern void create_ventilator_calibration_screen(void);
        create_ventilator_calibration_screen();
    }
}

/* Recursive helper to disable scrolling on all objects */
extern void disable_scroll_recursive(lv_obj_t * obj);

/* Helper to construct each settings card */
static lv_obj_t * create_setting_card(lv_obj_t * parent, int col, int row, const char * title, const char * desc, int icon_type)
{
    int x = (col == 0) ? 20 : 655;
    int y = 80 + row * 131;

    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, 605, 116);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 0, 0);

    /* Responsive pressed state feedback */
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(card, COLOR_BTN_NAV_ACTIVE, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(card, COLOR_ACCENT_BLUE, LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, setting_card_cb, LV_EVENT_CLICKED, (void*)title);

    /* Circular Icon Badge Container */
    lv_obj_t * badge = lv_obj_create(card);
    lv_obj_set_size(badge, 64, 64);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x061D3B), 0);
    lv_obj_set_style_border_color(badge, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(badge, 2, 0);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);

    /* Render custom icons depending on type */
    if(icon_type == 0) {
        /* Display Icon - monitor screen */
        lv_obj_t * monitor = lv_obj_create(badge);
        lv_obj_set_size(monitor, 32, 22);
        lv_obj_align(monitor, LV_ALIGN_CENTER, 0, -4);
        lv_obj_set_style_bg_opa(monitor, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(monitor, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(monitor, 2, 0);
        lv_obj_set_style_radius(monitor, 3, 0);

        lv_obj_t * stand = lv_obj_create(badge);
        lv_obj_set_size(stand, 8, 6);
        lv_obj_align(stand, LV_ALIGN_CENTER, 0, 8);
        lv_obj_set_style_bg_color(stand, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(stand, 0, 0);

        lv_obj_t * base = lv_obj_create(badge);
        lv_obj_set_size(base, 18, 2);
        lv_obj_align(base, LV_ALIGN_CENTER, 0, 12);
        lv_obj_set_style_bg_color(base, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(base, 0, 0);
    } 
    else if(icon_type == 1) {
        /* Brightness Icon - Sun center with 8 rays */
        lv_obj_t * sun_center = lv_obj_create(badge);
        lv_obj_set_size(sun_center, 18, 18);
        lv_obj_align(sun_center, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(sun_center, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(sun_center, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(sun_center, 2, 0);
        lv_obj_set_style_radius(sun_center, LV_RADIUS_CIRCLE, 0);

        int rays[8][2] = {
            {0, -13}, {9, -9}, {13, 0}, {9, 9},
            {0, 13}, {-9, 9}, {-13, 0}, {-9, -9}
        };
        for(int i = 0; i < 8; i++) {
            lv_obj_t * ray = lv_obj_create(badge);
            lv_obj_set_size(ray, 3, 3);
            lv_obj_align(ray, LV_ALIGN_CENTER, rays[i][0], rays[i][1]);
            lv_obj_set_style_bg_color(ray, COLOR_ACCENT_BLUE, 0);
            lv_obj_set_style_border_width(ray, 0, 0);
            lv_obj_set_style_radius(ray, LV_RADIUS_CIRCLE, 0);
        }
    } 
    else if(icon_type == 2) {
        /* Language Icon - Translation glyph "A/文" */
        lv_obj_t * lang_lbl = lv_label_create(badge);
        lv_label_set_text(lang_lbl, "A/文");
        lv_obj_set_style_text_font(lang_lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lang_lbl, COLOR_ACCENT_BLUE, 0);
        lv_obj_center(lang_lbl);
    } 
    else if(icon_type == 3) {
        /* Date & Time Icon - Calendar sheet */
        lv_obj_t * cal = lv_obj_create(badge);
        lv_obj_set_size(cal, 26, 26);
        lv_obj_align(cal, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(cal, COLOR_CARD_BG, 0);
        lv_obj_set_style_border_color(cal, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(cal, 2, 0);
        lv_obj_set_style_radius(cal, 4, 0);
        lv_obj_set_style_pad_all(cal, 0, 0);

        lv_obj_t * cal_hdr = lv_obj_create(cal);
        lv_obj_set_size(cal_hdr, 26, 6);
        lv_obj_align(cal_hdr, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(cal_hdr, COLOR_ACCENT_RED, 0);
        lv_obj_set_style_border_width(cal_hdr, 0, 0);

        for(int r = 0; r < 2; r++) {
            for(int c_idx = 0; c_idx < 3; c_idx++) {
                lv_obj_t * dot = lv_obj_create(cal);
                lv_obj_set_size(dot, 2, 2);
                lv_obj_set_pos(dot, 4 + c_idx * 6, 10 + r * 6);
                lv_obj_set_style_bg_color(dot, COLOR_TEXT_MUTED, 0);
                lv_obj_set_style_border_width(dot, 0, 0);
            }
        }
    } 
    else if(icon_type == 4) {
        /* Sound Icon - Volume symbol */
        lv_obj_t * sound_lbl = lv_label_create(badge);
        lv_label_set_text(sound_lbl, LV_SYMBOL_VOLUME_MAX);
        lv_obj_set_style_text_font(sound_lbl, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(sound_lbl, COLOR_ACCENT_BLUE, 0);
        lv_obj_center(sound_lbl);
    } 
    else if(icon_type == 5) {
        /* Network Icon - Wifi symbol */
        lv_obj_t * wifi_lbl = lv_label_create(badge);
        lv_label_set_text(wifi_lbl, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(wifi_lbl, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(wifi_lbl, COLOR_ACCENT_BLUE, 0);
        lv_obj_center(wifi_lbl);
    } 
    else if(icon_type == 6) {
        /* Calibration Icon - Target crosshair */
        lv_obj_t * circle = lv_obj_create(badge);
        lv_obj_set_size(circle, 24, 24);
        lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(circle, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(circle, 2, 0);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);

        lv_obj_t * h_line = lv_obj_create(badge);
        lv_obj_set_size(h_line, 32, 2);
        lv_obj_align(h_line, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(h_line, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(h_line, 0, 0);

        lv_obj_t * v_line = lv_obj_create(badge);
        lv_obj_set_size(v_line, 2, 32);
        lv_obj_align(v_line, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(v_line, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(v_line, 0, 0);
    } 
    else if(icon_type == 7) {
        /* Maintenance Icon - Gear symbol */
        lv_obj_t * gear_lbl = lv_label_create(badge);
        lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_font(gear_lbl, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(gear_lbl, COLOR_ACCENT_BLUE, 0);
        lv_obj_center(gear_lbl);
    } 
    else if(icon_type == 8) {
        /* Software Update Icon - Download symbol */
        lv_obj_t * down_lbl = lv_label_create(badge);
        lv_label_set_text(down_lbl, LV_SYMBOL_DOWNLOAD);
        lv_obj_set_style_text_font(down_lbl, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(down_lbl, COLOR_ACCENT_BLUE, 0);
        lv_obj_center(down_lbl);
    } 
    else if(icon_type == 9) {
        /* About Device Icon - 'i' letter in circle */
        lv_obj_t * circle = lv_obj_create(badge);
        lv_obj_set_size(circle, 24, 24);
        lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(circle, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_border_width(circle, 2, 0);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);

        lv_obj_t * info_lbl = lv_label_create(badge);
        lv_label_set_text(info_lbl, "i");
        lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(info_lbl, COLOR_ACCENT_BLUE, 0);
        lv_obj_center(info_lbl);
    }

    /* Text Stack for Title & Description */
    lv_obj_t * text_cont = lv_obj_create(card);
    lv_obj_set_size(text_cont, 450, 76);
    lv_obj_align(text_cont, LV_ALIGN_LEFT_MID, 100, 0);
    lv_obj_set_style_bg_opa(text_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_cont, 0, 0);
    lv_obj_set_style_pad_all(text_cont, 0, 0);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t * t_lbl = lv_label_create(text_cont);
    lv_label_set_text(t_lbl, title);
    lv_obj_set_style_text_font(t_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t_lbl, COLOR_TEXT_MAIN, 0);

    lv_obj_t * d_lbl = lv_label_create(text_cont);
    lv_label_set_text(d_lbl, desc);
    lv_obj_set_style_text_font(d_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(d_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_pad_top(d_lbl, 4, 0);

    /* Right Chevron Arrow */
    lv_obj_t * chevron = lv_label_create(card);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(chevron, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chevron, COLOR_TEXT_MUTED, 0);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -20, 0);

    return card;
}

/**
 * @brief Initialize and display System Settings Screen (1280x800)
 */
void create_ventilator_settings_screen(void)
{
    /* Clean up old clock timer to prevent leak */
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }

    /* Create the base screen */
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

    /* SYSTEM SETTINGS Badge (Left) */
    lv_obj_t * mode_box = lv_obj_create(top_bar);
    lv_obj_set_size(mode_box, 250, 42);
    lv_obj_align(mode_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(mode_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(mode_box, 0, 0);
    lv_obj_set_style_radius(mode_box, 6, 0);
    lv_obj_set_style_pad_all(mode_box, 4, 0);

    /* Lungs Icon Box */
    lv_obj_t * lung_icon_box = lv_obj_create(mode_box);
    lv_obj_set_size(lung_icon_box, 34, 34);
    lv_obj_align(lung_icon_box, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(lung_icon_box, lv_color_hex(0x007CFF), 0);
    lv_obj_set_style_radius(lung_icon_box, 6, 0);
    lv_obj_set_style_border_width(lung_icon_box, 0, 0);

    lv_obj_t * lung_lbl = lv_label_create(lung_icon_box);
    /* Unicode representation/character of lungs drawing, falling back to clean display symbol */
    lv_label_set_text(lung_lbl, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_font(lung_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lung_lbl, lv_color_white(), 0);
    lv_obj_center(lung_lbl);

    lv_obj_t * mode_title = lv_label_create(mode_box);
    lv_label_set_text(mode_title, "SYSTEM SETTINGS");
    lv_obj_set_style_text_font(mode_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mode_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(mode_title, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * mode_sub = lv_label_create(mode_box);
    lv_label_set_text(mode_sub, "Configure system preferences");
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

    lv_obj_t * set_icon = lv_label_create(right_hdr);
    lv_label_set_text(set_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(set_icon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(set_icon, COLOR_TEXT_MAIN, 0); // highlighted/active settings screen
    lv_obj_align(set_icon, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ==================================================================== */
    /* 2. GRID LAYOUT - 10 SETTING CARDS (2x5 Grid)                         */
    /* ==================================================================== */
    create_setting_card(scr, 0, 0, "Display", "Configure screen theme, layout, and display preferences.", 0);
    create_setting_card(scr, 1, 0, "Brightness", "Adjust screen brightness and sleep settings.", 1);
    create_setting_card(scr, 0, 1, "Language", "Select system language and regional preferences.", 2);
    create_setting_card(scr, 1, 1, "Date & Time", "Set date, time, format and time zone.", 3);
    create_setting_card(scr, 0, 2, "Sound", "Configure alarm volume, key tones and notifications.", 4);
    create_setting_card(scr, 1, 2, "Network", "Manage Wi-Fi, LAN, IP settings and connectivity.", 5);
    create_setting_card(scr, 0, 3, "Calibration", "Calibrate sensors, touch screen and flow/pressure modules.", 6);
    create_setting_card(scr, 1, 3, "Diagnosis", "Monitor system health, view communication status, and run system diagnostics.", 7);
    create_setting_card(scr, 0, 4, "Software Update", "Check for updates and install the latest software.", 8);
    create_setting_card(scr, 1, 4, "About Device", "View device information, model, serial number and legal details.", 9);

    /* ==================================================================== */
    /* 3. BOTTOM FOOTER NAVIGATION BAR                                      */
    /* ==================================================================== */
    lv_obj_t * bot_bar = lv_obj_create(scr);
    lv_obj_set_size(bot_bar, 1260, 55);
    lv_obj_set_pos(bot_bar, 10, 740);
    lv_obj_set_style_bg_opa(bot_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bot_bar, 0, 0);
    lv_obj_set_style_pad_all(bot_bar, 0, 0);

    /* HOME Button */
    lv_obj_t * home_btn = lv_button_create(bot_bar);
    lv_obj_set_size(home_btn, 140, 48);
    lv_obj_align(home_btn, LV_ALIGN_LEFT_MID, 0, 0);
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

    /* Vertical line 1 */
    lv_obj_t * line1 = lv_obj_create(bot_bar);
    lv_obj_set_size(line1, 1, 30);
    lv_obj_set_pos(line1, 160, 12);
    lv_obj_set_style_bg_color(line1, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(line1, 0, 0);

    /* Secure Settings Status Indicator */
    lv_obj_t * secure_cont = lv_obj_create(bot_bar);
    lv_obj_set_size(secure_cont, 220, 40);
    lv_obj_set_pos(secure_cont, 180, 7);
    lv_obj_set_style_bg_opa(secure_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(secure_cont, 0, 0);
    lv_obj_set_style_pad_all(secure_cont, 0, 0);

    lv_obj_t * secure_icon = lv_label_create(secure_cont);
    lv_label_set_text(secure_icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(secure_icon, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(secure_icon, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * secure_lbl = lv_label_create(secure_cont);
    lv_label_set_text(secure_lbl, "Secure Settings");
    lv_obj_set_style_text_font(secure_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(secure_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(secure_lbl, LV_ALIGN_LEFT_MID, 30, 0);

    /* Vertical line 2 */
    lv_obj_t * line2 = lv_obj_create(bot_bar);
    lv_obj_set_size(line2, 1, 30);
    lv_obj_set_pos(line2, 410, 12);
    lv_obj_set_style_bg_color(line2, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(line2, 0, 0);

    /* Change Authorization Status */
    lv_obj_t * auth_cont = lv_obj_create(bot_bar);
    lv_obj_set_size(auth_cont, 250, 40);
    lv_obj_set_pos(auth_cont, 430, 7);
    lv_obj_set_style_bg_opa(auth_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(auth_cont, 0, 0);
    lv_obj_set_style_pad_all(auth_cont, 0, 0);

    lv_obj_t * auth_icon = lv_label_create(auth_cont);
    lv_label_set_text(auth_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(auth_icon, COLOR_TEXT_MUTED, 0);
    lv_obj_align(auth_icon, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * auth_lbl = lv_label_create(auth_cont);
    lv_label_set_text(auth_lbl, "Change Authorization");
    lv_obj_set_style_text_font(auth_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(auth_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(auth_lbl, LV_ALIGN_LEFT_MID, 30, 0);

    /* Disable scrolling tree filter */
    disable_scroll_recursive(scr);

    /* Clock dynamic sync timer */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL); // Run immediately to populate clock

    /* Load Settings Screen */
    lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
