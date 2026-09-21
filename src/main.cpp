#include <Arduino.h>

// --- Identify and Isolate Environment Framework Toolchains ---
#ifdef PC_SIMULATOR
    // Native Desktop Windows Emulation System Drivers
    #include <SDL2/SDL.h>
    void hal_init(void); 
#else
    // Native Physical Micro-Controller Hardware Drivers
    #include <driver/twai.h> // ESP32 native CAN driver
    #include <Preferences.h> // Non-volatile flash storage management
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
const unsigned long UDS_POLL_INTERVAL_MS = 1000;    // Query coolant temp every 1000ms
const unsigned long NVS_WRITE_INTERVAL_MS = 30000;  // Save fuel data to flash every 30 seconds

// --- Thread-Safe Multi-Core Structures ---
SemaphoreHandle_t dataMutex;

struct BikeMetrics {
    float fuelRemaining = TOTAL_FUEL_CAPACITY;
    float fuelConsumed = 0.0;
    float distanceTraveledKm = 0.0; // Cumulative distance track since last fill-up
    float currentMPG = 0.0;         // Calculated average fuel efficiency
    uint16_t engineRPM = 0;
    int16_t coolantTemp = 0;
    uint8_t currentGear = 0;
    bool fuelResetTriggered = false;
};
volatile BikeMetrics liveData;

// --- Task & Flash Memory Handlers ---
#ifndef PC_SIMULATOR
    TaskHandle_t CANTaskHandle = NULL;
    TaskHandle_t UITaskHandle = NULL;
    Preferences nvStorage;
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

void CANProcessingLoop(void * pvParameters) {
    unsigned long lastUDSPoll = 0;
    unsigned long lastTimeCheck = millis();
    bool pollToggle = false; 

    while(true) {
        unsigned long currentMillis = millis();
        twai_message_t rx_msg;

        if (currentMillis - lastUDSPoll >= 500) {
            if (pollToggle) {
                sendUDSPIDRequest(0x05); 
            } else {
                sendUDSPIDRequest(0x0D); 
            }
            pollToggle = !pollToggle;
            lastUDSPoll = currentMillis;
        }

        if (twai_receive(&rx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);

            if (rx_msg.identifier == 0x7E8 && rx_msg.data[1] == 0x41) {
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
                        
                        if (liveData.fuelConsumed > 0.001) {
                            float miles = liveData.distanceTraveledKm * 0.621371;
                            float gallons = liveData.fuelConsumed * 0.264172;
                            liveData.currentMPG = miles / gallons;
                        }
                    }
                }
            }
            xSemaphoreGive(dataMutex);
        }

        if (liveData.fuelResetTriggered) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            liveData.fuelConsumed = 0.0;
            liveData.fuelRemaining = TOTAL_FUEL_CAPACITY;
            liveData.distanceTraveledKm = 0.0; 
            liveData.currentMPG = 0.0;
            liveData.fuelResetTriggered = false;

            nvStorage.begin("bike-fuel", false);
            nvStorage.putFloat("consumed", 0.0);
            nvStorage.putFloat("distance", 0.0); 
            nvStorage.end();
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
    lv_obj_set_size(rpm_bar, 760, 35);                  
    lv_obj_align(rpm_bar, LV_ALIGN_TOP_MID, 0, 10);      
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
    lv_obj_align(fuel_label, LV_ALIGN_CENTER, -220, -160);
    lv_obj_add_style(fuel_label, &style_large_metric, 0);

    fuel_bar = lv_bar_create(screen);
    lv_obj_set_size(fuel_bar, 300, 38);
    lv_obj_align(fuel_bar, LV_ALIGN_CENTER, -220, -60);
    lv_bar_set_range(fuel_bar, 0, 100);
    lv_obj_add_style(fuel_bar, &style_fuel_bar, LV_PART_MAIN);
    lv_obj_add_style(fuel_bar, &style_fuel_indicator, LV_PART_INDICATOR);

    fuel_percent_label = lv_label_create(fuel_bar);
    lv_obj_add_style(fuel_percent_label, &style_sub_header, 0);
    lv_obj_center(fuel_percent_label);

    lv_obj_t *fuel_markers = lv_label_create(screen);
    lv_label_set_text(fuel_markers, "E       1/4      1/2      3/4       F");
    lv_obj_add_style(fuel_markers, &style_sub_header, 0);
    lv_obj_align(fuel_markers, LV_ALIGN_CENTER, -220, -20);

    temp_label = lv_label_create(screen);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 220, -100);
    lv_obj_add_style(temp_label, &style_large_metric, 0);

    gear_label = lv_label_create(screen);
    lv_obj_align(gear_label, LV_ALIGN_CENTER, 0, -50);
    lv_obj_add_style(gear_label, &style_giant_gear, 0);

    mpg_label = lv_label_create(screen);
    lv_obj_align(mpg_label, LV_ALIGN_CENTER, 0, 65); 
    lv_obj_add_style(mpg_label, &style_sub_header, 0);

    range_label = lv_label_create(screen);
    lv_obj_align(range_label, LV_ALIGN_CENTER, 0, 105);
    lv_obj_add_style(range_label, &style_sub_header, 0);

    reset_btn = lv_button_create(screen);
    lv_obj_set_size(reset_btn, 180, 44);
    lv_obj_align(reset_btn, LV_ALIGN_CENTER, -220, -110);
    lv_obj_add_event_cb(reset_btn, fuel_reset_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * label_btn = lv_label_create(reset_btn);
    lv_label_set_text(label_btn, "Reset");
    lv_obj_add_style(label_btn, &style_sub_header, 0);
    lv_obj_center(label_btn);
}

