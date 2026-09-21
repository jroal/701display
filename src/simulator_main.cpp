#include <SDL2/SDL.h>
#include <lvgl.h>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

// --- Declare Standard Large Fonts (Guaranteed to Exist) ---
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);

// Global declaration allows the file to resolve definitions outside namespace boundaries
void create_dashboard();

// --- 🌐 USE EXTERN TO BIND TO EXISTING TRACKS & STOP MULTIPLE DEFINITIONS ---
extern lv_obj_t *rpm_bar;
extern lv_obj_t *rpm_value;

namespace {
constexpr int DISPLAY_WIDTH = 800;
constexpr int DISPLAY_HEIGHT = 480;
constexpr int BUFFER_LINES = 40;

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
uint8_t draw_buffer[DISPLAY_WIDTH * BUFFER_LINES * 2];
lv_point_t mouse_position{0, 0};
bool mouse_pressed = false;

void read_mouse(lv_indev_t *, lv_indev_data_t *data) {
    data->point = mouse_position;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

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
} // Namespace ends here cleanly before function setups execute

// =========================================================================
// 🚀 MAIN EMULATOR PROGRAM EXECUTION
// =========================================================================
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

    lv_indev_t *mouse = lv_indev_create();
    lv_indev_set_type(mouse, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse, read_mouse);
    
    // Calls the clean global dashboard creation process
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
            } else if (event.type == SDL_MOUSEMOTION) {
                mouse_position.x = event.motion.x;
                mouse_position.y = event.motion.y;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                mouse_position.x = event.button.x;
                mouse_position.y = event.button.y;
                mouse_pressed = true;
            } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                mouse_position.x = event.button.x;
                mouse_position.y = event.button.y;
                mouse_pressed = false;
            }
        }

        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - last_tick);
        last_tick = now;

        if (now - last_update >= 50) {
            rpm += rpm_step;
            if (rpm >= 10000 || rpm <= 1200) {
                rpm_step = -rpm_step;
            }
            
            // Updates metrics safely across your top progress indicator gauge
            lv_bar_set_value(rpm_bar, rpm, LV_ANIM_OFF);
            lv_label_set_text_fmt(rpm_value, "%d RPM", rpm);
            
            // Dynamic progressive coloration shifts map natively here
            if (rpm < 8000) {
                lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0x2ECC71), LV_PART_INDICATOR);
            } else if (rpm < 9000) {
                lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0xF1C40F), LV_PART_INDICATOR);
            } else {
                lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0xE74C3C), LV_PART_INDICATOR);
            }
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
