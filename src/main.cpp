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
    uint16_t engineRPM = 0;
    int16_t coolantTemp = 0;       // Degree Celsius 
    uint8_t currentGear = 0;       // 0=Neutral, 1-6=Gears
    bool fuelResetTriggered = false; // Signal flag passed from Core 1 to Core 0
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
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)16, (gpio_num_t)17, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); // Standard Husqvarna network speed
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        twai_start();
    }

    unsigned long lastUDSPoll = 0;
    unsigned long lastTimeCheck = millis();

    while(true) {
        unsigned long currentMillis = millis();
        twai_message_t rx_msg;

        // 1. Send UDS Polling requests cyclically at specified safe bounds
        if (currentMillis - lastUDSPoll >= UDS_POLL_INTERVAL_MS) {
            sendUDSPIDRequest(0x05); // Request Engine Coolant Temperature PID
            lastUDSPoll = currentMillis;
        }

        // 2. Incoming Vehicle Bus Packet Parser
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);

            // --- A. Process Active UDS Polling Diagnostics Diagnostics Responses ---
            if (rx_msg.identifier == 0x7E8) { // Standard ECU Diagnostic Response Broadcast ID
                // rx_msg.data[1] == 0x41 implies positive confirmation response matching Mode 01
                if (rx_msg.data[1] == 0x41 && rx_msg.data[2] == 0x05) { 
                    // OBD-II Coolant Formula Translation: A - 40 (Resulting variant ranges -40C to +215C)
                    liveData.coolantTemp = (int16_t)rx_msg.data[3] - 40;
                }
            }

            // --- B. Passive Sniffing Frames ---
            if (rx_msg.identifier == 0x2C0) { // Gear ID
                liveData.currentGear = rx_msg.data[0]; 
            }
            
            if (rx_msg.identifier == 0x320) { // Consumption ID
                float deltaTime = (currentMillis - lastTimeCheck) / 3600000.0;
                lastTimeCheck = currentMillis;
                float currentFuelRate = ((rx_msg.data[0] * 256) + rx_msg.data[1]) * 0.05; 
                
                liveData.fuelConsumed += currentFuelRate * deltaTime;
                liveData.fuelRemaining = TOTAL_FUEL_CAPACITY - liveData.fuelConsumed;
                if(liveData.fuelRemaining < 0) liveData.fuelRemaining = 0;
            }

            xSemaphoreGive(dataMutex);
        }

        // 3. Monitor for explicit fuel gauge initialization request from user interface (Core 1)
        if (liveData.fuelResetTriggered) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            liveData.fuelConsumed = 0.0;
            liveData.fuelRemaining = TOTAL_FUEL_CAPACITY;
            liveData.fuelResetTriggered = false; // Reset request flag line
            
            // Instantly sync state down to flash memory array
            nvStorage.begin("bike-fuel", false);
            nvStorage.putFloat("consumed", 0.0);
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
    // 1. Fuel Display Label Layout Setup
    fuel_label = lv_label_create(lv_screen_active());
    lv_obj_align(fuel_label, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_text_font(fuel_label, &lv_font_montserrat_28, 0);

    // 2. Coolant Temperature Label Configuration
    temp_label = lv_label_create(lv_screen_active());
    lv_obj_align(temp_label, LV_ALIGN_CENTER, -150, 0);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_20, 0);

    // 3. Transmission Gear Data Widget Layout Setup
    gear_label = lv_label_create(lv_screen_active());
    lv_obj_align(gear_label, LV_ALIGN_CENTER, 150, 0);
    lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_48, 0); // Big gear indicator text

    // 4. Interactive Fuel Gauge Reset Trigger Button Construction
    reset_btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(reset_btn, 220, 60);
    lv_obj_align(reset_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_event_cb(reset_btn, fuel_reset_event_cb, LV_EVENT_ALL, NULL); // Bind our click listener

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
            xSemaphoreGive(dataMutex);

            nvStorage.begin("bike-fuel", false);
            nvStorage.putFloat("consumed", currentConsumed);
            nvStorage.end();
            lastNVSSave = millis();
        }

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
    nvStorage.end();

    xTaskCreatePinnedToCore(CANProcessingLoop, "CAN_Core0", 4096, NULL, 2, &CANTaskHandle, 0);
    xTaskCreatePinnedToCore(UIProcessingLoop, "UI_Core1", 8192, NULL, 1, &UITaskHandle, 1);
}

void loop() {
    vTaskDelete(NULL); // Delete standard tracking loop to completely pass execution to cores
}
