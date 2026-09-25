#include <Arduino.h>

// --- Identify and Isolate Environment Framework Toolchains ---
#ifdef PC_SIMULATOR
    // Native Desktop Windows Emulation System Drivers
    #include <SDL2/SDL.h>
    void hal_init(void); 
#else
    // Native Physical Micro-Controller Hardware Drivers
    #include <driver/twai.h> // ESP32 native CAN driver
    #include <driver/i2c.h>
    #include <Preferences.h> // Non-volatile flash storage management
    #include <ESP_Panel_Library.h>
    #include <esp_log.h>
    #include <esp_heap_caps.h>
#endif

// Support both standard project directory formats cleanly
#if __has_include(<lvgl.h>)
    #include <lvgl.h>
#elif __has_include(<lvgl/lvgl.h>)
    #include <lvgl/lvgl.h>
#else
    #error "LVGL is not installed or its include directory is not configured"
#endif

// --- System Configuration Constants ---
const float TOTAL_FUEL_CAPACITY = 13;            // 2023 US Husqvarna 701 usable Liters
const unsigned long UDS_POLL_INTERVAL_MS = 500;
const uint16_t ENGINE_RUN_RPM_THRESHOLD = 500;      // RPM above which the engine is considered running

// --- Thread-Safe Multi-Core Structures ---
SemaphoreHandle_t dataMutex;

struct BikeMetrics {
    float fuelRemaining = 5;
    float fuelConsumed = 0.0;
    float distanceTraveledKm = 0.0; // Cumulative distance track since last fill-up
    float currentMPG = 0.0;         // Calculated average fuel efficiency
    uint16_t engineRPM = 0;
    int16_t coolantTemp = 0;
    uint8_t currentGear = 0;
    bool fuelResetTriggered = false;
    bool canActive = false;
    uint8_t absModeStatus = 0;
};
volatile BikeMetrics liveData;

// --- Task & Flash Memory Handlers ---
#ifndef PC_SIMULATOR
    TaskHandle_t CANTaskHandle = NULL;
    TaskHandle_t UITaskHandle = NULL;
    Preferences nvStorage;
    float fuelConsumedAtStartup = 0.0f;
#endif

// --- LVGL Graphical Object Pointers (Core 1) ---
lv_obj_t * fuel_label;
lv_obj_t * fuel_bar;
lv_obj_t * fuel_percent_label;
lv_obj_t * temp_label;
lv_obj_t * gear_label;
lv_obj_t * reset_btn;
lv_obj_t * mpg_label;
lv_obj_t * range_label;
lv_obj_t * rpm_bar;
lv_obj_t * rpm_value;
lv_obj_t * can_status_label;
lv_obj_t * abs_mode_label;

#ifndef PC_SIMULATOR
ESP_Panel hardwarePanel;
ESP_PanelLcd *hardwareLcd = nullptr;
ESP_PanelLcdTouch *hardwareTouch = nullptr;
lv_indev_t *hardwareTouchIndev = nullptr;
lv_display_t *hardwareDisplay = nullptr;
static uint8_t *lvglFrameBuffer = nullptr;
static uint8_t *lvglFrameBuffer2 = nullptr;
static uint8_t flushLogCount = 0;

// Waveshare board's CH422G I/O expander controls TP_RST/LCD_RST/backlight over I2C.
static const uint8_t CH422G_IO_WRITE_ADDRESS = 0x38;
TaskHandle_t TouchTaskHandle = NULL;

// Touch is polled from its own task on core 0, away from the RGB LCD flush on core 1 -
// the ESP32-S3's RGB panel DMA is sensitive to bus contention from I2C activity on the same core.
SemaphoreHandle_t touchMutex;
volatile bool touchPressedShared = false;
volatile uint16_t touchXShared = 0;
volatile uint16_t touchYShared = 0;

