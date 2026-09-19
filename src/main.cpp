#include <Arduino.h>
#include <driver/twai.h> // ESP32 native CAN driver
#include <Preferences.h> // Non-volatile flash storage management
#include <lvgl.h>        // UI Graphics Core Engine

// --- System Configuration Constants ---
const float TOTAL_FUEL_CAPACITY = 12.8;            // 2023 US Husqvarna 701 usable Liters
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
TaskHandle_t CANTaskHandle = NULL;
TaskHandle_t UITaskHandle = NULL;
Preferences nvStorage;

// --- LVGL Graphical Object Pointers (Core 1) ---
lv_obj_t * fuel_label;
lv_obj_t * temp_label;
lv_obj_t * gear_label;
lv_obj_t * reset_btn;
lv_obj_t * mpg_label;

// --- Tachometer Configuration Metrics ---
const int32_t RPM_MAX_SCALE = 10000;    // 1,000 RPM over the 9,000 redline limit
const int32_t RPM_PEAK_POWER = 8000;    // Yellow zone entry threshold
const int32_t RPM_REDLINE = 9000;       // Red zone entry threshold

// --- New Graphical Pointer ---
lv_obj_t * rpm_bar;


// =========================================================================
// 📡 CORE 0: VEHICLE DATA PROCESSING & UDS DIAGNOSTIC POLLING
// =========================================================================
void sendUDSPIDRequest(uint8_t pid) {
    twai_message_t tx_msg;
    tx_msg.identifier = 0x7DF;       // Functional OBD-II / UDS Broadcast Request ID
    tx_msg.extd = 0;                 // Standard 11-bit Identifier
    tx_msg.rtr = 0;                  // Data Frame
    tx_msg.data_length_code = 8;     // Standard UDS requirements impose fixed 8-byte frames
    
    // UDS Request Mapping Payload Structure:
    tx_msg.data[0] = 0x02;           // Number of remaining valid bytes in this frame request
    tx_msg.data[1] = 0x01;           // Service Mode (Show Current Powertrain Diagnostics Data)
    tx_msg.data[2] = pid;            // The precise parameter ID requested
    for (int i = 3; i < 8; i++) tx_msg.data[i] = 0xAA; // Padding unused bytes

    twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
}