#ifndef PC_SIMULATOR
void UIProcessingLoop(void * pvParameters) {
    lv_init();
    UI_Layout_Init();
    unsigned long lastNVSSave = millis();

    while(true) {
        lv_timer_handler(); 

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        float localFuel = liveData.fuelRemaining;
        int16_t localTemp = liveData.coolantTemp;
        uint8_t localGear = liveData.currentGear;
        uint16_t localRPM = liveData.engineRPM; 
        float localMPG = liveData.currentMPG;
        float localDistance = liveData.distanceTraveledKm * 0.621371; 
        float localRange = localFuel * 0.264172 * localMPG;
        float localFuelGallons = localFuel * 0.264172;
        float localTempFahrenheit = (localTemp * 9.0f / 5.0f) + 32.0f;
        int32_t localFuelPercent = (int32_t)((localFuel / TOTAL_FUEL_CAPACITY) * 100.0f + 0.5f);
        if (localFuelPercent < 0) localFuelPercent = 0;
        if (localFuelPercent > 100) localFuelPercent = 100;
        xSemaphoreGive(dataMutex);

        lv_label_set_text_fmt(fuel_label, "FUEL: %.2f gal", localFuelGallons);
        lv_bar_set_value(fuel_bar, localFuelPercent, LV_ANIM_OFF);
        lv_label_set_text_fmt(fuel_percent_label, "%d%%", localFuelPercent);
        lv_label_set_text_fmt(temp_label, "TEMP: %.0f F", localTempFahrenheit);
        
        if(localGear == 0) {
            lv_label_set_text(gear_label, "GEAR: N");
        } else {
            lv_label_set_text_fmt(gear_label, "GEAR: %d", localGear);
        }

        lv_bar_set_value(rpm_bar, localRPM, LV_ANIM_OFF);

        if (localRPM < RPM_PEAK_POWER) {
            lv_obj_set_style_bg_color(rpm_bar, lv_color_make(0, 220, 0), LV_PART_INDICATOR); 
        } 
        else if (localRPM < RPM_REDLINE) {
            lv_obj_set_style_bg_color(rpm_bar, lv_color_make(240, 240, 0), LV_PART_INDICATOR); 
        } 
        else {
            lv_obj_set_style_bg_color(rpm_bar, lv_color_make(255, 0, 0), LV_PART_INDICATOR); 
        }

        lv_label_set_text_fmt(mpg_label, "ECONOMY: %.1f MPG (%.1f mi)", localMPG, localDistance);
        if (localMPG > 0.0f) {
            lv_label_set_text_fmt(range_label, "RANGE: %.1f mi", localRange);
        } else {
            lv_label_set_text(range_label, "RANGE: -- mi");
        }

        if (millis() - lastNVSSave >= NVS_WRITE_INTERVAL_MS) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            float currentConsumed = liveData.fuelConsumed;
            float currentDistance = liveData.distanceTraveledKm;
            xSemaphoreGive(dataMutex);

            nvStorage.begin("bike-fuel", false);
            nvStorage.putFloat("consumed", currentConsumed);
            nvStorage.putFloat("distance", currentDistance); 
            nvStorage.end();
            lastNVSSave = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(16)); 
    }
}

void setup() {
    Serial.begin(115200);
    dataMutex = xSemaphoreCreateMutex();

    nvStorage.begin("bike-fuel", false);
    liveData.fuelConsumed = nvStorage.getFloat("consumed", 0.0);
    liveData.distanceTraveledKm = nvStorage.getFloat("distance", 0.0); 
    liveData.fuelRemaining = TOTAL_FUEL_CAPACITY - liveData.fuelConsumed;
    nvStorage.end();

    xTaskCreatePinnedToCore(CANProcessingLoop, "CAN_Core0", 4096, NULL, 2, &CANTaskHandle, 0);
    xTaskCreatePinnedToCore(UIProcessingLoop, "UI_Core1", 8192, NULL, 1, &UITaskHandle, 1);
}

void loop() {
    vTaskDelete(NULL); 
}
#endif
