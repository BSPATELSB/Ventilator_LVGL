#include "ventilator_ui.h"
#include "ventilator_main_screen.h"
#include "usb_detect.h"
#include "theme_manager.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

/* Color Definitions matching dynamic theme palette */
#define COLOR_BG_DARK           (theme_get_palette()->bg)
#define COLOR_AURA_GLOW         (theme_get_palette()->card_bg)
#define COLOR_BADGE_BG          (theme_get_palette()->panel_hdr)
#define COLOR_CYAN_ACCENT       (theme_get_palette()->accent_blue)
#define COLOR_CYAN_BRIGHT       (theme_get_palette()->accent_blue)
#define COLOR_TEXT_WHITE        (theme_get_palette()->text_main)
#define COLOR_TEXT_SUBTITLE     (theme_get_palette()->text_muted)
#define COLOR_TEXT_TAGLINE      (theme_get_palette()->text_muted)
#define COLOR_TEXT_MUTED        (theme_get_palette()->text_muted)
#define COLOR_CARD_BG           (theme_get_palette()->card_bg)
#define COLOR_CARD_BORDER       (theme_get_palette()->card_border)
#define COLOR_BAR_BG            (theme_get_palette()->btn_bg)

/* UI References for Animation */
static lv_obj_t * boot_arc = NULL;
static lv_obj_t * boot_bar = NULL;
static lv_obj_t * label_percentage = NULL;
static lv_obj_t * label_status_footer = NULL;
static lv_obj_t * label_usb_status = NULL;
static lv_timer_t * boot_anim_timer = NULL;

static int boot_progress = 72; /* Default snapshot matches 72% from screenshot */
static int boot_direction = 1;

