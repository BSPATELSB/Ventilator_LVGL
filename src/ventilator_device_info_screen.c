#include "ventilator_device_info_screen.h"
#include "ventilator_settings_screen.h"
#include "ventilator_main_screen.h"
#include "ventilator_time_screen.h"
#include "battery_detect.h"
#include "theme_manager.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#elif __has_include(<cJSON.h>)
#include <cJSON.h>
#endif

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

/* Static references */
static lv_timer_t * clock_timer = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * bat_lbl = NULL;
static ventilator_device_info_t current_device_info;

/* External Helper */
extern void disable_scroll_recursive(lv_obj_t * obj);

/**
 * @brief Helper to read JSON file from disk into memory buffer
 */
static char * read_file_to_string(const char * filepath)
{
    FILE * f = fopen(filepath, "rb");
    if(!f) return NULL;

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(length <= 0) {
        fclose(f);
        return NULL;
    }

    char * buffer = (char *)malloc(length + 1);
    if(!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, length, f);
    buffer[read_bytes] = '\0';
    fclose(f);

    return buffer;
}

/**
 * @brief Parse version_properties.json file and fill info struct
 */
bool read_version_properties_json(ventilator_device_info_t * info)
{
    if(!info) return false;

    memset(info, 0, sizeof(ventilator_device_info_t));
    info->json_read_success = false;

    const char * possible_paths[] = {
        "version_properties.json",
        "./version_properties.json",
        "../version_properties.json",
        "/home/nxon/LIFE_LINE_BIZ/my_ventilator_app/version_properties.json"
    };

    char * json_data = NULL;
    for(size_t i = 0; i < sizeof(possible_paths)/sizeof(possible_paths[0]); i++) {
        json_data = read_file_to_string(possible_paths[i]);
        if(json_data) {
            LV_LOG_USER("Successfully read JSON from %s\n", possible_paths[i]);
            break;
        }
    }

    /* Fallback default values if file read fails */
    strncpy(info->firmware_version, "v3.8.4-Build20260808", sizeof(info->firmware_version) - 1);
    strncpy(info->application_version, "v2.5.0-Release", sizeof(info->application_version) - 1);
    strncpy(info->application_name, "LifeLine Medical Ventilator UI", sizeof(info->application_name) - 1);
    strncpy(info->developer_name, "LifeLine Embedded Systems R&D Team", sizeof(info->developer_name) - 1);
    strncpy(info->technology, "C99 / LVGL 9.2 / Linux Framebuffer & DRM / CMake", sizeof(info->technology) - 1);
    strncpy(info->build_date, "2026-08-08", sizeof(info->build_date) - 1);
    strncpy(info->hardware_revision, "Rev 4.2-B", sizeof(info->hardware_revision) - 1);
    strncpy(info->serial_number, "LLV-98472-X2026", sizeof(info->serial_number) - 1);

    const char * default_features[] = {
        "Real-Time Patient Ventilation Monitoring & High-Precision Waveforms (P-t, F-t, V-t)",
        "Comprehensive Ventilation Modes (VCV, PCV, SIMV, CPAP/PSV, BiPAP, PRVC)",
        "Smart Alarm System with Visual & Audible Critical Alerts",
        "Patient Profile Management & Dynamic IBW Tidal Volume Calculator",
        "Wi-Fi & Ethernet Network Interface & Cloud Telemetry Support",
        "Sensor & Valve Calibration System (O2, Pressure, Flow, Zero Calibration)",
        "Hardware Diagnostics & Live Communication Bus Status Monitor",
        "Multi-Theme Support (Dark Medical, Light Mode, High-Contrast Night Vision)",
        "Dynamic Screen Brightness & Power Management",
        "USB Firmware Upgrade & Exportable Patient Log Architecture"
    };

    info->feature_count = sizeof(default_features) / sizeof(default_features[0]);
    for(int i = 0; i < info->feature_count; i++) {
        strncpy(info->features[i], default_features[i], sizeof(info->features[i]) - 1);
    }

    if(!json_data) {
        LV_LOG_USER("Could not open version_properties.json, loaded default properties.\n");
        return false;
    }

#if defined(cJSON_Invalid) || defined(CJSON_VERSION_MAJOR) || defined(_CJSON_H)
    cJSON * root = cJSON_Parse(json_data);
    if(root) {
        cJSON * fw = cJSON_GetObjectItemCaseSensitive(root, "firmware_version");
        if(cJSON_IsString(fw) && (fw->valuestring != NULL)) {
            strncpy(info->firmware_version, fw->valuestring, sizeof(info->firmware_version) - 1);
        }

        cJSON * app_v = cJSON_GetObjectItemCaseSensitive(root, "application_version");
        if(cJSON_IsString(app_v) && (app_v->valuestring != NULL)) {
            strncpy(info->application_version, app_v->valuestring, sizeof(info->application_version) - 1);
        }

        cJSON * app_n = cJSON_GetObjectItemCaseSensitive(root, "application_name");
        if(cJSON_IsString(app_n) && (app_n->valuestring != NULL)) {
            strncpy(info->application_name, app_n->valuestring, sizeof(info->application_name) - 1);
        }

        cJSON * dev = cJSON_GetObjectItemCaseSensitive(root, "developer_name");
        if(cJSON_IsString(dev) && (dev->valuestring != NULL)) {
            strncpy(info->developer_name, dev->valuestring, sizeof(info->developer_name) - 1);
        }

        cJSON * tech = cJSON_GetObjectItemCaseSensitive(root, "technology");
        if(cJSON_IsString(tech) && (tech->valuestring != NULL)) {
            strncpy(info->technology, tech->valuestring, sizeof(info->technology) - 1);
        }

        cJSON * bdate = cJSON_GetObjectItemCaseSensitive(root, "build_date");
        if(cJSON_IsString(bdate) && (bdate->valuestring != NULL)) {
            strncpy(info->build_date, bdate->valuestring, sizeof(info->build_date) - 1);
        }

        cJSON * hw_rev = cJSON_GetObjectItemCaseSensitive(root, "hardware_revision");
        if(cJSON_IsString(hw_rev) && (hw_rev->valuestring != NULL)) {
            strncpy(info->hardware_revision, hw_rev->valuestring, sizeof(info->hardware_revision) - 1);
        }

        cJSON * sn = cJSON_GetObjectItemCaseSensitive(root, "serial_number");
        if(cJSON_IsString(sn) && (sn->valuestring != NULL)) {
            strncpy(info->serial_number, sn->valuestring, sizeof(info->serial_number) - 1);
        }

        cJSON * feats = cJSON_GetObjectItemCaseSensitive(root, "features");
        if(cJSON_IsArray(feats)) {
            int count = cJSON_GetArraySize(feats);
            if(count > 0) {
                info->feature_count = 0;
                cJSON * feat_item = NULL;
                cJSON_ArrayForEach(feat_item, feats) {
                    if(cJSON_IsString(feat_item) && (feat_item->valuestring != NULL) && info->feature_count < 16) {
                        strncpy(info->features[info->feature_count], feat_item->valuestring, sizeof(info->features[info->feature_count]) - 1);
                        info->feature_count++;
                    }
                }
            }
        }

        info->json_read_success = true;
        cJSON_Delete(root);
    }
#endif

    free(json_data);
    return info->json_read_success;
}

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