static void hardware_touch_read_cb(lv_indev_t *, lv_indev_data_t *data) {
    xSemaphoreTake(touchMutex, portMAX_DELAY);
    bool pressed = touchPressedShared;
    uint16_t x = touchXShared;
    uint16_t y = touchYShared;
    xSemaphoreGive(touchMutex);

    if (pressed) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void TouchProcessingLoop(void *) {
    while (true) {
        if (hardwareTouch != nullptr) {
            hardwareTouch->readData();
            bool pressed = hardwareTouch->getTouchState();
            TouchPoint point = pressed ? hardwareTouch->getPoint() : TouchPoint();

            xSemaphoreTake(touchMutex, portMAX_DELAY);
            touchPressedShared = pressed;
            touchXShared = point.x;
            touchYShared = point.y;
            xSemaphoreGive(touchMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Brings up the CH422G expander's I2C bus and releases TP_RST/LCD_RST (driven high here)
// before the touch/LCD peripherals are touched - GT911 stays unresponsive if this runs late.
static bool expander_init() {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = ESP_PANEL_LCD_TOUCH_I2C_IO_SDA;
    conf.scl_io_num = ESP_PANEL_LCD_TOUCH_I2C_IO_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    if (i2c_param_config(I2C_NUM_0, &conf) != ESP_OK || i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0) != ESP_OK) {
        Serial.println("EXPANDER: I2C bus initialization failed");
        return false;
    }

    uint8_t outputLevels = 0xFF;
    if (i2c_master_write_to_device(I2C_NUM_0, CH422G_IO_WRITE_ADDRESS, &outputLevels,
                                   sizeof(outputLevels), pdMS_TO_TICKS(10)) != ESP_OK) {
        Serial.println("EXPANDER: output level write failed");
        return false;
    }
    return true;
}

static void hardware_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap) {
    if (flushLogCount < 3) {
        Serial.printf("DISPLAY: flush lcd=%p %d,%d to %d,%d\n", hardwareLcd, area->x1, area->y1, area->x2 + 1, area->y2 + 1);
        flushLogCount++;
    }
    if (hardwareLcd != nullptr) {
        hardwareLcd->drawBitmap(
            area->x1,
            area->y1,
            area->x2 + 1,
            area->y2 + 1,
            pixelMap);
    }
    lv_display_flush_ready(display);
}

static bool hardware_display_init() {
    constexpr size_t BUFFER_LINES = 40;

    Serial.println("DISPLAY: panel init");
    hardwarePanel.init();
    Serial.println("DISPLAY: panel begin");
    hardwarePanel.begin();
    Serial.println("DISPLAY: panel ready");
    Serial.println("DISPLAY: get LCD");
    hardwareLcd = hardwarePanel.getLcd();
    Serial.println("DISPLAY: got LCD");
    if (hardwareLcd == nullptr) {
        Serial.println("LCD initialization failed");
        return false;
    }

    Serial.println("DISPLAY: LVGL init");
    Serial.flush();
    lv_init();
    Serial.println("DISPLAY: LVGL initialized");
    Serial.flush();
    hardwareDisplay = lv_display_create(800, 480);
    Serial.println("DISPLAY: display created");
    Serial.flush();
    lv_display_set_color_format(hardwareDisplay, LV_COLOR_FORMAT_RGB565);
    lvglFrameBuffer = static_cast<uint8_t *>(
        heap_caps_malloc(800 * BUFFER_LINES * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    lvglFrameBuffer2 = static_cast<uint8_t *>(
        heap_caps_malloc(800 * BUFFER_LINES * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (lvglFrameBuffer == nullptr || lvglFrameBuffer2 == nullptr) {
        Serial.println("DISPLAY: partial-frame buffer allocation failed");
        return false;
    }
    lv_display_set_buffers(
        hardwareDisplay,
        lvglFrameBuffer,
        lvglFrameBuffer2,
        800 * BUFFER_LINES * 2,
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(hardwareDisplay, hardware_flush_cb);
    lv_display_set_default(hardwareDisplay);

    hardwareTouch = hardwarePanel.getLcdTouch();
    if (hardwareTouch == nullptr) {
        Serial.println("DISPLAY: touch initialization failed, screen will be view-only");
    } else {
        hardwareTouchIndev = lv_indev_create();
        lv_indev_set_type(hardwareTouchIndev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(hardwareTouchIndev, hardware_touch_read_cb);
    }
    Serial.printf("DISPLAY: state cf=%d mode=%d buf=%u timer=%p\n",
                  static_cast<int>(lv_display_get_color_format(hardwareDisplay)),
                  static_cast<int>(lv_display_get_render_mode(hardwareDisplay)),
                  static_cast<unsigned>(lv_display_get_draw_buf_size(hardwareDisplay)),
                  lv_display_get_refr_timer(hardwareDisplay));
    Serial.println("DISPLAY: LVGL ready");
    return true;
}
#endif

// --- Tachometer Configuration Metrics ---
const int32_t RPM_MAX_SCALE = 10000;    // 1,000 RPM over the 9,000 redline limit
const int32_t RPM_PEAK_POWER = 8000;    // Yellow zone entry threshold
const int32_t RPM_REDLINE = 9000;       // Red zone entry threshold

// --- Explicitly Declare Verified Lowercase Core Fonts ---
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);

void CANProcessingLoop(void * pvParameters);
void UIProcessingLoop(void * pvParameters);
void UI_Layout_Init();
// =========================================================================
// 📡 CORE 0: VEHICLE DATA PROCESSING & UDS DIAGNOSTIC POLLING
// =========================================================================
#ifndef PC_SIMULATOR
void sendUDSPIDRequest(uint8_t pid) {
    twai_message_t tx_msg;
    tx_msg.identifier = 0x7DF;       
    tx_msg.extd = 0;                 
    tx_msg.rtr = 0;                  
    tx_msg.data_length_code = 8;     
    
    tx_msg.data[0] = 0x02;           
    tx_msg.data[1] = 0x01;           
    tx_msg.data[2] = pid;            
    for (int i = 3; i < 8; i++) tx_msg.data[i] = 0xAA; 

    twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
}

static void persistFuelDataIfChanged(float consumed, float distance) {
    nvStorage.begin("bike-fuel", false);
    float storedConsumed = nvStorage.getFloat("consumed", 0.0f);
    float storedDistance = nvStorage.getFloat("distance", 0.0f);

    if (storedConsumed != consumed) {
        nvStorage.putFloat("consumed", consumed);
    }
    if (storedDistance != distance) {
        nvStorage.putFloat("distance", distance);
    }
    nvStorage.end();
}

void CANProcessingLoop(void * pvParameters) {
    unsigned long lastUDSPoll = 0;
    unsigned long lastTimeCheck = millis();
    unsigned long lastFuelRateCheck = 0;
    unsigned long lastValidCanResponse = 0;
    uint8_t pollIndex = 0;
    bool canWasActiveThisBoot = false;
    bool fuelLossWriteComplete = false;

    while(true) {
        unsigned long currentMillis = millis();
        twai_message_t rx_msg;

        if (currentMillis - lastUDSPoll >= UDS_POLL_INTERVAL_MS) {
            const uint8_t polledPids[] = {0x05, 0x0D, 0x5E};
            sendUDSPIDRequest(polledPids[pollIndex]);
            pollIndex = (pollIndex + 1) % 3;
            lastUDSPoll = currentMillis;
        }

        if (twai_receive(&rx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);

            if (rx_msg.identifier == 0x130 || rx_msg.identifier == 0x136) {
                uint8_t absByte = rx_msg.data[1];
                if ((absByte & 0x0C) == 0x0C) {
                    liveData.absModeStatus = 2;
                }
                else if ((absByte & 0x04) == 0x04) {
                    liveData.absModeStatus = 1;
                }
                else {
                    liveData.absModeStatus = 0;
                }
            }
            else if (rx_msg.identifier == 0x7E8 && rx_msg.data[1] == 0x41) {
                canWasActiveThisBoot = true;
                lastValidCanResponse = millis();
                fuelLossWriteComplete = false;
                liveData.canActive = true;

                if (rx_msg.data[2] == 0x05) {
                    liveData.coolantTemp = (int16_t)rx_msg.data[3] - 40;
                }
                else if (rx_msg.data[2] == 0x0D) {
                    uint8_t currentSpeedKmh = rx_msg.data[3]; 
                    
                    unsigned long currentTime = millis();
                    float deltaTimeHours = (currentTime - lastTimeCheck) / 3600000.0;
                    lastTimeCheck = currentTime;

                    if (currentSpeedKmh > 0) {
                        liveData.distanceTraveledKm += (float)currentSpeedKmh * deltaTimeHours;
                    }

                    if (liveData.fuelConsumed > 0.001) {
                        float miles = liveData.distanceTraveledKm * 0.621371;
                        float gallons = liveData.fuelConsumed * 0.264172;
                        liveData.currentMPG = miles / gallons;
                    }
                }
                else if (rx_msg.data[2] == 0x5E) {
                    unsigned long currentTime = millis();
                    if (lastFuelRateCheck != 0) {
                        float deltaFuelHours = (currentTime - lastFuelRateCheck) / 3600000.0;
                        float fuelRateLitersPerHour =
                            ((rx_msg.data[3] * 256.0f) + rx_msg.data[4]) / 20.0f;
                        liveData.fuelConsumed += fuelRateLitersPerHour * deltaFuelHours;
                        liveData.fuelRemaining = TOTAL_FUEL_CAPACITY - liveData.fuelConsumed;
                        if (liveData.fuelRemaining < 0.0f) {
                            liveData.fuelRemaining = 0.0f;
                        }
                    }
                    lastFuelRateCheck = currentTime;

                    if (liveData.fuelConsumed > 0.001) {
                        float miles = liveData.distanceTraveledKm * 0.621371;
                        float gallons = liveData.fuelConsumed * 0.264172;
                        liveData.currentMPG = miles / gallons;
                    }
                }
            }
            xSemaphoreGive(dataMutex);
        }

        if (canWasActiveThisBoot && !fuelLossWriteComplete &&
            millis() - lastValidCanResponse >= 2500) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            float currentConsumed = liveData.fuelConsumed;
            float currentDistance = liveData.distanceTraveledKm;
            xSemaphoreGive(dataMutex);

            if (currentConsumed > fuelConsumedAtStartup + 0.001f) {
                persistFuelDataIfChanged(currentConsumed, currentDistance);
            }
            fuelLossWriteComplete = true;
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            liveData.canActive = false;
            xSemaphoreGive(dataMutex);
        }

        if (liveData.fuelResetTriggered) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            liveData.fuelConsumed = 0.0;
            liveData.fuelRemaining = TOTAL_FUEL_CAPACITY;
            liveData.distanceTraveledKm = 0.0; 
            liveData.currentMPG = 0.0;
            liveData.fuelResetTriggered = false;

            persistFuelDataIfChanged(0.0f, 0.0f);
            xSemaphoreGive(dataMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
#endif

static void fuel_reset_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        liveData.fuelResetTriggered = true;
    }
}
// =========================================================================
// 🎨 CORE 1: GRAPHICS UI INTERACTION AND ENGINE INTERACTION LAYER
// =========================================================================
void UI_Layout_Init() {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xF4F7FA), 0);

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

    rpm_bar = lv_bar_create(screen);
    lv_obj_set_size(rpm_bar, 620, 34);                  
    lv_obj_align(rpm_bar, LV_ALIGN_CENTER, 0, 25);      
    lv_bar_set_range(rpm_bar, 0, RPM_MAX_SCALE);

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_make(35, 35, 35));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_radius(&style_bg, 3);                  
    lv_obj_add_style(rpm_bar, &style_bg, LV_PART_MAIN);

    static lv_style_t style_indic;
    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_radius(&style_indic, 0);
    lv_obj_add_style(rpm_bar, &style_indic, LV_PART_INDICATOR);

    fuel_label = lv_label_create(screen);
    lv_obj_align(fuel_label, LV_ALIGN_CENTER, 240, -175);
    lv_obj_add_style(fuel_label, &style_large_metric, 0);

    fuel_bar = lv_bar_create(screen);
    lv_obj_set_size(fuel_bar, 300, 38);
    lv_obj_align(fuel_bar, LV_ALIGN_CENTER, -220, -175);
    lv_bar_set_range(fuel_bar, 0, 100);
    lv_obj_add_style(fuel_bar, &style_fuel_bar, LV_PART_MAIN);
    lv_obj_add_style(fuel_bar, &style_fuel_indicator, LV_PART_INDICATOR);

    fuel_percent_label = lv_label_create(fuel_bar);
    lv_obj_add_style(fuel_percent_label, &style_sub_header, 0);
    lv_obj_center(fuel_percent_label);

    lv_obj_t *fuel_markers = lv_label_create(screen);
    lv_label_set_text(fuel_markers, "E       1/4      1/2      3/4       F");
    lv_obj_add_style(fuel_markers, &style_sub_header, 0);
    lv_obj_align(fuel_markers, LV_ALIGN_CENTER, -220, -135);

    temp_label = lv_label_create(screen);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 20, -100);
    lv_obj_add_style(temp_label, &style_large_metric, 0);

    gear_label = lv_label_create(screen);
    lv_obj_align(gear_label, LV_ALIGN_CENTER, 0, -50);
    lv_obj_add_style(gear_label, &style_giant_gear, 0);

    rpm_value = lv_label_create(screen);
    lv_obj_align(rpm_value, LV_ALIGN_CENTER, 0, 65);
    lv_obj_add_style(rpm_value, &style_sub_header, 0);

    mpg_label = lv_label_create(screen);
    lv_obj_align(mpg_label, LV_ALIGN_CENTER, 0, 115); 
    lv_obj_add_style(mpg_label, &style_sub_header, 0);

    range_label = lv_label_create(screen);
    lv_obj_align(range_label, LV_ALIGN_CENTER, 0, 155);
    lv_obj_add_style(range_label, &style_sub_header, 0);

    reset_btn = lv_button_create(screen);
    lv_obj_set_size(reset_btn, 180, 44);
    lv_obj_align(reset_btn, LV_ALIGN_CENTER, 30, -175);
    lv_obj_add_event_cb(reset_btn, fuel_reset_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * label_btn = lv_label_create(reset_btn);
    lv_label_set_text(label_btn, "Reset");
    lv_obj_add_style(label_btn, &style_sub_header, 0);
    lv_obj_center(label_btn);

    can_status_label = lv_label_create(screen);
    lv_label_set_text(can_status_label, "CAN");
    lv_obj_add_style(can_status_label, &style_sub_header, 0);
    lv_obj_set_style_text_color(can_status_label, lv_color_hex(0xF1C40F), 0);
    lv_obj_align(can_status_label, LV_ALIGN_BOTTOM_RIGHT, -20, -15);

    abs_mode_label = lv_label_create(screen);
    lv_label_set_text(abs_mode_label, "ABS: ON");
    lv_obj_add_style(abs_mode_label, &style_sub_header, 0);
    lv_obj_align(abs_mode_label, LV_ALIGN_BOTTOM_LEFT, 20, -15);
}

#ifndef PC_SIMULATOR
void UIProcessingLoop(void * pvParameters) {
    Serial.println("UI: task entered");
    unsigned long lastLvglTick = millis();
    bool engineHasRun = false; // Tracks whether RPM has crossed the running threshold since the last stop
    float previousFuelGallons = -1.0f;
    int32_t previousFuelPercent = -1;
    float previousTempFahrenheit = -1000.0f;
    uint8_t previousGear = 255;
    uint16_t previousRPM = UINT16_MAX;
    float previousMPG = -1.0f;
    float previousDistance = -1.0f;
    float previousRange = -1.0f;
    bool previousCanActive = false;
    uint8_t previousAbsModeStatus = 255;

    while(true) {
        unsigned long currentMillis = millis();
        lv_tick_inc(currentMillis - lastLvglTick);
        lastLvglTick = currentMillis;

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        float localFuel = liveData.fuelRemaining;
        int16_t localTemp = liveData.coolantTemp;
        uint8_t localGear = liveData.currentGear;
        uint16_t localRPM = liveData.engineRPM; 
        bool localCanActive = liveData.canActive;
        uint8_t localAbsModeStatus = liveData.absModeStatus;
        float localMPG = liveData.currentMPG;
        float localDistance = liveData.distanceTraveledKm * 0.621371; 
        float localRange = localFuel * 0.264172 * localMPG;
        float localFuelGallons = localFuel * 0.264172;
        float localTempFahrenheit = (localTemp * 9.0f / 5.0f) + 32.0f;
        int32_t localFuelPercent = (int32_t)((localFuel / TOTAL_FUEL_CAPACITY) * 100.0f + 0.5f);
        if (localFuelPercent < 0) localFuelPercent = 0;
        if (localFuelPercent > 100) localFuelPercent = 100;
        xSemaphoreGive(dataMutex);

        if (localFuelGallons != previousFuelGallons) {
            lv_label_set_text_fmt(fuel_label, "FUEL: %.2f gal", localFuelGallons);
            previousFuelGallons = localFuelGallons;
        }
        if (localFuelPercent != previousFuelPercent) {
            lv_bar_set_value(fuel_bar, localFuelPercent, LV_ANIM_OFF);
            lv_label_set_text_fmt(fuel_percent_label, "%d%%", localFuelPercent);
            previousFuelPercent = localFuelPercent;
        }
        if (localTempFahrenheit != previousTempFahrenheit) {
            lv_label_set_text_fmt(temp_label, "TEMP: %.0f F", localTempFahrenheit);
            previousTempFahrenheit = localTempFahrenheit;
        }
        
        if (localGear != previousGear) {
            if(localGear == 0) {
                lv_label_set_text(gear_label, "GEAR: N");
            } else {
                lv_label_set_text_fmt(gear_label, "GEAR: %d", localGear);
            }
            previousGear = localGear;
        }

        bool rpmChanged = localRPM != previousRPM;
        if (rpmChanged) {
            lv_bar_set_value(rpm_bar, localRPM, LV_ANIM_OFF);
            lv_label_set_text_fmt(rpm_value, "%u RPM", localRPM);
            previousRPM = localRPM;
        }

        if (rpmChanged) {
            if (localRPM < RPM_PEAK_POWER) {
                lv_obj_set_style_bg_color(rpm_bar, lv_color_make(0, 220, 0), LV_PART_INDICATOR);
            }
            else if (localRPM < RPM_REDLINE) {
                lv_obj_set_style_bg_color(rpm_bar, lv_color_make(240, 240, 0), LV_PART_INDICATOR);
            }
            else {
                lv_obj_set_style_bg_color(rpm_bar, lv_color_make(255, 0, 0), LV_PART_INDICATOR);
            }
        }

        if (localMPG != previousMPG || localDistance != previousDistance) {
            lv_label_set_text_fmt(mpg_label, "ECONOMY: %.1f MPG (%.1f mi)", localMPG, localDistance);
            previousMPG = localMPG;
            previousDistance = localDistance;
        }
        if (localRange != previousRange || localMPG != previousMPG) {
            if (localMPG > 0.0f) {
                lv_label_set_text_fmt(range_label, "RANGE: %.1f mi", localRange);
            } else {
                lv_label_set_text(range_label, "RANGE: -- mi");
            }
            previousRange = localRange;
        }

        if (localCanActive != previousCanActive) {
            lv_obj_set_style_text_color(
                can_status_label,
                localCanActive ? lv_color_hex(0x2ECC71) : lv_color_hex(0xF1C40F),
                0);
            previousCanActive = localCanActive;
        }

        if (localAbsModeStatus != previousAbsModeStatus) {
            if (localAbsModeStatus == 2) {
                lv_label_set_text(abs_mode_label, "ABS: OFF");
            }
            else if (localAbsModeStatus == 1) {
                lv_label_set_text(abs_mode_label, "ABS: OFF-Road");
            }
            else {
                lv_label_set_text(abs_mode_label, "ABS: ON");
            }
            previousAbsModeStatus = localAbsModeStatus;
        }

        if (localRPM > ENGINE_RUN_RPM_THRESHOLD) {
            engineHasRun = true;
        } else if (engineHasRun) {
            // Engine just transitioned from running to stopped; persist fuel usage once
            engineHasRun = false;

            xSemaphoreTake(dataMutex, portMAX_DELAY);
            float currentConsumed = liveData.fuelConsumed;
            float currentDistance = liveData.distanceTraveledKm;
            xSemaphoreGive(dataMutex);

            if (currentConsumed > fuelConsumedAtStartup + 0.001f) {
                persistFuelDataIfChanged(currentConsumed, currentDistance);
            }
        }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(16)); 
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.setDebugOutput(true);
    esp_log_level_set("ESP_Panel", ESP_LOG_DEBUG);
    Serial.println("BOOT: setup");
#if ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM
    Serial.println("CONFIG: custom board enabled");
#else
    Serial.println("CONFIG: custom board disabled");
#endif
#if ESP_PANEL_BOARD_USE_LCD
    Serial.println("CONFIG: LCD enabled");
#else
    Serial.println("CONFIG: LCD disabled");
#endif
    dataMutex = xSemaphoreCreateMutex();
    touchMutex = xSemaphoreCreateMutex();

    if (!expander_init()) {
        Serial.println("EXPANDER: initialization failed, touch/backlight may misbehave");
    }

    if (!hardware_display_init()) {
        while (true) {
            delay(1000);
        }
    }

    UI_Layout_Init();
    lv_display_enable_invalidation(hardwareDisplay, true);
    lv_obj_invalidate(lv_screen_active());
    Serial.println("BOOT: UI ready");

    nvStorage.begin("bike-fuel", false);
    liveData.fuelConsumed = nvStorage.getFloat("consumed", 0.0);
    fuelConsumedAtStartup = liveData.fuelConsumed;
    liveData.distanceTraveledKm = nvStorage.getFloat("distance", 0.0); 
    liveData.fuelRemaining = TOTAL_FUEL_CAPACITY - liveData.fuelConsumed;
    if (liveData.fuelRemaining < 0.0f) {
        liveData.fuelRemaining = 0.0f;
    }
    nvStorage.end();

    BaseType_t uiTaskResult = xTaskCreatePinnedToCore(UIProcessingLoop, "UI_Core1", 8192, NULL, 1, &UITaskHandle, 1);
    Serial.printf("BOOT: UI task result %d\n", static_cast<int>(uiTaskResult));
    Serial.println("BOOT: UI task started");

    xTaskCreatePinnedToCore(TouchProcessingLoop, "TouchTask", 3072, NULL, 1, &TouchTaskHandle, 0);
}

void loop() {
    vTaskDelete(NULL); 
}
#endif