/* Draw detailed glowing vector lungs graphic onto canvas */
static void draw_lungs_graphic(lv_obj_t * parent)
{
    lv_obj_t * canvas = lv_canvas_create(parent);
    lv_obj_set_size(canvas, 200, 200);
    lv_obj_center(canvas);

    lv_draw_buf_t * draw_buf = lv_draw_buf_create(200, 200, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    lv_canvas_set_draw_buf(canvas, draw_buf);
    lv_canvas_fill_bg(canvas, lv_color_hex(0x051733), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    /* Draw trachea line */
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = COLOR_CYAN_ACCENT;
    line_dsc.width = 4;
    line_dsc.round_start = true;
    line_dsc.round_end = true;

    line_dsc.p1.x = 100; line_dsc.p1.y = 35;
    line_dsc.p2.x = 100; line_dsc.p2.y = 80;
    lv_draw_line(&layer, &line_dsc);

    /* Left Bronchial Main Branch */
    line_dsc.width = 3;
    line_dsc.p1.x = 100; line_dsc.p1.y = 80;
    line_dsc.p2.x = 75;  line_dsc.p2.y = 105;
    lv_draw_line(&layer, &line_dsc);

    /* Left Bronchial Sub-branches */
    line_dsc.width = 2;
    line_dsc.p1.x = 75; line_dsc.p1.y = 105;
    line_dsc.p2.x = 55; line_dsc.p2.y = 125;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.p1.x = 75; line_dsc.p1.y = 105;
    line_dsc.p2.x = 65; line_dsc.p2.y = 145;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.p1.x = 75; line_dsc.p1.y = 105;
    line_dsc.p2.x = 88; line_dsc.p2.y = 135;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.width = 1;
    line_dsc.p1.x = 55; line_dsc.p1.y = 125;
    line_dsc.p2.x = 45; line_dsc.p2.y = 138;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.p1.x = 65; line_dsc.p1.y = 145;
    line_dsc.p2.x = 58; line_dsc.p2.y = 160;
    lv_draw_line(&layer, &line_dsc);

    /* Right Bronchial Main Branch */
    line_dsc.width = 3;
    line_dsc.p1.x = 100; line_dsc.p1.y = 80;
    line_dsc.p2.x = 125; line_dsc.p2.y = 105;
    lv_draw_line(&layer, &line_dsc);

    /* Right Bronchial Sub-branches */
    line_dsc.width = 2;
    line_dsc.p1.x = 125; line_dsc.p1.y = 105;
    line_dsc.p2.x = 145; line_dsc.p2.y = 125;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.p1.x = 125; line_dsc.p1.y = 105;
    line_dsc.p2.x = 135; line_dsc.p2.y = 145;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.p1.x = 125; line_dsc.p1.y = 105;
    line_dsc.p2.x = 112; line_dsc.p2.y = 135;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.width = 1;
    line_dsc.p1.x = 145; line_dsc.p1.y = 125;
    line_dsc.p2.x = 155; line_dsc.p2.y = 138;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.p1.x = 135; line_dsc.p1.y = 145;
    line_dsc.p2.x = 142; line_dsc.p2.y = 160;
    lv_draw_line(&layer, &line_dsc);

    /* Left Lung Outer Outline */
    lv_point_precise_t left_lobe_pts[] = {
        {92, 45}, {60, 60}, {40, 95}, {35, 130}, {50, 168}, {88, 155}, {90, 115}
    };
    line_dsc.width = 2;
    line_dsc.color = lv_color_hex(0x007CFF);
    for(size_t i = 0; i < sizeof(left_lobe_pts)/sizeof(left_lobe_pts[0]) - 1; i++) {
        line_dsc.p1 = left_lobe_pts[i];
        line_dsc.p2 = left_lobe_pts[i+1];
        lv_draw_line(&layer, &line_dsc);
    }

    /* Right Lung Outer Outline */
    lv_point_precise_t right_lobe_pts[] = {
        {108, 45}, {140, 60}, {160, 95}, {165, 130}, {150, 168}, {112, 155}, {110, 115}
    };
    for(size_t i = 0; i < sizeof(right_lobe_pts)/sizeof(right_lobe_pts[0]) - 1; i++) {
        line_dsc.p1 = right_lobe_pts[i];
        line_dsc.p2 = right_lobe_pts[i+1];
        lv_draw_line(&layer, &line_dsc);
    }

    /* Bottom Particle Wave Dots */
    lv_draw_arc_dsc_t dot_dsc;
    lv_draw_arc_dsc_init(&dot_dsc);
    dot_dsc.color = COLOR_CYAN_ACCENT;
    dot_dsc.width = 2;

    int dot_coords[][2] = {
        {45, 178}, {58, 182}, {72, 179}, {85, 184}, {100, 180},
        {115, 184}, {128, 179}, {142, 182}, {155, 178}
    };
    for(size_t i = 0; i < sizeof(dot_coords)/sizeof(dot_coords[0]); i++) {
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = COLOR_CYAN_ACCENT;
        rect_dsc.bg_opa = LV_OPA_COVER;
        rect_dsc.radius = LV_RADIUS_CIRCLE;
        
        lv_area_t area;
        area.x1 = dot_coords[i][0] - 2;
        area.y1 = dot_coords[i][1] - 2;
        area.x2 = dot_coords[i][0] + 2;
        area.y2 = dot_coords[i][1] + 2;
        lv_draw_rect(&layer, &rect_dsc, &area);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

/* Timer Callback for Dynamic Boot Sequence Animation */
static void boot_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    boot_progress += 2;
    if(boot_progress >= 100) {
        boot_progress = 100;
        if(boot_anim_timer) {
            lv_timer_delete(boot_anim_timer);
            boot_anim_timer = NULL;
        }
        create_ventilator_main_screen();
        return;
    }

    if(boot_arc) {
        lv_arc_set_value(boot_arc, boot_progress);
    }
    if(boot_bar) {
        lv_bar_set_value(boot_bar, boot_progress, LV_ANIM_ON);
    }
    if(label_percentage) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", boot_progress);
        lv_label_set_text(label_percentage, buf);
    }

    if(label_status_footer) {
        if(boot_progress < 35) {
            lv_label_set_text(label_status_footer, "INITIALIZING MODULES  •  checking system  •  preparing for operation");
        } else if(boot_progress < 75) {
            lv_label_set_text(label_status_footer, "initializing modules  •  CHECKING SYSTEM  •  preparing for operation");
        } else {
            lv_label_set_text(label_status_footer, "initializing modules  •  checking system  •  PREPARING FOR OPERATION");
        }
    }

    /* Live USB Storage Hotplug Scan */
    static int usb_scan_counter = 0;
    if(++usb_scan_counter >= 10) { /* Check every ~800ms */
        usb_scan_counter = 0;
        usb_drive_info_t usb_info;
        if(usb_detect_check(&usb_info) && usb_info.is_connected) {
            if(label_usb_status) {
                char ubuf[64];
                snprintf(ubuf, sizeof(ubuf), "USB: %s", usb_info.dev_name);
                lv_label_set_text(label_usb_status, ubuf);
                lv_obj_set_style_text_color(label_usb_status, COLOR_CYAN_ACCENT, 0);
            }
        } else {
            if(label_usb_status) {
                lv_label_set_text(label_usb_status, "HW 1.3");
                lv_obj_set_style_text_color(label_usb_status, COLOR_TEXT_WHITE, 0);
            }
        }
    }
}

/* Helper to Create Hardware Info Column inside Bottom Card */
static void create_hardware_info_col(lv_obj_t * parent, const char * label_str, const char * val_str, int icon_type)
{
    lv_obj_t * col = lv_obj_create(parent);
    lv_obj_set_size(col, 260, 90);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Hardware Icon Box */
    lv_obj_t * icon_box = lv_obj_create(col);
    lv_obj_set_size(icon_box, 48, 48);
    lv_obj_set_style_bg_color(icon_box, lv_color_hex(0x061D3B), 0);
    lv_obj_set_style_border_color(icon_box, COLOR_CYAN_ACCENT, 0);
    lv_obj_set_style_border_width(icon_box, 1, 0);
    lv_obj_set_style_radius(icon_box, 10, 0);
    lv_obj_set_style_pad_all(icon_box, 0, 0);

    lv_obj_t * icon_lbl = lv_label_create(icon_box);
    lv_obj_center(icon_lbl);
    lv_obj_set_style_text_color(icon_lbl, COLOR_CYAN_ACCENT, 0);
    if(icon_type == 0) {
        lv_label_set_text(icon_lbl, LV_SYMBOL_SETTINGS);
    } else if(icon_type == 1) {
        lv_label_set_text(icon_lbl, LV_SYMBOL_SD_CARD);
    } else {
        lv_label_set_text(icon_lbl, LV_SYMBOL_USB);
    }

    /* Text Stack Container */
    lv_obj_t * text_box = lv_obj_create(col);
    lv_obj_set_size(text_box, 190, 70);
    lv_obj_set_style_bg_opa(text_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_box, 0, 0);
    lv_obj_set_style_pad_left(text_box, 15, 0);
    lv_obj_set_flex_flow(text_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t * lbl_title = lv_label_create(text_box);
    lv_label_set_text(lbl_title, label_str);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_letter_space(lbl_title, 2, 0);

    lv_obj_t * lbl_val = lv_label_create(text_box);
    lv_label_set_text(lbl_val, val_str);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_val, COLOR_TEXT_WHITE, 0);

    if(icon_type == 2) {
        label_usb_status = lbl_val;
    }
}

/**
 * @brief Create the complete Medical Ventilator System Boot UI
 */
void create_ventilator_boot_screen(void)
{
    boot_progress = 0;
    lv_obj_t * scr = lv_screen_active();
    lv_obj_clean(scr);

    /* Main Screen Background */
    //lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
    //lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

#if 0
    /* Background Soft Glowing Aura behind center */
    lv_obj_t * aura = lv_obj_create(scr);
    lv_obj_set_size(aura, 550, 550);
    lv_obj_center(aura);
    lv_obj_set_style_bg_color(aura, COLOR_AURA_GLOW, 0);
    lv_obj_set_style_bg_opa(aura, LV_OPA_30, 0);
    lv_obj_set_style_radius(aura, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(aura, 0, 0);
#endif
    /* ==================================================================== */
    /* 1. TOP HEADER SECTION (Branding)                                      */
    /* ==================================================================== */
    lv_obj_t * header_cont = lv_obj_create(scr);
    lv_obj_set_size(header_cont, 1920, 360);
    lv_obj_align(header_cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(header_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_cont, 0, 0);
    lv_obj_set_flex_flow(header_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(header_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Hospital Logo Badge Container */
    lv_obj_t * badge = lv_obj_create(header_cont);
    lv_obj_set_size(badge, 60, 60);
    lv_obj_set_style_bg_color(badge, COLOR_BADGE_BG, 0);
    lv_obj_set_style_border_color(badge, COLOR_CYAN_ACCENT, 0);
    lv_obj_set_style_border_width(badge, 2, 0);
    lv_obj_set_style_radius(badge, 16, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);

    /* Medical Cross Symbol inside badge */
    lv_obj_t * cross_lbl = lv_label_create(badge);
    lv_label_set_text(cross_lbl, "+");
    lv_obj_set_style_text_font(cross_lbl, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(cross_lbl, COLOR_TEXT_WHITE, 0);
    lv_obj_center(cross_lbl);

    /* "MEDICARE" */
    lv_obj_t * lbl_medicare = lv_label_create(header_cont);
    lv_label_set_text(lbl_medicare, "M E D I C A R E");
    lv_obj_set_style_text_font(lbl_medicare, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_medicare, COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_pad_top(lbl_medicare, 8, 0);

    /* "— HOSPITAL —" */
    lv_obj_t * lbl_hospital = lv_label_create(header_cont);
    lv_label_set_text(lbl_hospital, "H O S P I T A L");
    lv_obj_set_style_text_font(lbl_hospital, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_hospital, COLOR_TEXT_SUBTITLE, 0);
    lv_obj_set_style_pad_top(lbl_hospital, 2, 0);

    /* "CARE YOU CAN TRUST" */
    lv_obj_t * lbl_tagline = lv_label_create(header_cont);
    lv_label_set_text(lbl_tagline, "C A R E   Y O U   C A N   T R U S T");
    lv_obj_set_style_text_font(lbl_tagline, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_tagline, COLOR_TEXT_TAGLINE, 0);
    lv_obj_set_style_pad_top(lbl_tagline, 2, 0);

    /* Prominent Brand Title: "MEDIVENT™" */
    lv_obj_t * medivent_cont = lv_obj_create(header_cont);
    lv_obj_set_size(medivent_cont, 400, 60);
    lv_obj_set_style_bg_opa(medivent_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(medivent_cont, 0, 0);
    lv_obj_set_style_pad_top(medivent_cont, 15, 0);
    lv_obj_set_flex_flow(medivent_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(medivent_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
#if 0
    lv_obj_t * lbl_medi = lv_label_create(medivent_cont);
    lv_label_set_text(lbl_medi, "MEDI");
    lv_obj_set_style_text_font(lbl_medi, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_medi, COLOR_TEXT_WHITE, 0);

    lv_obj_t * lbl_vent = lv_label_create(medivent_cont);
    lv_label_set_text(lbl_vent, "VENT™");
    lv_obj_set_style_text_font(lbl_vent, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_vent, COLOR_CYAN_ACCENT, 0);
#endif
    /* Cyan Divider Line under MEDIVENT */
    lv_obj_t * line_bar = lv_obj_create(header_cont);
    lv_obj_set_size(line_bar, 480, 2);
    lv_obj_set_style_bg_color(line_bar, COLOR_CYAN_ACCENT, 0);
    lv_obj_set_style_bg_opa(line_bar, LV_OPA_60, 0);
    lv_obj_set_style_border_width(line_bar, 0, 0);
    lv_obj_set_style_margin_top(line_bar, 5, 0);

    /* "ADVANCED VENTILATOR SYSTEM" */
    lv_obj_t * lbl_adv = lv_label_create(header_cont);
    lv_label_set_text(lbl_adv, "A D V A N C E D   V E N T I L A T O R   S Y S T E M");
    lv_obj_set_style_text_font(lbl_adv, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_adv, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_pad_top(lbl_adv, 6, 0);

    /* ==================================================================== */
    /* 2. CENTER SECTION (Loading GIF, Booting Label, Bar, Percentage)       */
    /* ==================================================================== */
    
    /* Loading GIF Animation from Images/loading.gif */
    const char * gif_path = "A:Images/loading.gif";
    FILE * f_chk = fopen("Images/loading.gif", "rb");
    if(!f_chk) {
        f_chk = fopen("../Images/loading.gif", "rb");
        if(f_chk) {
            gif_path = "A:../Images/loading.gif";
        }
    }
    if(f_chk) fclose(f_chk);

    lv_obj_t * gif_anim = lv_gif_create(scr);
    lv_gif_set_src(gif_anim, gif_path);
    lv_obj_set_size(gif_anim, 600, 400);
    lv_obj_align(gif_anim, LV_ALIGN_CENTER, 0, 30);
    boot_arc = NULL; /* No longer using arc indicator */

    /* "SYSTEM BOOTING..." Label */
    lv_obj_t * lbl_booting = lv_label_create(scr);
    lv_label_set_text(lbl_booting, "S Y S T E M   B O O T I N G . . .");
    lv_obj_set_style_text_font(lbl_booting, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_booting, lv_color_hex(0xD4EEFF), 0);
    lv_obj_align(lbl_booting, LV_ALIGN_CENTER, 0, 230);

    /* Progress Bar */
    boot_bar = lv_bar_create(scr);
    lv_obj_set_size(boot_bar, 540, 8);
    lv_obj_align(boot_bar, LV_ALIGN_CENTER, 0, 275);
    lv_bar_set_range(boot_bar, 0, 100);
    lv_bar_set_value(boot_bar, boot_progress, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(boot_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(boot_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(boot_bar, COLOR_CYAN_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(boot_bar, 4, LV_PART_INDICATOR);

    /* Percentage Label: "72%" */
    label_percentage = lv_label_create(scr);
    char pct_buf[16];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", boot_progress);
    lv_label_set_text(label_percentage, pct_buf);
    lv_obj_set_style_text_font(label_percentage, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_percentage, COLOR_TEXT_MUTED, 0);
    lv_obj_align(label_percentage, LV_ALIGN_CENTER, 0, 305);

    /* ==================================================================== */
    /* 3. BOTTOM SYSTEM INFO CARD (Version, Firmware, Hardware)             */
    /* ==================================================================== */
#if 0
    lv_obj_t * info_card = lv_obj_create(scr);
    lv_obj_set_size(info_card, 860, 100); // 960,120
    lv_obj_align(info_card, LV_ALIGN_BOTTOM_MID, 0, -110);
    lv_obj_set_style_bg_color(info_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(info_card, LV_OPA_90, 0);
    lv_obj_set_style_border_color(info_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(info_card, 2, 0);
    lv_obj_set_style_radius(info_card, 20, 0);
    lv_obj_set_flex_flow(info_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(info_card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Column 1: VERSION */
    create_hardware_info_col(info_card, "VERSION", "2.1.0", 0);

    /* Separator 1 */
    lv_obj_t * sep1 = lv_obj_create(info_card);
    lv_obj_set_size(sep1, 2, 60);
    lv_obj_set_style_bg_color(sep1, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);

    /* Column 2: FIRMWARE */
    create_hardware_info_col(info_card, "FIRMWARE", "FW 2.1.0.45", 1);

    /* Separator 2 */
    lv_obj_t * sep2 = lv_obj_create(info_card);
    lv_obj_set_size(sep2, 2, 60);
    lv_obj_set_style_bg_color(sep2, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);

    /* Column 3: HARDWARE */
    create_hardware_info_col(info_card, "HARDWARE", "HW 1.3", 2);

#endif
    /* ==================================================================== */
    /* 4. BOTTOM STATUS FOOTER                                              */
    /* ==================================================================== */
    label_status_footer = lv_label_create(scr);
    lv_label_set_text(label_status_footer, "INITIALIZING MODULES  •  CHECKING SYSTEM  •  PREPARING FOR OPERATION");
    lv_obj_set_style_text_font(label_status_footer, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_status_footer, COLOR_TEXT_TAGLINE, 0);
    lv_obj_set_style_text_letter_space(label_status_footer, 2, 0);
    lv_obj_align(label_status_footer, LV_ALIGN_BOTTOM_MID, 0, -45);
    
    /* Disable scrolling on all elements in the boot screen tree */
    extern void disable_scroll_recursive(lv_obj_t * obj);
    disable_scroll_recursive(scr);

    /* Create Timer for Dynamic Progress & Arc Animation */
    boot_anim_timer = lv_timer_create(boot_timer_cb, 80, NULL);
}

void create_ventilator_ui(void)
{
    create_ventilator_boot_screen();
}