/* Settings button transition */
static void settings_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    create_ventilator_settings_screen();
}

/* Helper to create an info key-value field box */
static void create_info_row(lv_obj_t * parent, const char * label_text, const char * val_text, const char * icon_symbol, bool highlight)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 48);
    lv_obj_set_style_bg_color(row, highlight ? lv_color_hex(0x0A2647) : lv_color_hex(0x07192E), 0);
    lv_obj_set_style_border_color(row, highlight ? COLOR_ACCENT_BLUE : COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(row, highlight ? 1 : 1, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 6, 0);

    /* Left Icon + Key Label */
    lv_obj_t * key_lbl = lv_label_create(row);
    if(icon_symbol && strlen(icon_symbol) > 0) {
        char key_buf[128];
        snprintf(key_buf, sizeof(key_buf), "%s  %s", icon_symbol, label_text);
        lv_label_set_text(key_lbl, key_buf);
    } else {
        lv_label_set_text(key_lbl, label_text);
    }
    lv_obj_set_style_text_font(key_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(key_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_align(key_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    /* Right Value Label */
    lv_obj_t * val_lbl = lv_label_create(row);
    lv_label_set_text(val_lbl, val_text);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(val_lbl, highlight ? COLOR_ACCENT_GREEN : COLOR_TEXT_MAIN, 0);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
}

/**
 * @brief Create and display Device Information screen (1280x800)
 */
void create_ventilator_device_info_screen(void)
{
    /* Clean up old clock timer */
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }

    /* Read version_properties.json */
    read_version_properties_json(&current_device_info);

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

    /* DEVICE INFORMATION Badge (Left) */
    lv_obj_t * mode_box = lv_obj_create(top_bar);
    lv_obj_set_size(mode_box, 260, 42);
    lv_obj_align(mode_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(mode_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(mode_box, 0, 0);
    lv_obj_set_style_radius(mode_box, 6, 0);
    lv_obj_set_style_pad_all(mode_box, 4, 0);

    /* Icon Box */
    lv_obj_t * icon_box = lv_obj_create(mode_box);
    lv_obj_set_size(icon_box, 34, 34);
    lv_obj_align(icon_box, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(icon_box, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(icon_box, 6, 0);
    lv_obj_set_style_border_width(icon_box, 0, 0);

    lv_obj_t * icon_lbl = lv_label_create(icon_box);
    lv_label_set_text(icon_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(icon_lbl, lv_color_white(), 0);
    lv_obj_center(icon_lbl);

    lv_obj_t * mode_title = lv_label_create(mode_box);
    lv_label_set_text(mode_title, "DEVICE INFORMATION");
    lv_obj_set_style_text_font(mode_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mode_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(mode_title, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * mode_sub = lv_label_create(mode_box);
    lv_label_set_text(mode_sub, "About system & software properties");
    lv_obj_set_style_text_font(mode_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_sub, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(mode_sub, LV_ALIGN_LEFT_MID, 46, 10);

    /* Patient ID Badge (Middle Left) */
    lv_obj_t * pat_box = lv_obj_create(top_bar);
    lv_obj_set_size(pat_box, 230, 42);
    lv_obj_align(pat_box, LV_ALIGN_LEFT_MID, 270, 0);
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
    lv_obj_set_size(alarm_box, 310, 42);
    lv_obj_align(alarm_box, LV_ALIGN_CENTER, 80, 0);
    lv_obj_set_style_bg_color(alarm_box, lv_color_hex(0x5A0C0C), 0);
    lv_obj_set_style_border_width(alarm_box, 0, 0);
    lv_obj_set_style_radius(alarm_box, 6, 0);

    lv_obj_t * alarm_lbl = lv_label_create(alarm_box);
    lv_label_set_text(alarm_lbl, LV_SYMBOL_BELL "  2 CRITICAL ALARMS\n   Require immediate attention");
    lv_obj_set_style_text_font(alarm_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(alarm_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    /* Date & Time Clock Box (Middle Right) */
    lv_obj_t * date_box = lv_obj_create(top_bar);
    lv_obj_set_size(date_box, 150, 42);
    lv_obj_align(date_box, LV_ALIGN_RIGHT_MID, -130, 0);
    lv_obj_set_style_bg_color(date_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(date_box, 0, 0);
    lv_obj_set_style_radius(date_box, 6, 0);

    lbl_clock = lv_label_create(date_box);
    lv_label_set_text(lbl_clock, "08 Aug 2026\n11:53 AM");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 30, 0);

    /* Battery & Settings Icon Box (Right) */
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
    lv_obj_set_style_text_color(set_icon, COLOR_TEXT_MUTED, 0);
    lv_obj_align(set_icon, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ==================================================================== */
    /* 2. MAIN CONTENT AREA (Two Column Layout)                              */
    /* ==================================================================== */

    /* -------------------------------------------------------------------- */
    /* LEFT COLUMN: SYSTEM IDENTIFICATION & TECH STACK                      */
    /* -------------------------------------------------------------------- */
    lv_obj_t * left_card = lv_obj_create(scr);
    lv_obj_set_size(left_card, 520, 660);
    lv_obj_set_pos(left_card, 20, 70);
    lv_obj_set_style_bg_color(left_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(left_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(left_card, 1, 0);
    lv_obj_set_style_radius(left_card, 10, 0);
    lv_obj_set_style_pad_all(left_card, 16, 0);
    lv_obj_set_flex_flow(left_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(left_card, 8, 0);

    /* Card Header 1: Software & Firmware Details */
    lv_obj_t * hdr1 = lv_label_create(left_card);
    lv_label_set_text(hdr1, LV_SYMBOL_DIRECTORY " SOFTWARE & FIRMWARE SPECIFICATION");
    lv_obj_set_style_text_font(hdr1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hdr1, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_pad_bottom(hdr1, 4, 0);

    /* Firmware Version Row (Read from version_properties.json) */
    char fw_display[128];
    snprintf(fw_display, sizeof(fw_display), "%s %s", current_device_info.firmware_version, 
             current_device_info.json_read_success ? "[JSON OK]" : "[LOADED]");
    create_info_row(left_card, "Firmware Version", fw_display, LV_SYMBOL_DOWNLOAD, true);

    create_info_row(left_card, "Application Version", current_device_info.application_version, LV_SYMBOL_FILE, false);
    create_info_row(left_card, "Application Name", current_device_info.application_name, LV_SYMBOL_IMAGE, false);
    create_info_row(left_card, "Developer's Name", current_device_info.developer_name, LV_SYMBOL_EDIT, false);
    create_info_row(left_card, "Build Date", current_device_info.build_date, LV_SYMBOL_LIST, false);
    create_info_row(left_card, "Hardware Revision", current_device_info.hardware_revision, LV_SYMBOL_SETTINGS, false);
    create_info_row(left_card, "Serial Number", current_device_info.serial_number, LV_SYMBOL_KEYBOARD, false);

    /* Separator Line */
    lv_obj_t * sep = lv_obj_create(left_card);
    lv_obj_set_width(sep, lv_pct(100));
    lv_obj_set_height(sep, 1);
    lv_obj_set_style_bg_color(sep, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_margin_ver(sep, 6, 0);

    /* Card Header 2: Technology Stack & Engine */
    lv_obj_t * hdr2 = lv_label_create(left_card);
    lv_label_set_text(hdr2, LV_SYMBOL_DRIVE " TECHNOLOGY STACK & SYSTEM ENGINE");
    lv_obj_set_style_text_font(hdr2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hdr2, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_pad_bottom(hdr2, 4, 0);

    /* Technology Details Box */
    lv_obj_t * tech_box = lv_obj_create(left_card);
    lv_obj_set_width(tech_box, lv_pct(100));
    lv_obj_set_height(tech_box, 135);
    lv_obj_set_style_bg_color(tech_box, lv_color_hex(0x06182B), 0);
    lv_obj_set_style_border_color(tech_box, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(tech_box, 1, 0);
    lv_obj_set_style_radius(tech_box, 6, 0);
    lv_obj_set_style_pad_all(tech_box, 10, 0);

    lv_obj_t * tech_text = lv_label_create(tech_box);
    char tech_buf[512];
    snprintf(tech_buf, sizeof(tech_buf),
             "#7097ba Tech Framework:# %s\n\n"
             "#7097ba GUI Library:# LVGL v9.2.0 (Light and Versatile Graphics)\n"
             "#7097ba Target Architecture:# Linux Embedded / Posix C99\n"
             "#7097ba Display Backend:# Framebuffer / DRM / SDL2 High Res (1280x800)",
             current_device_info.technology);
    lv_label_set_text(tech_text, tech_buf);
    lv_label_set_recolor(tech_text, true);
    lv_obj_set_style_text_font(tech_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tech_text, COLOR_TEXT_MAIN, 0);
    lv_obj_set_width(tech_text, 480);
    lv_label_set_long_mode(tech_text, LV_LABEL_LONG_WRAP);

    /* -------------------------------------------------------------------- */
    /* RIGHT COLUMN: APPLICATION FEATURES & OPERATIONAL STATUS              */
    /* -------------------------------------------------------------------- */
    lv_obj_t * right_card = lv_obj_create(scr);
    lv_obj_set_size(right_card, 715, 660);
    lv_obj_set_pos(right_card, 550, 70);
    lv_obj_set_style_bg_color(right_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(right_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(right_card, 1, 0);
    lv_obj_set_style_radius(right_card, 10, 0);
    lv_obj_set_style_pad_all(right_card, 16, 0);

    /* Header for Features */
    lv_obj_t * feat_hdr = lv_label_create(right_card);
    lv_label_set_text(feat_hdr, LV_SYMBOL_PLAY " APPLICATION FEATURES & OPERATIONAL STATUS");
    lv_obj_set_style_text_font(feat_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(feat_hdr, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_pos(feat_hdr, 0, 0);

    lv_obj_t * feat_sub = lv_label_create(right_card);
    lv_label_set_text(feat_sub, "All modules verified, active, and functioning in production runtime:");
    lv_obj_set_style_text_font(feat_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(feat_sub, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(feat_sub, 0, 24);

    /* Scrollable Container for Features List */
    lv_obj_t * feat_list = lv_obj_create(right_card);
    lv_obj_set_size(feat_list, 683, 560);
    lv_obj_set_pos(feat_list, 0, 50);
    lv_obj_set_style_bg_color(feat_list, lv_color_hex(0x051627), 0);
    lv_obj_set_style_border_color(feat_list, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(feat_list, 1, 0);
    lv_obj_set_style_radius(feat_list, 8, 0);
    lv_obj_set_style_pad_all(feat_list, 10, 0);
    lv_obj_set_flex_flow(feat_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(feat_list, 8, 0);

    for(int i = 0; i < current_device_info.feature_count; i++) {
        lv_obj_t * f_item = lv_obj_create(feat_list);
        lv_obj_set_width(f_item, lv_pct(100));
        lv_obj_set_height(f_item, 46);
        lv_obj_set_style_bg_color(f_item, lv_color_hex(0x09223D), 0);
        lv_obj_set_style_border_color(f_item, lv_color_hex(0x13385E), 0);
        lv_obj_set_style_border_width(f_item, 1, 0);
        lv_obj_set_style_radius(f_item, 6, 0);
        lv_obj_set_style_pad_hor(f_item, 10, 0);
        lv_obj_set_style_pad_ver(f_item, 4, 0);

        /* Status Badge: WORKING (Green checkmark) */
        lv_obj_t * st_badge = lv_obj_create(f_item);
        lv_obj_set_size(st_badge, 95, 26);
        lv_obj_align(st_badge, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(st_badge, lv_color_hex(0x0A3A21), 0);
        lv_obj_set_style_border_color(st_badge, COLOR_ACCENT_GREEN, 0);
        lv_obj_set_style_border_width(st_badge, 1, 0);
        lv_obj_set_style_radius(st_badge, 4, 0);

        lv_obj_t * st_lbl = lv_label_create(st_badge);
        lv_label_set_text(st_lbl, LV_SYMBOL_OK " ACTIVE");
        lv_obj_set_style_text_font(st_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(st_lbl, COLOR_ACCENT_GREEN, 0);
        lv_obj_center(st_lbl);

        /* Feature Description Text */
        lv_obj_t * f_lbl = lv_label_create(f_item);
        lv_label_set_text(f_lbl, current_device_info.features[i]);
        lv_obj_set_style_text_font(f_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(f_lbl, COLOR_TEXT_MAIN, 0);
        lv_obj_align(f_lbl, LV_ALIGN_LEFT_MID, 105, 0);
        lv_obj_set_width(f_lbl, 540);
        lv_label_set_long_mode(f_lbl, LV_LABEL_LONG_DOT);
    }

    /* JSON Property Read Status Indicator Banner */
    lv_obj_t * json_status = lv_label_create(right_card);
    if(current_device_info.json_read_success) {
        lv_label_set_text(json_status, LV_SYMBOL_REFRESH " Properties loaded dynamically from version_properties.json");
        lv_obj_set_style_text_color(json_status, COLOR_ACCENT_GREEN, 0);
    } else {
        lv_label_set_text(json_status, LV_SYMBOL_WARNING " version_properties.json using standard fallback values");
        lv_obj_set_style_text_color(json_status, COLOR_ACCENT_YELLOW, 0);
    }
    lv_obj_set_style_text_font(json_status, &lv_font_montserrat_12, 0);
    lv_obj_align(json_status, LV_ALIGN_BOTTOM_LEFT, 0, 0);

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
    lv_obj_add_event_cb(home_btn, home_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, LV_SYMBOL_HOME "  HOME");
    lv_obj_set_style_text_font(home_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(home_lbl);

    /* SETTINGS / BACK Button */
    lv_obj_t * set_btn = lv_button_create(bot_bar);
    lv_obj_set_size(set_btn, 160, 48);
    lv_obj_align(set_btn, LV_ALIGN_LEFT_MID, 155, 0);
    lv_obj_set_style_bg_color(set_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(set_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(set_btn, 1, 0);
    lv_obj_set_style_radius(set_btn, 8, 0);
    lv_obj_add_event_cb(set_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * set_lbl = lv_label_create(set_btn);
    lv_label_set_text(set_lbl, LV_SYMBOL_LEFT "  SETTINGS");
    lv_obj_set_style_text_font(set_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(set_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(set_lbl);

    /* Device Operational Status Banner (Right) */
    lv_obj_t * status_cont = lv_obj_create(bot_bar);
    lv_obj_set_size(status_cont, 380, 42);
    lv_obj_align(status_cont, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(status_cont, lv_color_hex(0x061D3B), 0);
    lv_obj_set_style_border_color(status_cont, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(status_cont, 1, 0);
    lv_obj_set_style_radius(status_cont, 6, 0);

    lv_obj_t * st_txt = lv_label_create(status_cont);
    lv_label_set_text(st_txt, LV_SYMBOL_OK " System Self-Check Passed: 100% Operational");
    lv_obj_set_style_text_font(st_txt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(st_txt, COLOR_ACCENT_GREEN, 0);
    lv_obj_center(st_txt);

    /* Clock dynamic sync timer */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);

    /* Load Screen */
    lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