void CANProcessingLoop(void * pvParameters) {
    // ... Existing CAN / TWAI Driver Initialization ...

    unsigned long lastUDSPoll = 0;
    unsigned long lastTimeCheck = millis();
    bool pollToggle = false; 

    while(true) {
        unsigned long currentMillis = millis();
        twai_message_t rx_msg;

        // Alternately poll Coolant Temp and Speed every 500ms to avoid overwhelming the bus
        if (currentMillis - lastUDSPoll >= 500) {
            if (pollToggle) {
                sendUDSPIDRequest(0x05); // Request Coolant Temp
            } else {
                sendUDSPIDRequest(0x0D); // Request Vehicle Speed (km/h)
            }
            pollToggle = !pollToggle;
            lastUDSPoll = currentMillis;
        }

        if (twai_receive(&rx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);

            if (rx_msg.identifier == 0x7E8 && rx_msg.data[1] == 0x41) {
                // Parse Coolant Response
                if (rx_msg.data[2] == 0x05) {
                    liveData.coolantTemp = (int16_t)rx_msg.data[3] - 40;
                }
                // --- PARSE VEHICLE SPEED RESPONSE ---
                else if (rx_msg.data[2] == 0x0D) {
                    uint8_t currentSpeedKmh = rx_msg.data[3]; // Raw byte value matches km/h directly
                    
                    unsigned long currentTime = millis();
                    float deltaTimeHours = (currentTime - lastTimeCheck) / 3600000.0;
                    lastTimeCheck = currentTime;

                    if (currentSpeedKmh > 0) {
                        // Dead reckoning calculation logs dynamic interval distance metrics
                        liveData.distanceTraveledKm += (float)currentSpeedKmh * deltaTimeHours;
                        
                        // Calculate cumulative average economy
                        if (liveData.fuelConsumed > 0.001) {
                            float miles = liveData.distanceTraveledKm * 0.621371;
                            float gallons = liveData.fuelConsumed * 0.264172;
                            liveData.currentMPG = miles / gallons;
                        }
                    }
                }
            }
            
            // ... Keep your existing passive sniffing parses for Gear and Injection Duration here ...

            xSemaphoreGive(dataMutex);
        }

        // --- UPDATE THE USER INITIALIZATION TRIPPED BLOCK ---
        if (liveData.fuelResetTriggered) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            liveData.fuelConsumed = 0.0;
            liveData.fuelRemaining = TOTAL_FUEL_CAPACITY;
            liveData.distanceTraveledKm = 0.0; // Wipe the distance interval tracking loop
            liveData.currentMPG = 0.0;
            liveData.fuelResetTriggered = false;

            nvStorage.begin("bike-fuel", false);
            nvStorage.putFloat("consumed", 0.0);
            nvStorage.putFloat("distance", 0.0); // Persist zero baseline to NVS flash memory
            nvStorage.end();
            xSemaphoreGive(dataMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// =========================================================================
// 🎨 CORE 1: GRAPHICS UI INTERACTION AND ENGINE INTERACTION LAYER
// =========================================================================
static void fuel_reset_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        // Safe cross-core signal assertion
        liveData.fuelResetTriggered = true;
    }
}

void UI_Layout_Init() {
    // ---------------------------------------------------------------------
    // 🚦 1. DESIGN THE MULTICOLOR PROGRESSIVE TACHOMETER BAR
    // ---------------------------------------------------------------------
    rpm_bar = lv_bar_create(lv_screen_active());
    lv_obj_set_size(rpm_bar, 760, 35);                  // Stretches across your 800x480 panel boundaries
    lv_obj_align(rpm_bar, LV_ALIGN_TOP_MID, 0, 10);      // Locks cleanly to the top ridge of the dash
    lv_bar_set_range(rpm_bar, 0, RPM_MAX_SCALE);
    mpg_label = lv_label_create(lv_screen_active());
    lv_obj_align(mpg_label, LV_ALIGN_TOP_MID, 0, 110); // Positions code below fuel_label array elements
    lv_obj_set_style_text_font(mpg_label, &lv_font_montserrat_20, 0);

    // Style the empty background track of the bar (Dark charcoal housing)
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_make(35, 35, 35));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_radius(&style_bg, 3);                  // Sharp clean utility dash style
    lv_obj_add_style(rpm_bar, &style_bg, LV_PART_MAIN);

    // Style the progressive inner indicator (The colored segments)
    static lv_style_t style_indic;
    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_radius(&style_indic, 0);
    lv_obj_add_style(rpm_bar, &style_indic, LV_PART_INDICATOR);

    // ---------------------------------------------------------------------
    // 🏷️ 2. EXISTING READOUT LABELS (FUEL, TEMPERATURE, GEAR, RESET)
    // ---------------------------------------------------------------------
    fuel_label = lv_label_create(lv_screen_active());
    lv_obj_align(fuel_label, LV_ALIGN_TOP_MID, 0, 65);   // Pushed down slightly to make room for RPM bar
    lv_obj_set_style_text_font(fuel_label, &lv_font_montserrat_28, 0);

    temp_label = lv_label_create(lv_screen_active());
    lv_obj_align(temp_label, LV_ALIGN_CENTER, -150, 20);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_20, 0);

    gear_label = lv_label_create(lv_screen_active());
    lv_obj_align(gear_label, LV_ALIGN_CENTER, 150, 20);
    lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_48, 0);

    reset_btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(reset_btn, 220, 60);
    lv_obj_align(reset_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(reset_btn, fuel_reset_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * label_btn = lv_label_create(reset_btn);
    lv_label_set_text(label_btn, "RESET FUEL TANK");
    lv_obj_center(label_btn);
}


void UIProcessingLoop(void * pvParameters) {
    // Note: Insert your Waveshare hardware panel initialization routines here before rendering
    // waveshare_panel_spi_touch_init();
    lv_init();
    UI_Layout_Init();

    unsigned long lastNVSSave = millis();

    while(true) {
        // Execute frame layout calculus updates
        lv_timer_handler(); 

        // Safely extract metrics tracked by Core 0 checking constraints via Mutex guards
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        float localFuel = liveData.fuelRemaining;
        int16_t localTemp = liveData.coolantTemp;
        uint8_t localGear = liveData.currentGear;
        xSemaphoreGive(dataMutex);

        // Update individual visual element buffers dynamically
        lv_label_set_text_fmt(fuel_label, "FUEL: %.2f L", localFuel);
        lv_label_set_text_fmt(temp_label, "TEMP: %d °C", localTemp);
        
        if(localGear == 0) {
            lv_label_set_text(gear_label, "GEAR: N");
        } else {
            lv_label_set_text_fmt(gear_label, "GEAR: %d", localGear);
        }

        // Periodic NVS storage background safety checkpoint updates
        if (millis() - lastNVSSave >= NVS_WRITE_INTERVAL_MS) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            float currentConsumed = liveData.fuelConsumed;
            float currentDistance = liveData.distanceTraveledKm;
            xSemaphoreGive(dataMutex);

            nvStorage.begin("bike-fuel", false);
            nvStorage.putFloat("consumed", currentConsumed);
            nvStorage.putFloat("distance", currentDistance); // Protect calculations from sudden key turn off
            nvStorage.end();
            lastNVSSave = millis();
        }

        // --- INSERT THIS INTERNALLY WITHIN YOUR UIProcessingLoop WHILE(TRUE) ---
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        uint16_t localRPM = liveData.engineRPM; // Pull fresh telemetry intercepted from CAN Core 0
        xSemaphoreGive(dataMutex);

        // Apply the value mapping logic down to the screen buffer
        lv_bar_set_value(rpm_bar, localRPM, LV_ANIM_OFF);

        // Dynamic boundary coloration adjustments matching your specified ranges
        if (localRPM < RPM_PEAK_POWER) {
            // Normal Bounds: Solid Green Alert Color (RGB: 0, 220, 0)
            lv_obj_set_style_bg_color(rpm_bar, lv_color_make(0, 220, 0), LV_PART_INDICATOR);
        } 
        else if (localRPM >= RPM_PEAK_POWER && localRPM < RPM_REDLINE) {
            // Power Band: Warning Yellow Alert Color (RGB: 240, 240, 0)
            lv_obj_set_style_bg_color(rpm_bar, lv_color_make(240, 240, 0), LV_PART_INDICATOR);
        } 
        else {
            // Over-Redline: Critical Red Warning Flash Trigger (RGB: 255, 0, 0)
            lv_obj_set_style_bg_color(rpm_bar, lv_color_make(255, 0, 0), LV_PART_INDICATOR);
        }


        // --- INSERT INTO YOUR UIProcessingLoop WHILE(TRUE) REFRESH BUFFER ---
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        float localFuel = liveData.fuelRemaining;
        int16_t localTemp = liveData.coolantTemp;
        uint8_t localGear = liveData.currentGear;
        float localMPG = liveData.currentMPG;
        float localDistance = liveData.distanceTraveledKm * 0.621371; // Convert to miles for screen readout
        xSemaphoreGive(dataMutex);

        lv_label_set_text_fmt(fuel_label, "FUEL: %.2f L", localFuel);
        lv_label_set_text_fmt(temp_label, "TEMP: %d °C", localTemp);
        lv_label_set_text_fmt(mpg_label, "ECONOMY: %.1f MPG (%.1f mi)", localMPG, localDistance);


        vTaskDelay(pdMS_TO_TICKS(16)); // Limit tick calculation to ~60 frames per second
    }
}

