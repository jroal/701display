#include <lvgl.h>

// Global object handles so they can be read by both loops
lv_obj_t *rpm_bar = nullptr;
lv_obj_t *rpm_value = nullptr;

LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);

void create_dashboard() {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xF4F7FA), 0);

    // Initialize local high-visibility style sheets
    static lv_style_t style_sub_header;
    lv_style_init(&style_sub_header);
    lv_style_set_text_font(&style_sub_header, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_sub_header, lv_color_hex(0x65D6FF));

    static lv_style_t style_large_metric;
    lv_style_init(&style_large_metric);
    lv_style_set_text_font(&style_large_metric, &lv_font_montserrat_32);

    static lv_style_t style_giant_gear;
    lv_style_init(&style_giant_gear);
    lv_style_set_text_font(&style_giant_gear, &lv_font_montserrat_48);
    lv_style_set_text_color(&style_giant_gear, lv_color_hex(0xFFD166));

    // UI Elements Construction
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "701 ENDURO  |  DASHBOARD");
    lv_obj_add_style(title, &style_sub_header, 0); 
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *fuel = lv_label_create(screen);
    lv_label_set_text(fuel, "FUEL: 12.80 L");
    lv_obj_add_style(fuel, &style_large_metric, 0); 
    lv_obj_align(fuel, LV_ALIGN_CENTER, -220, -100); 

    lv_obj_t *temperature = lv_label_create(screen);
    lv_label_set_text(temperature, "TEMP: 82 C");
    lv_obj_add_style(temperature, &style_large_metric, 0); 
    lv_obj_align(temperature, LV_ALIGN_CENTER, 220, -100);

    lv_obj_t *gear = lv_label_create(screen);
    lv_label_set_text(gear, "GEAR: N");
    lv_obj_add_style(gear, &style_giant_gear, 0); 
    lv_obj_align(gear, LV_ALIGN_CENTER, 0, -50);

    rpm_bar = lv_bar_create(screen);
    lv_obj_set_size(rpm_bar, 620, 34);
    lv_obj_align(rpm_bar, LV_ALIGN_CENTER, 0, 25);
    lv_bar_set_range(rpm_bar, 0, 10000);
    lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0x17354B), LV_PART_MAIN);

    rpm_value = lv_label_create(screen);
    lv_label_set_text(rpm_value, "3200 RPM");
    lv_obj_add_style(rpm_value, &style_sub_header, 0); 
    lv_obj_align(rpm_value, LV_ALIGN_CENTER, 0, 65);

    lv_obj_t *economy = lv_label_create(screen);
    lv_label_set_text(economy, "ECONOMY: 48.2 MPG");
    lv_obj_add_style(economy, &style_sub_header, 0); 
    lv_obj_align(economy, LV_ALIGN_CENTER, 0, 115);
}
