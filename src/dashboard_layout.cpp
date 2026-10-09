#include <lvgl.h>

#ifdef PC_SIMULATOR

// Global object handles so they can be read by both loops
lv_obj_t *rpm_bar = nullptr;
lv_obj_t *rpm_value = nullptr;
lv_obj_t *can_status_label = nullptr;
lv_obj_t *abs_mode_label = nullptr;
lv_obj_t *side_stand_label = nullptr;
lv_obj_t *simulator_fuel = nullptr;
lv_obj_t *simulator_fuel_bar = nullptr;
lv_obj_t *simulator_fuel_fill = nullptr;

LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);

static void simulator_fuel_reset_event_cb(lv_event_t *event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        lv_label_set_text(simulator_fuel, "3.43 gal");
        lv_bar_set_value(simulator_fuel_bar, 99, LV_ANIM_OFF);
        lv_obj_set_width(simulator_fuel_fill, 294);
    }
}

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

    static lv_style_t style_fuel_bar;
    lv_style_init(&style_fuel_bar);
    lv_style_set_bg_color(&style_fuel_bar, lv_color_hex(0x17354B));
    lv_style_set_bg_opa(&style_fuel_bar, LV_OPA_COVER);
    lv_style_set_border_width(&style_fuel_bar, 3);
    lv_style_set_border_color(&style_fuel_bar, lv_color_hex(0xF4F7FA));
    lv_style_set_radius(&style_fuel_bar, 4);

    static lv_style_t style_fuel_indicator;
    lv_style_init(&style_fuel_indicator);
    lv_style_set_bg_color(&style_fuel_indicator, lv_color_hex(0x65D6FF));
    lv_style_set_bg_opa(&style_fuel_indicator, LV_OPA_COVER);
    lv_style_set_radius(&style_fuel_indicator, 1);

    // UI Elements Construction
    simulator_fuel = lv_label_create(screen);
    lv_label_set_text(simulator_fuel, "2.85 gal");
    lv_obj_add_style(simulator_fuel, &style_large_metric, 0);
    lv_obj_align(simulator_fuel, LV_ALIGN_CENTER, 240, -175);

    simulator_fuel_bar = lv_bar_create(screen);
    lv_obj_set_size(simulator_fuel_bar, 300, 38);
    lv_obj_align(simulator_fuel_bar, LV_ALIGN_CENTER, -220, -175);
    lv_bar_set_range(simulator_fuel_bar, 0, 100);
    lv_bar_set_value(simulator_fuel_bar, 58, LV_ANIM_OFF);
    lv_obj_add_style(simulator_fuel_bar, &style_fuel_bar, LV_PART_MAIN);
    lv_obj_add_style(simulator_fuel_bar, &style_fuel_indicator, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(simulator_fuel_bar, LV_OPA_TRANSP, LV_PART_INDICATOR);

    simulator_fuel_fill = lv_obj_create(screen);
    lv_obj_set_size(simulator_fuel_fill, 171, 30);
    lv_obj_align_to(simulator_fuel_fill, simulator_fuel_bar, LV_ALIGN_TOP_LEFT, 1, 1);
    lv_obj_set_style_bg_color(simulator_fuel_fill, lv_color_hex(0x65D6FF), 0);
    lv_obj_set_style_bg_opa(simulator_fuel_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(simulator_fuel_fill, 0, 0);
    lv_obj_set_style_radius(simulator_fuel_fill, 1, 0);

    lv_obj_t *fuel_markers = lv_label_create(screen);
    lv_label_set_text(fuel_markers, "E       1/4      1/2      3/4       F");
    lv_obj_add_style(fuel_markers, &style_sub_header, 0);
    lv_obj_align(fuel_markers, LV_ALIGN_CENTER, -220, -135);

    lv_obj_t *temperature = lv_label_create(screen);
    lv_label_set_text(temperature, "Coolant Temperature: 180 F");
    lv_obj_add_style(temperature, &style_large_metric, 0); 
    lv_obj_align(temperature, LV_ALIGN_CENTER, 20, -100);

    lv_obj_t *gear = lv_label_create(screen);
    lv_label_set_text(gear, "Gear: N");
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
    lv_label_set_text(economy, "Fuel Economy: 48.2 MPG");
    lv_obj_add_style(economy, &style_sub_header, 0); 
    lv_obj_align(economy, LV_ALIGN_CENTER, 0, 115);

    lv_obj_t *range = lv_label_create(screen);
    lv_label_set_text(range, "Range: 163.8 mi");
    lv_obj_add_style(range, &style_sub_header, 0);
    lv_obj_align(range, LV_ALIGN_CENTER, 0, 155);

    lv_obj_t *reset_button = lv_button_create(screen);
    lv_obj_set_size(reset_button, 180, 44);
    lv_obj_align(reset_button, LV_ALIGN_CENTER, 30, -175);
    lv_obj_add_event_cb(reset_button, simulator_fuel_reset_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *reset_label = lv_label_create(reset_button);
    lv_label_set_text(reset_label, "Reset");
    lv_obj_add_style(reset_label, &style_sub_header, 0);
    lv_obj_center(reset_label);

    can_status_label = lv_label_create(screen);
    lv_label_set_text(can_status_label, "CAN");
    lv_obj_add_style(can_status_label, &style_sub_header, 0);
    lv_obj_set_style_text_color(can_status_label, lv_color_hex(0x2ECC71), 0);
    lv_obj_align(can_status_label, LV_ALIGN_BOTTOM_RIGHT, -20, -15);

    abs_mode_label = lv_label_create(screen);
    lv_label_set_text(abs_mode_label, "ABS: ON");
    lv_obj_add_style(abs_mode_label, &style_sub_header, 0);
    lv_obj_align(abs_mode_label, LV_ALIGN_BOTTOM_LEFT, 20, -15);

    side_stand_label = lv_label_create(screen);
    lv_label_set_text(side_stand_label, "STAND: UP");
    lv_obj_add_style(side_stand_label, &style_sub_header, 0);
    lv_obj_set_style_text_color(side_stand_label, lv_color_hex(0x2ECC71), 0);
    lv_obj_align(side_stand_label, LV_ALIGN_BOTTOM_LEFT, 20, -55);
}

#endif