// =========================================================================
// 🚀 MAIN EXECUTION INITIALIZATION
// =========================================================================
void setup() {
    Serial.begin(115200);
    dataMutex = xSemaphoreCreateMutex();

    // Extract saved fuel baseline context before initiating processing cores
    nvStorage.begin("bike-fuel", false);
    liveData.fuelConsumed = nvStorage.getFloat("consumed", 0.0);
    liveData.fuelRemaining = TOTAL_FUEL_CAPACITY - liveData.fuelConsumed;

    nvStorage.begin("bike-fuel", false);
    liveData.fuelConsumed = nvStorage.getFloat("consumed", 0.0);
    liveData.distanceTraveledKm = nvStorage.getFloat("distance", 0.0); // Recover distance traveled
    liveData.fuelRemaining = TOTAL_FUEL_CAPACITY - liveData.fuelConsumed;
    
    nvStorage.end();

    xTaskCreatePinnedToCore(CANProcessingLoop, "CAN_Core0", 4096, NULL, 2, &CANTaskHandle, 0);
    xTaskCreatePinnedToCore(UIProcessingLoop, "UI_Core1", 8192, NULL, 1, &UITaskHandle, 1);
}

void loop() {
    vTaskDelete(NULL); // Delete standard tracking loop to completely pass execution to cores
}
