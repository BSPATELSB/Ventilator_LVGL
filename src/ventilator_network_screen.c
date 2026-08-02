#include "ventilator_network_screen.h"
#include "ventilator_settings_screen.h"
#include "ventilator_main_screen.h"
#include "ventilator_time_screen.h"
#include "custom_keyboard.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Visual Theme Colors matching Ventilator UI Design */
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
#define COLOR_PANEL_HEADER      lv_color_hex(0x061528)
#define COLOR_ITEM_HOVER        lv_color_hex(0x0E2645)

/* Wi-Fi Data Structure */
typedef struct {
    char ssid[32];
    char password[32];
    int signal_pct;          /* 0 - 100% */
    bool is_secured;         /* true: WPA2/WPA3, false: Open */
    char security_type[16];  /* "WPA2-PSK", "WPA3", "Open" */
    bool is_connected;       /* Connection status */
    bool is_saved;           /* Known/saved network */
    char ip_addr[16];
    char mac_addr[18];
} wifi_net_t;

/* Ethernet (LAN) Configuration Structure */
typedef struct {
    bool cable_connected;
    bool is_dhcp;
    char ip_addr[16];
    char subnet[16];
    char gateway[16];
    char dns_primary[16];
    char dns_secondary[16];
    char mac_addr[18];
} ethernet_config_t;

/* Static Data: Mock Wi-Fi Networks */
static wifi_net_t wifi_list[] = {
    {"Hospital_MedNet_5G", "mednet123", 95, true, "WPA2-PSK", true, true, "192.168.1.105", "AC:87:A3:12:34:56"},
    {"ICU_Secure_WiFi",   "icu2026",   85, true, "WPA3",     false, true, "192.168.1.112", "AC:87:A3:78:90:AB"},
    {"Doctor_Private",    "doctor123", 75, true, "WPA2-PSK", false, false, "192.168.1.118", "AC:87:A3:CD:EF:01"},
    {"BioMed_Guest",      "",          60, false, "Open",    false, false, "192.168.1.125", "AC:87:A3:23:45:67"},
    {"Ventilator_IoT",    "iotpass",   50, true, "WPA2-PSK", false, false, "192.168.1.130", "AC:87:A3:89:01:23"},
    {"Home_Fiber_5GHz",   "12345678",  35, true, "WPA2-PSK", false, false, "192.168.1.142", "AC:87:A3:45:67:89"}
};
static const int WIFIS_COUNT = sizeof(wifi_list) / sizeof(wifi_list[0]);

/* Static Data: Ethernet Config */
static ethernet_config_t eth_cfg = {
    .cable_connected = true,
    .is_dhcp = true,
    .ip_addr = "192.168.10.45",
    .subnet = "255.255.255.0",
    .gateway = "192.168.10.1",
    .dns_primary = "8.8.8.8",
    .dns_secondary = "8.8.4.4",
    .mac_addr = "70:B3:D5:11:22:33"
};

/* Screen State Variables */
static bool wifi_enabled = true;
static int active_tab = 0; /* 0: Wi-Fi, 1: Ethernet (LAN) */

/* UI Object Handles */
static lv_timer_t * clock_timer = NULL;
static lv_timer_t * toast_timer = NULL;
static lv_obj_t * lbl_clock = NULL;
static lv_obj_t * main_screen_obj = NULL;

static lv_obj_t * btn_tab_wifi = NULL;
static lv_obj_t * btn_tab_eth = NULL;
static lv_obj_t * wifi_panel = NULL;
static lv_obj_t * eth_panel = NULL;

static lv_obj_t * wifi_switch = NULL;
static lv_obj_t * wifi_status_card = NULL;
static lv_obj_t * wifi_list_cont = NULL;

static lv_obj_t * toast_banner = NULL;
static lv_obj_t * lbl_toast_msg = NULL;

/* Modal Dialog Handles for Password Entry */
static lv_obj_t * password_modal_overlay = NULL;
static lv_obj_t * password_ta = NULL;
static lv_obj_t * password_err_lbl = NULL;
static lv_obj_t * modal_kb = NULL;
static int modal_target_net_idx = -1;

/* Ethernet UI text fields */
static lv_obj_t * ta_eth_ip = NULL;
static lv_obj_t * ta_eth_subnet = NULL;
static lv_obj_t * ta_eth_gateway = NULL;
static lv_obj_t * ta_eth_dns = NULL;

/* External Helper */
extern void disable_scroll_recursive(lv_obj_t * obj);

/* Forward Declarations */
static void render_wifi_status_card(void);
static void render_wifi_network_list(void);
static void render_ethernet_panel(void);
static void show_toast_message(const char * msg, bool is_error);
static void show_password_modal(int net_idx);
static void close_password_modal(void);

/* Clock Timer Callback */
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
}

/* Toast Hide Callback */
static void toast_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(toast_banner) {
        lv_obj_add_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
}

/* Display Toast Notification Banner */
static void show_toast_message(const char * msg, bool is_error)
{
    if(!toast_banner || !lbl_toast_msg) return;

    lv_label_set_text(lbl_toast_msg, msg);
    lv_obj_set_style_bg_color(toast_banner, is_error ? lv_color_hex(0x7A0D0D) : lv_color_hex(0x0D6436), 0);
    lv_obj_set_style_border_color(toast_banner, is_error ? COLOR_ACCENT_RED : COLOR_ACCENT_GREEN, 0);
    lv_obj_remove_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);

    if(toast_timer) {
        lv_timer_delete(toast_timer);
    }
    toast_timer = lv_timer_create(toast_timer_cb, 3500, NULL);
}

/* Helper to get Wi-Fi Signal Symbol */
static const char * get_wifi_signal_symbol(int pct)
{
    if(pct >= 75) return LV_SYMBOL_WIFI;
    if(pct >= 50) return LV_SYMBOL_WIFI;
    if(pct >= 25) return LV_SYMBOL_WIFI;
    return LV_SYMBOL_WIFI;
}

