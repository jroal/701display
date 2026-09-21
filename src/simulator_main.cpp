#include <SDL2/SDL.h>

#include <lvgl.h>

#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

// Explicitly pull the verified multi-size assets from the core repository maps
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);


namespace {
constexpr int DISPLAY_WIDTH = 800;
constexpr int DISPLAY_HEIGHT = 480;
constexpr int BUFFER_LINES = 40;

// --- Engine Speed Warning Thresholds ---
constexpr int RPM_PEAK_POWER = 8000;    // Changes bar to Yellow
constexpr int RPM_REDLINE = 9000;       // Changes bar to Red

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
uint8_t draw_buffer[DISPLAY_WIDTH * BUFFER_LINES * 2];
lv_obj_t *rpm_bar = nullptr;
lv_obj_t *rpm_value = nullptr;

void flush_display(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map) {
    const int width = lv_area_get_width(area);
    const int height = lv_area_get_height(area);

    SDL_Rect destination{
        area->x1,
        area->y1,
        width,
        height,
    };
    SDL_UpdateTexture(texture, &destination, pixel_map, width * 2);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    lv_display_flush_ready(display);
}

// --- 2. RENDER THE HIGH-VISIBILITY INSTRUMENTATION WINDOW ---
void create_dashboard() {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xF4F7FA), 0);

    // ---------------------------------------------------------------------
    // 🧱 INITIALIZE LOCAL HIGH-VISIBILITY DASHBOARD STYLE SHEETS
    // ---------------------------------------------------------------------
    static lv_style_t style_sub_header;
    lv_style_init(&style_sub_header);
    lv_style_set_text_font(&style_sub_header, &lv_font_montserrat_24); // Use verified standard 24px
    lv_style_set_text_color(&style_sub_header, lv_color_hex(0x65D6FF));

    static lv_style_t style_large_metric;
    lv_style_init(&style_large_metric);
    lv_style_set_text_font(&style_large_metric, &lv_font_montserrat_32); // Use verified standard 32px
    lv_style_set_text_color(&style_large_metric, lv_color_hex(0xF4F7FA));

    static lv_style_t style_giant_gear;
    lv_style_init(&style_giant_gear);
    lv_style_set_text_font(&style_giant_gear, &lv_font_montserrat_48); // Use verified standard 48px
    lv_style_set_text_color(&style_giant_gear, lv_color_hex(0xFFD166));

    static lv_style_t style_bold_footer;
    lv_style_init(&style_bold_footer);
    lv_style_set_text_font(&style_bold_footer, &lv_font_montserrat_24); // Use verified standard 24px
    lv_style_set_text_color(&style_bold_footer, lv_color_hex(0xF4F7FA));

    // ---------------------------------------------------------------------
    // 🏷️ APPLY CODES DIRECTLY TO SCREEN INDICES
    // ---------------------------------------------------------------------
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "701 ENDURO  |  SIMULATOR");
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

    // RPM Bar Layout Construction
    rpm_bar = lv_bar_create(screen);
    lv_obj_set_size(rpm_bar, 620, 34);
    lv_obj_align(rpm_bar, LV_ALIGN_CENTER, 0, 25);
    lv_bar_set_range(rpm_bar, 0, 10000);
    lv_bar_set_value(rpm_bar, 3200, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0x17354B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0x2EA3F2), LV_PART_INDICATOR);

    rpm_value = lv_label_create(screen);
    lv_label_set_text(rpm_value, "3200 RPM");
    lv_obj_add_style(rpm_value, &style_sub_header, 0); 
    lv_obj_align(rpm_value, LV_ALIGN_CENTER, 0, 65);

    lv_obj_t *economy = lv_label_create(screen);
    lv_label_set_text(economy, "ECONOMY: 48.2 MPG");
    lv_obj_add_style(economy, &style_bold_footer, 0); 
    lv_obj_align(economy, LV_ALIGN_CENTER, 0, 115);
}



}

int main(int, char **) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }

    window = SDL_CreateWindow(
        "701 Display Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT);

    if (window == nullptr || renderer == nullptr || texture == nullptr) {
        SDL_Quit();
        return 1;
    }

    lv_init();
    lv_display_t *display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(
        display,
        draw_buffer,
        nullptr,
        sizeof(draw_buffer),
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_display);
    create_dashboard();

    bool running = true;
    uint32_t last_tick = SDL_GetTicks();
    uint32_t last_update = last_tick;
    int rpm = 3200;
    int rpm_step = 80;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - last_tick);
        last_tick = now;

        if (now - last_update >= 50) {
            rpm += rpm_step;
            if (rpm >= 7800 || rpm <= 1800) {
                rpm_step = -rpm_step;
            }
            lv_bar_set_value(rpm_bar, rpm, LV_ANIM_OFF);
            lv_label_set_text_fmt(rpm_value, "%d RPM", rpm);
            last_update = now;
        }

        lv_timer_handler();
        SDL_Delay(5);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
#endif