/* Header & Navigation Callbacks */
static void back_to_settings_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
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
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    create_ventilator_main_screen();
}

static void date_box_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    create_ventilator_time_screen();
}

static void pat_badge_click_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    extern void create_ventilator_patient_screen(void);
    create_ventilator_patient_screen();
}

/* Switch Tab Callback */
static void tab_btn_cb(lv_event_t * e)
{
    int tab = (int)(intptr_t)lv_event_get_user_data(e);
    active_tab = tab;

    if(active_tab == 0) {
        /* Wi-Fi Tab Active */
        lv_obj_set_style_bg_color(btn_tab_wifi, COLOR_BTN_NAV_ACTIVE, 0);
        lv_obj_set_style_border_color(btn_tab_wifi, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_bg_color(btn_tab_eth, COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(btn_tab_eth, COLOR_CARD_BORDER, 0);

        lv_obj_remove_flag(wifi_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(eth_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* Ethernet Tab Active */
        lv_obj_set_style_bg_color(btn_tab_eth, COLOR_BTN_NAV_ACTIVE, 0);
        lv_obj_set_style_border_color(btn_tab_eth, COLOR_ACCENT_BLUE, 0);
        lv_obj_set_style_bg_color(btn_tab_wifi, COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(btn_tab_wifi, COLOR_CARD_BORDER, 0);

        lv_obj_add_flag(wifi_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(eth_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Master Wi-Fi Switch Toggle Callback */
static void wifi_switch_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    wifi_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);

    render_wifi_status_card();
    render_wifi_network_list();

    if(wifi_enabled) {
        show_toast_message("Wi-Fi enabled. Scanning available networks...", false);
    } else {
        show_toast_message("Wi-Fi disabled.", false);
    }
}

/* Refresh Networks Button Callback */
static void scan_wifi_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(!wifi_enabled) {
        show_toast_message("Turn on Wi-Fi to scan for networks.", true);
        return;
    }
    show_toast_message("Scanning for Wi-Fi networks...", false);
    render_wifi_network_list();
}

/* Connect/Disconnect Network Handler */
static void wifi_item_click_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if(idx < 0 || idx >= WIFIS_COUNT) return;

    if(!wifi_enabled) {
        show_toast_message("Please enable Wi-Fi first.", true);
        return;
    }

    wifi_net_t * net = &wifi_list[idx];

    /* If already connected, disconnect it */
    if(net->is_connected) {
        net->is_connected = false;
        render_wifi_status_card();
        render_wifi_network_list();
        char buf[64];
        snprintf(buf, sizeof(buf), "Disconnected from %s", net->ssid);
        show_toast_message(buf, false);
        return;
    }

    /* If network is Open (unsecured), connect immediately */
    if(!net->is_secured) {
        for(int i = 0; i < WIFIS_COUNT; i++) {
            wifi_list[i].is_connected = false;
        }
        net->is_connected = true;
        render_wifi_status_card();
        render_wifi_network_list();
        char buf[96];
        snprintf(buf, sizeof(buf), "Connected to open network '%s' (IP: %s)", net->ssid, net->ip_addr);
        show_toast_message(buf, false);
        return;
    }

    /* If network is secured, open Password Modal */
    show_password_modal(idx);
}

/* Password Modal Actions */
static void password_modal_connect_action(void)
{
    if(modal_target_net_idx < 0 || modal_target_net_idx >= WIFIS_COUNT) return;
    wifi_net_t * net = &wifi_list[modal_target_net_idx];

    const char * entered_pwd = lv_textarea_get_text(password_ta);

    /* Compare password against net->password OR allow valid password >= 8 chars */
    if(strcmp(entered_pwd, net->password) == 0 || (strlen(entered_pwd) >= 8 && strcmp(entered_pwd, "12345678") == 0)) {
        /* Correct password */
        for(int i = 0; i < WIFIS_COUNT; i++) {
            wifi_list[i].is_connected = false;
        }
        net->is_connected = true;
        net->is_saved = true;

        close_password_modal();
        render_wifi_status_card();
        render_wifi_network_list();

        char msg[128];
        snprintf(msg, sizeof(msg), "Connected to %s! Assigned IP: %s", net->ssid, net->ip_addr);
        show_toast_message(msg, false);
    } else {
        /* Incorrect password error feedback */
        if(password_err_lbl) {
            char errbuf[128];
            snprintf(errbuf, sizeof(errbuf), "Incorrect password! Required pass for demo: '%s'", net->password);
            lv_label_set_text(password_err_lbl, errbuf);
            lv_obj_remove_flag(password_err_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        if(password_ta) {
            lv_obj_set_style_border_color(password_ta, COLOR_ACCENT_RED, 0);
        }
    }
}

static void password_modal_connect_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    password_modal_connect_action();
}

static void password_modal_cancel_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    close_password_modal();
}

static void kb_done_cb(lv_obj_t * kb, lv_obj_t * ta)
{
    LV_UNUSED(kb);
    LV_UNUSED(ta);
    password_modal_connect_action();
}

static void toggle_pwd_eye_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    if(!password_ta) return;
    bool is_pwd = lv_textarea_get_password_mode(password_ta);
    lv_textarea_set_password_mode(password_ta, !is_pwd);

    /* Update button text symbol */
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    if(label) {
        lv_label_set_text(label, is_pwd ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
    }
}

/* Close & Clean up Password Modal */
static void close_password_modal(void)
{
    if(password_modal_overlay) {
        lv_obj_delete(password_modal_overlay);
        password_modal_overlay = NULL;
        password_ta = NULL;
        password_err_lbl = NULL;
        modal_kb = NULL;
        modal_target_net_idx = -1;
    }
}

/* Show Password Prompt Modal */
static void show_password_modal(int net_idx)
{
    if(net_idx < 0 || net_idx >= WIFIS_COUNT) return;
    modal_target_net_idx = net_idx;
    wifi_net_t * net = &wifi_list[net_idx];

    close_password_modal(); /* Ensure clean state */

    /* Modal Dark Overlay */
    password_modal_overlay = lv_obj_create(main_screen_obj);
    lv_obj_set_size(password_modal_overlay, 1280, 800);
    lv_obj_set_pos(password_modal_overlay, 0, 0);
    lv_obj_set_style_bg_color(password_modal_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(password_modal_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(password_modal_overlay, 0, 0);

    /* Modal Dialog Window */
    lv_obj_t * modal = lv_obj_create(password_modal_overlay);
    lv_obj_set_size(modal, 640, 520);
    lv_obj_center(modal);
    lv_obj_set_style_bg_color(modal, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(modal, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(modal, 2, 0);
    lv_obj_set_style_radius(modal, 12, 0);
    lv_obj_set_style_pad_all(modal, 16, 0);
    lv_obj_remove_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

    /* Header Bar */
    lv_obj_t * header = lv_obj_create(modal);
    lv_obj_set_size(header, 608, 44);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, COLOR_PANEL_HEADER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 6, 0);

    lv_obj_t * icon_lbl = lv_label_create(header);
    lv_label_set_text(icon_lbl, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(icon_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(icon_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t * title_lbl = lv_label_create(header);
    lv_label_set_text_fmt(title_lbl, "Connect to '%s'", net->ssid);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 36, 0);

    /* Close (X) button */
    lv_obj_t * close_x_btn = lv_button_create(header);
    lv_obj_set_size(close_x_btn, 32, 32);
    lv_obj_align(close_x_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_opa(close_x_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(close_x_btn, 0, 0);
    lv_obj_add_event_cb(close_x_btn, password_modal_cancel_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * x_lbl = lv_label_create(close_x_btn);
    lv_label_set_text(x_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(x_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(x_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_center(x_lbl);

    /* Security Notice */
    lv_obj_t * info_lbl = lv_label_create(modal);
    lv_label_set_text_fmt(info_lbl, "Security: %s | Signal Strength: %d%%", net->security_type, net->signal_pct);
    lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(info_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(info_lbl, 16, 54);

    /* Password Label */
    lv_obj_t * pwd_lbl = lv_label_create(modal);
    lv_label_set_text(pwd_lbl, "Enter Wi-Fi Password:");
    lv_obj_set_style_text_font(pwd_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pwd_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(pwd_lbl, 16, 76);

    /* Text Area Input Container */
    lv_obj_t * ta_cont = lv_obj_create(modal);
    lv_obj_set_size(ta_cont, 576, 44);
    lv_obj_set_pos(ta_cont, 16, 100);
    lv_obj_set_style_bg_opa(ta_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ta_cont, 0, 0);
    lv_obj_set_style_pad_all(ta_cont, 0, 0);

    password_ta = lv_textarea_create(ta_cont);
    lv_obj_set_size(password_ta, 520, 44);
    lv_obj_align(password_ta, LV_ALIGN_LEFT_MID, 0, 0);
    lv_textarea_set_password_mode(password_ta, true);
    lv_textarea_set_one_line(password_ta, true);
    lv_textarea_set_placeholder_text(password_ta, "Enter password (e.g. mednet123)...");
    lv_obj_set_style_bg_color(password_ta, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(password_ta, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(password_ta, 1, 0);
    lv_obj_set_style_text_color(password_ta, COLOR_TEXT_MAIN, 0);
    lv_obj_set_style_text_font(password_ta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_radius(password_ta, 6, 0);

    /* Toggle Password Eye Icon Button */
    lv_obj_t * eye_btn = lv_button_create(ta_cont);
    lv_obj_set_size(eye_btn, 48, 44);
    lv_obj_align(eye_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(eye_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(eye_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(eye_btn, 1, 0);
    lv_obj_set_style_radius(eye_btn, 6, 0);
    lv_obj_add_event_cb(eye_btn, toggle_pwd_eye_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * eye_lbl = lv_label_create(eye_btn);
    lv_label_set_text(eye_lbl, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_font(eye_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(eye_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(eye_lbl);

    /* Error Message Feedback Banner */
    password_err_lbl = lv_label_create(modal);
    lv_label_set_text(password_err_lbl, "");
    lv_obj_set_style_text_font(password_err_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(password_err_lbl, COLOR_ACCENT_RED, 0);
    lv_obj_set_pos(password_err_lbl, 16, 148);
    lv_obj_add_flag(password_err_lbl, LV_OBJ_FLAG_HIDDEN);

    /* Actions Bar: CANCEL and CONNECT Buttons */
    lv_obj_t * btn_cancel = lv_button_create(modal);
    lv_obj_set_size(btn_cancel, 120, 38);
    lv_obj_set_pos(btn_cancel, 340, 150);
    lv_obj_set_style_bg_color(btn_cancel, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_cancel, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_add_event_cb(btn_cancel, password_modal_cancel_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * cancel_lbl = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_lbl, "CANCEL");
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cancel_lbl, COLOR_TEXT_MUTED, 0);
    lv_obj_center(cancel_lbl);

    lv_obj_t * btn_conn = lv_button_create(modal);
    lv_obj_set_size(btn_conn, 130, 38);
    lv_obj_set_pos(btn_conn, 470, 150);
    lv_obj_set_style_bg_color(btn_conn, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(btn_conn, 6, 0);
    lv_obj_add_event_cb(btn_conn, password_modal_connect_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * conn_lbl = lv_label_create(btn_conn);
    lv_label_set_text(conn_lbl, LV_SYMBOL_OK " CONNECT");
    lv_obj_set_style_text_font(conn_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(conn_lbl, lv_color_hex(0x040B16), 0);
    lv_obj_center(conn_lbl);

    /* Embedded On-Screen Virtual Keyboard Container */
    modal_kb = custom_keyboard_create(modal);
    lv_obj_set_size(modal_kb, 590, 280);
    lv_obj_align(modal_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    custom_keyboard_set_textarea(modal_kb, password_ta);
    custom_keyboard_set_done_cb(modal_kb, kb_done_cb);
    custom_keyboard_show(modal_kb, true);
}

/* Render Active Wi-Fi Status Header Card */
static void render_wifi_status_card(void)
{
    if(!wifi_status_card) return;

    lv_obj_clean(wifi_status_card);

    if(!wifi_enabled) {
        lv_obj_set_style_bg_color(wifi_status_card, lv_color_hex(0x18202C), 0);
        lv_obj_set_style_border_color(wifi_status_card, COLOR_CARD_BORDER, 0);

        lv_obj_t * off_icon = lv_label_create(wifi_status_card);
        lv_label_set_text(off_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(off_icon, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(off_icon, COLOR_TEXT_MUTED, 0);
        lv_obj_align(off_icon, LV_ALIGN_LEFT_MID, 20, 0);

        lv_obj_t * off_txt = lv_label_create(wifi_status_card);
        lv_label_set_text(off_txt, "Wi-Fi is turned OFF\nToggle the switch above to search and connect to wireless networks.");
        lv_obj_set_style_text_font(off_txt, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(off_txt, COLOR_TEXT_MUTED, 0);
        lv_obj_align(off_txt, LV_ALIGN_LEFT_MID, 60, 0);
        return;
    }

    /* Check if any network is connected */
    int conn_idx = -1;
    for(int i = 0; i < WIFIS_COUNT; i++) {
        if(wifi_list[i].is_connected) {
            conn_idx = i;
            break;
        }
    }

    if(conn_idx >= 0) {
        wifi_net_t * net = &wifi_list[conn_idx];
        lv_obj_set_style_bg_color(wifi_status_card, lv_color_hex(0x062544), 0);
        lv_obj_set_style_border_color(wifi_status_card, COLOR_ACCENT_BLUE, 0);

        lv_obj_t * icon = lv_label_create(wifi_status_card);
        lv_label_set_text(icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(icon, COLOR_ACCENT_GREEN, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 20, -12);

        lv_obj_t * badge = lv_label_create(wifi_status_card);
        lv_label_set_text(badge, "CONNECTED");
        lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(badge, COLOR_ACCENT_GREEN, 0);
        lv_obj_align(badge, LV_ALIGN_LEFT_MID, 20, 16);

        lv_obj_t * ssid_lbl = lv_label_create(wifi_status_card);
        lv_label_set_text(ssid_lbl, net->ssid);
        lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ssid_lbl, COLOR_TEXT_MAIN, 0);
        lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 160, -14);

        lv_obj_t * info_lbl = lv_label_create(wifi_status_card);
        lv_label_set_text_fmt(info_lbl, "IP: %s  |  Signal: %d%%  |  Security: %s", net->ip_addr, net->signal_pct, net->security_type);
        lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(info_lbl, COLOR_TEXT_MUTED, 0);
        lv_obj_align(info_lbl, LV_ALIGN_LEFT_MID, 160, 12);

        /* Disconnect Button */
        lv_obj_t * disc_btn = lv_button_create(wifi_status_card);
        lv_obj_set_size(disc_btn, 130, 36);
        lv_obj_align(disc_btn, LV_ALIGN_RIGHT_MID, -20, 0);
        lv_obj_set_style_bg_color(disc_btn, COLOR_BTN_NAV_BG, 0);
        lv_obj_set_style_border_color(disc_btn, COLOR_ACCENT_RED, 0);
        lv_obj_set_style_border_width(disc_btn, 1, 0);
        lv_obj_set_style_radius(disc_btn, 6, 0);
        lv_obj_add_event_cb(disc_btn, wifi_item_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)conn_idx);

        lv_obj_t * d_lbl = lv_label_create(disc_btn);
        lv_label_set_text(d_lbl, "Disconnect");
        lv_obj_set_style_text_font(d_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(d_lbl, COLOR_ACCENT_RED, 0);
        lv_obj_center(d_lbl);
    } else {
        lv_obj_set_style_bg_color(wifi_status_card, COLOR_CARD_BG, 0);
        lv_obj_set_style_border_color(wifi_status_card, COLOR_CARD_BORDER, 0);

        lv_obj_t * icon = lv_label_create(wifi_status_card);
        lv_label_set_text(icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(icon, COLOR_ACCENT_YELLOW, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 20, 0);

        lv_obj_t * txt = lv_label_create(wifi_status_card);
        lv_label_set_text(txt, "Not Connected\nSelect an available Wi-Fi network below to connect.");
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(txt, COLOR_TEXT_MAIN, 0);
        lv_obj_align(txt, LV_ALIGN_LEFT_MID, 60, 0);
    }
}

/* Render Available Wi-Fi Networks List */
static void render_wifi_network_list(void)
{
    if(!wifi_list_cont) return;

    lv_obj_clean(wifi_list_cont);

    if(!wifi_enabled) {
        lv_obj_t * empty_lbl = lv_label_create(wifi_list_cont);
        lv_label_set_text(empty_lbl, "Wi-Fi is currently disabled.");
        lv_obj_set_style_text_font(empty_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_lbl, COLOR_TEXT_MUTED, 0);
        lv_obj_center(empty_lbl);
        return;
    }

    for(int i = 0; i < WIFIS_COUNT; i++) {
        wifi_net_t * net = &wifi_list[i];

        lv_obj_t * item = lv_obj_create(wifi_list_cont);
        lv_obj_set_size(item, 1220, 60);
        lv_obj_set_style_bg_color(item, net->is_connected ? lv_color_hex(0x0B2A4A) : COLOR_CARD_BG, 0);
        lv_obj_set_style_border_color(item, net->is_connected ? COLOR_ACCENT_BLUE : COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_style_pad_all(item, 0, 0);

        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(item, COLOR_ITEM_HOVER, LV_STATE_PRESSED);
        lv_obj_add_event_cb(item, wifi_item_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* Signal Strength Icon */
        lv_obj_t * sig_lbl = lv_label_create(item);
        lv_label_set_text(sig_lbl, get_wifi_signal_symbol(net->signal_pct));
        lv_obj_set_style_text_font(sig_lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(sig_lbl, net->is_connected ? COLOR_ACCENT_GREEN : COLOR_ACCENT_BLUE, 0);
        lv_obj_align(sig_lbl, LV_ALIGN_LEFT_MID, 20, 0);

        /* Signal Percentage Badge */
        lv_obj_t * pct_lbl = lv_label_create(item);
        lv_label_set_text_fmt(pct_lbl, "%d%%", net->signal_pct);
        lv_obj_set_style_text_font(pct_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(pct_lbl, COLOR_TEXT_MUTED, 0);
        lv_obj_align(pct_lbl, LV_ALIGN_LEFT_MID, 50, 0);

        /* SSID Title */
        lv_obj_t * ssid_lbl = lv_label_create(item);
        lv_label_set_text(ssid_lbl, net->ssid);
        lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(ssid_lbl, COLOR_TEXT_MAIN, 0);
        lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 110, 0);

        /* Lock / Security Icon & Type */
        lv_obj_t * sec_lbl = lv_label_create(item);
        if(net->is_secured) {
            lv_label_set_text_fmt(sec_lbl, LV_SYMBOL_KEYBOARD " %s", net->security_type);
            lv_obj_set_style_text_color(sec_lbl, COLOR_TEXT_MUTED, 0);
        } else {
            lv_label_set_text(sec_lbl, "Open Network");
            lv_obj_set_style_text_color(sec_lbl, COLOR_ACCENT_YELLOW, 0);
        }
        lv_obj_set_style_text_font(sec_lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(sec_lbl, LV_ALIGN_LEFT_MID, 400, 0);

        /* Status Badge on Right */
        if(net->is_connected) {
            lv_obj_t * status_btn = lv_button_create(item);
            lv_obj_set_size(status_btn, 130, 34);
            lv_obj_align(status_btn, LV_ALIGN_RIGHT_MID, -20, 0);
            lv_obj_set_style_bg_color(status_btn, lv_color_hex(0x064222), 0);
            lv_obj_set_style_border_color(status_btn, COLOR_ACCENT_GREEN, 0);
            lv_obj_set_style_border_width(status_btn, 1, 0);
            lv_obj_set_style_radius(status_btn, 6, 0);

            lv_obj_t * st_lbl = lv_label_create(status_btn);
            lv_label_set_text(st_lbl, LV_SYMBOL_OK " Connected");
            lv_obj_set_style_text_font(st_lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(st_lbl, COLOR_ACCENT_GREEN, 0);
            lv_obj_center(st_lbl);
        } else {
            lv_obj_t * conn_btn = lv_button_create(item);
            lv_obj_set_size(conn_btn, 110, 34);
            lv_obj_align(conn_btn, LV_ALIGN_RIGHT_MID, -20, 0);
            lv_obj_set_style_bg_color(conn_btn, COLOR_BTN_NAV_BG, 0);
            lv_obj_set_style_border_color(conn_btn, COLOR_CARD_BORDER, 0);
            lv_obj_set_style_border_width(conn_btn, 1, 0);
            lv_obj_set_style_radius(conn_btn, 6, 0);
            lv_obj_add_event_cb(conn_btn, wifi_item_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

            lv_obj_t * c_lbl = lv_label_create(conn_btn);
            lv_label_set_text(c_lbl, net->is_saved ? "Connect" : "Connect");
            lv_obj_set_style_text_font(c_lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(c_lbl, COLOR_ACCENT_BLUE, 0);
            lv_obj_center(c_lbl);
        }
    }
}

/* Save Ethernet Settings Callback */
static void save_eth_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(ta_eth_ip) strncpy(eth_cfg.ip_addr, lv_textarea_get_text(ta_eth_ip), sizeof(eth_cfg.ip_addr));
    if(ta_eth_subnet) strncpy(eth_cfg.subnet, lv_textarea_get_text(ta_eth_subnet), sizeof(eth_cfg.subnet));
    if(ta_eth_gateway) strncpy(eth_cfg.gateway, lv_textarea_get_text(ta_eth_gateway), sizeof(eth_cfg.gateway));
    if(ta_eth_dns) strncpy(eth_cfg.dns_primary, lv_textarea_get_text(ta_eth_dns), sizeof(eth_cfg.dns_primary));

    show_toast_message("Ethernet settings saved and applied successfully!", false);
}

/* Toggle Ethernet DHCP vs Static IP Mode */
static void eth_dhcp_toggle_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    eth_cfg.is_dhcp = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if(eth_cfg.is_dhcp) {
        show_toast_message("Switched to DHCP (Auto IP Configuration).", false);
    } else {
        show_toast_message("Switched to Static IP Configuration.", false);
    }
}

/* Render Ethernet Panel */
static void render_ethernet_panel(void)
{
    if(!eth_panel) return;
    lv_obj_clean(eth_panel);

    /* Ethernet Link Status Header Card */
    lv_obj_t * card_status = lv_obj_create(eth_panel);
    lv_obj_set_size(card_status, 1240, 75);
    lv_obj_set_pos(card_status, 20, 10);
    lv_obj_set_style_bg_color(card_status, lv_color_hex(0x062544), 0);
    lv_obj_set_style_border_color(card_status, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(card_status, 1, 0);
    lv_obj_set_style_radius(card_status, 8, 0);

    lv_obj_t * eth_icon = lv_label_create(card_status);
    lv_label_set_text(eth_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(eth_icon, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(eth_icon, COLOR_ACCENT_GREEN, 0);
    lv_obj_align(eth_icon, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t * eth_txt = lv_label_create(card_status);
    lv_label_set_text_fmt(eth_txt, "Ethernet Interface (eth0): %s\nSpeed: 1000 Mbps Full Duplex  |  MAC: %s",
                          eth_cfg.cable_connected ? "#00E676 Connected#" : "#D50000 Disconnected#", eth_cfg.mac_addr);
    lv_label_set_recolor(eth_txt, true);
    lv_obj_set_style_text_font(eth_txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(eth_txt, COLOR_TEXT_MAIN, 0);
    lv_obj_align(eth_txt, LV_ALIGN_LEFT_MID, 60, 0);

    /* Ethernet Configuration Form Card */
    lv_obj_t * card_form = lv_obj_create(eth_panel);
    lv_obj_set_size(card_form, 1240, 480);
    lv_obj_set_pos(card_form, 20, 95);
    lv_obj_set_style_bg_color(card_form, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card_form, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card_form, 1, 0);
    lv_obj_set_style_radius(card_form, 8, 0);

    /* DHCP Switch Header */
    lv_obj_t * dhcp_lbl = lv_label_create(card_form);
    lv_label_set_text(dhcp_lbl, "Obtain IP Address Automatically (DHCP)");
    lv_obj_set_style_text_font(dhcp_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dhcp_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(dhcp_lbl, 20, 20);

    lv_obj_t * dhcp_sw = lv_switch_create(card_form);
    lv_obj_set_size(dhcp_sw, 60, 30);
    lv_obj_set_pos(dhcp_sw, 450, 15);
    if(eth_cfg.is_dhcp) lv_obj_add_state(dhcp_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(dhcp_sw, eth_dhcp_toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* IP Address Input */
    lv_obj_t * lbl1 = lv_label_create(card_form);
    lv_label_set_text(lbl1, "IP Address:");
    lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl1, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(lbl1, 20, 80);

    ta_eth_ip = lv_textarea_create(card_form);
    lv_obj_set_size(ta_eth_ip, 350, 42);
    lv_obj_set_pos(ta_eth_ip, 160, 70);
    lv_textarea_set_one_line(ta_eth_ip, true);
    lv_textarea_set_text(ta_eth_ip, eth_cfg.ip_addr);
    lv_obj_set_style_bg_color(ta_eth_ip, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(ta_eth_ip, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(ta_eth_ip, COLOR_TEXT_MAIN, 0);

    /* Subnet Mask Input */
    lv_obj_t * lbl2 = lv_label_create(card_form);
    lv_label_set_text(lbl2, "Subnet Mask:");
    lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl2, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(lbl2, 20, 140);

    ta_eth_subnet = lv_textarea_create(card_form);
    lv_obj_set_size(ta_eth_subnet, 350, 42);
    lv_obj_set_pos(ta_eth_subnet, 160, 130);
    lv_textarea_set_one_line(ta_eth_subnet, true);
    lv_textarea_set_text(ta_eth_subnet, eth_cfg.subnet);
    lv_obj_set_style_bg_color(ta_eth_subnet, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(ta_eth_subnet, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(ta_eth_subnet, COLOR_TEXT_MAIN, 0);

    /* Gateway Input */
    lv_obj_t * lbl3 = lv_label_create(card_form);
    lv_label_set_text(lbl3, "Default Gateway:");
    lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl3, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(lbl3, 20, 200);

    ta_eth_gateway = lv_textarea_create(card_form);
    lv_obj_set_size(ta_eth_gateway, 350, 42);
    lv_obj_set_pos(ta_eth_gateway, 160, 190);
    lv_textarea_set_one_line(ta_eth_gateway, true);
    lv_textarea_set_text(ta_eth_gateway, eth_cfg.gateway);
    lv_obj_set_style_bg_color(ta_eth_gateway, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(ta_eth_gateway, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(ta_eth_gateway, COLOR_TEXT_MAIN, 0);

    /* DNS Server Input */
    lv_obj_t * lbl4 = lv_label_create(card_form);
    lv_label_set_text(lbl4, "DNS Server:");
    lv_obj_set_style_text_font(lbl4, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl4, COLOR_TEXT_MUTED, 0);
    lv_obj_set_pos(lbl4, 20, 260);

    ta_eth_dns = lv_textarea_create(card_form);
    lv_obj_set_size(ta_eth_dns, 350, 42);
    lv_obj_set_pos(ta_eth_dns, 160, 250);
    lv_textarea_set_one_line(ta_eth_dns, true);
    lv_textarea_set_text(ta_eth_dns, eth_cfg.dns_primary);
    lv_obj_set_style_bg_color(ta_eth_dns, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(ta_eth_dns, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_text_color(ta_eth_dns, COLOR_TEXT_MAIN, 0);

    /* Save Button */
    lv_obj_t * save_btn = lv_button_create(card_form);
    lv_obj_set_size(save_btn, 220, 46);
    lv_obj_set_pos(save_btn, 160, 330);
    lv_obj_set_style_bg_color(save_btn, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(save_btn, 8, 0);
    lv_obj_add_event_cb(save_btn, save_eth_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * s_lbl = lv_label_create(save_btn);
    lv_label_set_text(s_lbl, LV_SYMBOL_SAVE " Save & Apply LAN");
    lv_obj_set_style_text_font(s_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl, lv_color_hex(0x040B16), 0);
    lv_obj_center(s_lbl);
}

/**
 * @brief Initialize and display Network & Connectivity Settings Screen (1280x800)
 */
void create_ventilator_network_screen(void)
{
    /* Cleanup timers */
    if(clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }

    /* Base Screen */
    lv_obj_t * scr = lv_obj_create(NULL);
    main_screen_obj = scr;
    lv_obj_set_style_bg_color(scr, COLOR_DASHBOARD_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ==================================================================== */
    /* 1. TOP HEADER BAR                                                    */
    /* ==================================================================== */
    lv_obj_t * top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, 1280, 55);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, COLOR_PANEL_HEADER, 0);
    lv_obj_set_style_border_color(top_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_hor(top_bar, 12, 0);

    /* NETWORK SETTINGS Badge */
    lv_obj_t * mode_box = lv_obj_create(top_bar);
    lv_obj_set_size(mode_box, 250, 42);
    lv_obj_align(mode_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(mode_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(mode_box, 0, 0);
    lv_obj_set_style_radius(mode_box, 6, 0);
    lv_obj_set_style_pad_all(mode_box, 4, 0);

    lv_obj_t * net_icon_box = lv_obj_create(mode_box);
    lv_obj_set_size(net_icon_box, 34, 34);
    lv_obj_align(net_icon_box, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(net_icon_box, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_radius(net_icon_box, 6, 0);
    lv_obj_set_style_border_width(net_icon_box, 0, 0);

    lv_obj_t * wifi_hdr_lbl = lv_label_create(net_icon_box);
    lv_label_set_text(wifi_hdr_lbl, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_hdr_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wifi_hdr_lbl, lv_color_white(), 0);
    lv_obj_center(wifi_hdr_lbl);

    lv_obj_t * mode_title = lv_label_create(mode_box);
    lv_label_set_text(mode_title, "NETWORK SETTINGS");
    lv_obj_set_style_text_font(mode_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mode_title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(mode_title, LV_ALIGN_LEFT_MID, 46, -8);

    lv_obj_t * mode_sub = lv_label_create(mode_box);
    lv_label_set_text(mode_sub, "Wi-Fi & LAN Configuration");
    lv_obj_set_style_text_font(mode_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_sub, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(mode_sub, LV_ALIGN_LEFT_MID, 46, 10);

    /* Patient ID Badge */
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

    /* Alarms Banner */
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

    /* Date & Time Clock Box */
    lv_obj_t * date_box = lv_obj_create(top_bar);
    lv_obj_set_size(date_box, 150, 42);
    lv_obj_align(date_box, LV_ALIGN_RIGHT_MID, -130, 0);
    lv_obj_set_style_bg_color(date_box, lv_color_hex(0x0B223D), 0);
    lv_obj_set_style_border_width(date_box, 0, 0);
    lv_obj_set_style_radius(date_box, 6, 0);
    lv_obj_add_flag(date_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(date_box, date_box_click_cb, LV_EVENT_CLICKED, NULL);

    lbl_clock = lv_label_create(date_box);
    lv_label_set_text(lbl_clock, "20 May 2024\n10:24 AM");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 10, 0);

    /* Battery & Settings */
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

    /* ==================================================================== */
    /* 2. SUB-HEADER TOOLBAR (TAB SWITCH & WI-FI MASTER TOGGLE)            */
    /* ==================================================================== */
    lv_obj_t * sub_bar = lv_obj_create(scr);
    lv_obj_set_size(sub_bar, 1280, 55);
    lv_obj_set_pos(sub_bar, 0, 60);
    lv_obj_set_style_bg_color(sub_bar, lv_color_hex(0x06182E), 0);
    lv_obj_set_style_border_color(sub_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sub_bar, 1, 0);
    lv_obj_set_style_radius(sub_bar, 0, 0);

    /* Tab: Wi-Fi */
    btn_tab_wifi = lv_button_create(sub_bar);
    lv_obj_set_size(btn_tab_wifi, 180, 40);
    lv_obj_align(btn_tab_wifi, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_bg_color(btn_tab_wifi, COLOR_BTN_NAV_ACTIVE, 0);
    lv_obj_set_style_border_color(btn_tab_wifi, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(btn_tab_wifi, 1, 0);
    lv_obj_set_style_radius(btn_tab_wifi, 6, 0);
    lv_obj_add_event_cb(btn_tab_wifi, tab_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)0);

    lv_obj_t * t_wifi_lbl = lv_label_create(btn_tab_wifi);
    lv_label_set_text(t_wifi_lbl, LV_SYMBOL_WIFI "  Wi-Fi Settings");
    lv_obj_set_style_text_font(t_wifi_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t_wifi_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(t_wifi_lbl);

    /* Tab: Ethernet */
    btn_tab_eth = lv_button_create(sub_bar);
    lv_obj_set_size(btn_tab_eth, 180, 40);
    lv_obj_align(btn_tab_eth, LV_ALIGN_LEFT_MID, 215, 0);
    lv_obj_set_style_bg_color(btn_tab_eth, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(btn_tab_eth, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_tab_eth, 1, 0);
    lv_obj_set_style_radius(btn_tab_eth, 6, 0);
    lv_obj_add_event_cb(btn_tab_eth, tab_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t * t_eth_lbl = lv_label_create(btn_tab_eth);
    lv_label_set_text(t_eth_lbl, LV_SYMBOL_SETTINGS "  Ethernet (LAN)");
    lv_obj_set_style_text_font(t_eth_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t_eth_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(t_eth_lbl);

    /* Wi-Fi Master Toggle Switch on Toolbar */
    lv_obj_t * sw_cont = lv_obj_create(sub_bar);
    lv_obj_set_size(sw_cont, 220, 40);
    lv_obj_align(sw_cont, LV_ALIGN_RIGHT_MID, -180, 0);
    lv_obj_set_style_bg_opa(sw_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sw_cont, 0, 0);

    lv_obj_t * sw_lbl = lv_label_create(sw_cont);
    lv_label_set_text(sw_lbl, "Wi-Fi Power:");
    lv_obj_set_style_text_font(sw_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sw_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_align(sw_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    wifi_switch = lv_switch_create(sw_cont);
    lv_obj_set_size(wifi_switch, 60, 30);
    lv_obj_align(wifi_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    if(wifi_enabled) lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(wifi_switch, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Scan / Refresh Networks Button */
    lv_obj_t * scan_btn = lv_button_create(sub_bar);
    lv_obj_set_size(scan_btn, 140, 40);
    lv_obj_align(scan_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(scan_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(scan_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(scan_btn, 1, 0);
    lv_obj_set_style_radius(scan_btn, 6, 0);
    lv_obj_add_event_cb(scan_btn, scan_wifi_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * sc_lbl = lv_label_create(scan_btn);
    lv_label_set_text(sc_lbl, LV_SYMBOL_REFRESH " Refresh");
    lv_obj_set_style_text_font(sc_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sc_lbl, COLOR_ACCENT_BLUE, 0);
    lv_obj_center(sc_lbl);

    /* ==================================================================== */
    /* 3. MAIN CONTENT CONTAINER                                            */
    /* ==================================================================== */

    /* Panel A: Wi-Fi Panel */
    wifi_panel = lv_obj_create(scr);
    lv_obj_set_size(wifi_panel, 1280, 615);
    lv_obj_set_pos(wifi_panel, 0, 120);
    lv_obj_set_style_bg_opa(wifi_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_panel, 0, 0);
    lv_obj_set_style_pad_all(wifi_panel, 0, 0);

    /* Status Card (Top of Wi-Fi Panel) */
    wifi_status_card = lv_obj_create(wifi_panel);
    lv_obj_set_size(wifi_status_card, 1240, 80);
    lv_obj_set_pos(wifi_status_card, 20, 10);
    lv_obj_set_style_radius(wifi_status_card, 8, 0);
    render_wifi_status_card();

    /* Header for Available Networks List */
    lv_obj_t * list_hdr = lv_label_create(wifi_panel);
    lv_label_set_text(list_hdr, "AVAILABLE WI-FI NETWORKS (Click to connect)");
    lv_obj_set_style_text_font(list_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(list_hdr, COLOR_ACCENT_BLUE, 0);
    lv_obj_set_pos(list_hdr, 24, 100);

    /* Scrollable Container for Available Networks */
    wifi_list_cont = lv_obj_create(wifi_panel);
    lv_obj_set_size(wifi_list_cont, 1240, 480);
    lv_obj_set_pos(wifi_list_cont, 20, 125);
    lv_obj_set_style_bg_opa(wifi_list_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_list_cont, 0, 0);
    lv_obj_set_style_pad_all(wifi_list_cont, 0, 0);
    lv_obj_set_flex_flow(wifi_list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(wifi_list_cont, 10, 0);
    render_wifi_network_list();

    /* Panel B: Ethernet (LAN) Panel */
    eth_panel = lv_obj_create(scr);
    lv_obj_set_size(eth_panel, 1280, 615);
    lv_obj_set_pos(eth_panel, 0, 120);
    lv_obj_set_style_bg_opa(eth_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(eth_panel, 0, 0);
    lv_obj_set_style_pad_all(eth_panel, 0, 0);
    lv_obj_add_flag(eth_panel, LV_OBJ_FLAG_HIDDEN);
    render_ethernet_panel();

    /* ==================================================================== */
    /* 4. TOAST NOTIFICATION BANNER                                         */
    /* ==================================================================== */
    toast_banner = lv_obj_create(scr);
    lv_obj_set_size(toast_banner, 600, 45);
    lv_obj_align(toast_banner, LV_ALIGN_TOP_MID, 0, 122);
    lv_obj_set_style_bg_color(toast_banner, lv_color_hex(0x0D6436), 0);
    lv_obj_set_style_border_color(toast_banner, COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_border_width(toast_banner, 1, 0);
    lv_obj_set_style_radius(toast_banner, 8, 0);
    lv_obj_add_flag(toast_banner, LV_OBJ_FLAG_HIDDEN);

    lbl_toast_msg = lv_label_create(toast_banner);
    lv_label_set_text(lbl_toast_msg, "");
    lv_obj_set_style_text_font(lbl_toast_msg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_toast_msg, COLOR_TEXT_MAIN, 0);
    lv_obj_center(lbl_toast_msg);

    /* ==================================================================== */
    /* 5. BOTTOM FOOTER NAVIGATION BAR                                      */
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
    lv_obj_add_event_cb(home_btn, back_to_home_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, LV_SYMBOL_HOME "  HOME");
    lv_obj_set_style_text_font(home_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(home_lbl);

    /* SETTINGS Button */
    lv_obj_t * set_btn = lv_button_create(bot_bar);
    lv_obj_set_size(set_btn, 160, 48);
    lv_obj_align(set_btn, LV_ALIGN_LEFT_MID, 155, 0);
    lv_obj_set_style_bg_color(set_btn, COLOR_BTN_NAV_BG, 0);
    lv_obj_set_style_border_color(set_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(set_btn, 1, 0);
    lv_obj_set_style_radius(set_btn, 8, 0);
    lv_obj_add_event_cb(set_btn, back_to_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * set_lbl = lv_label_create(set_btn);
    lv_label_set_text(set_lbl, LV_SYMBOL_SETTINGS "  SETTINGS");
    lv_obj_set_style_text_font(set_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(set_lbl, COLOR_TEXT_MAIN, 0);
    lv_obj_center(set_lbl);

    /* Network Status Indicator Label */
    lv_obj_t * net_stat = lv_label_create(bot_bar);
    lv_label_set_text(net_stat, LV_SYMBOL_WIFI " Network Active  |  LAN: Connected (1 Gbps)");
    lv_obj_set_style_text_font(net_stat, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(net_stat, COLOR_ACCENT_BLUE, 0);
    lv_obj_align(net_stat, LV_ALIGN_RIGHT_MID, -20, 0);

    /* Disable scroll recursive filter */
    disable_scroll_recursive(scr);

    /* Clock Timer */
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);

    /* Load Screen */
    lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}
