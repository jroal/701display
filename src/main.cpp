#include <Arduino.h>

// --- Identify and Isolate Environment Framework Toolchains ---
#ifdef PC_SIMULATOR
    // Native Desktop Windows Emulation System Drivers
    #include <SDL2/SDL.h>
    void hal_init(void); 
#else
    // Native Physical Micro-Controller Hardware Drivers
    #include <driver/twai.h> // ESP32 native CAN driver
    #include <Wire.h>
    #include <SD.h>
    #include <SPI.h>
    #include <sys/time.h>
    #include <time.h>
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
#ifndef PC_SIMULATOR
static constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_15;
static constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_16;
static constexpr unsigned long CAN_INIT_RETRY_INTERVAL_MS = 5000;
static constexpr uint8_t PCF85063A_I2C_ADDRESS = 0x51;
static constexpr uint8_t PCF85063A_CTRL1_REGISTER = 0x00;
static constexpr uint8_t PCF85063A_SECONDS_REGISTER = 0x04;
static constexpr int PCF85063A_YEAR_OFFSET = 1970;
#endif

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
lv_obj_t * clock_label;

#ifndef PC_SIMULATOR
ESP_Panel hardwarePanel;
ESP_PanelLcd *hardwareLcd = nullptr;
ESP_PanelLcdTouch *hardwareTouch = nullptr;
lv_indev_t *hardwareTouchIndev = nullptr;
lv_display_t *hardwareDisplay = nullptr;
static uint8_t *lvglFrameBuffer = nullptr;
static uint8_t *lvglFrameBuffer2 = nullptr;

// Waveshare board's CH422G I/O expander controls TP_RST/LCD_RST/backlight over I2C.
static const uint8_t CH422G_IO_WRITE_ADDRESS = 0x38;
TaskHandle_t TouchTaskHandle = NULL;
TaskHandle_t SDLoggerTaskHandle = NULL;
static lv_obj_t *time_settings_overlay = nullptr;
static lv_obj_t *year_spinbox = nullptr;
static lv_obj_t *month_spinbox = nullptr;
static lv_obj_t *day_spinbox = nullptr;
static lv_obj_t *hour_spinbox = nullptr;
static lv_obj_t *minute_spinbox = nullptr;
static lv_obj_t *time_error_label = nullptr;
static int64_t clock_epoch_base = 0;
static uint32_t clock_tick_base = 0;

// --- SD CARD (SPI; card select is CH422G EXIO4) - same wiring as BafangBLE ---
static constexpr int PIN_SD_CLK = 12;
static constexpr int PIN_SD_MOSI = 11;
static constexpr int PIN_SD_MISO = 13;
static constexpr int PIN_SD_DUMMY_CS = 6;
static constexpr uint8_t PIN_SD_CS_EXPANDER = 4;
static constexpr const char *ERROR_LOG_PATH = "/error.txt";
static constexpr UBaseType_t ERROR_LOG_QUEUE_LENGTH = 32;
static constexpr unsigned long SD_LOG_RETRY_INTERVAL_MS = 5000;

struct ErrorLogRecord {
    time_t timestamp;
    uint32_t timestampMs;
    char message[128];
};

QueueHandle_t errorLogQueue = NULL;
File errorLogFile;
volatile bool sdCardMounted = false;
volatile uint32_t errorLogDroppedMessages = 0;
volatile bool rtcTimeValid = false;
time_t rtcSyncEpoch = 0;
uint32_t rtcSyncUptimeMs = 0;
bool sdSpiStarted = false;

static void log_error(const char *message) {
    if (errorLogQueue == NULL) {
        ++errorLogDroppedMessages;
        Serial.println(message);
        return;
    }

    ErrorLogRecord record = {};
    record.timestamp = rtcTimeValid ? time(nullptr) : 0;
    record.timestampMs = millis();
    snprintf(record.message, sizeof(record.message), "%s", message);
    if (xQueueSend(errorLogQueue, &record, 0) != pdTRUE) {
        ++errorLogDroppedMessages;
    }
}

static void format_log_timestamp(time_t timestamp, uint32_t uptimeMs, char *buffer, size_t bufferSize) {
    struct tm utcTime;
    if (timestamp > 0 && gmtime_r(&timestamp, &utcTime) != nullptr &&
        strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S UTC", &utcTime) > 0) {
        return;
    }
    snprintf(buffer, bufferSize, "uptime %lu ms", static_cast<unsigned long>(uptimeMs));
}

// Touch is polled from its own task on core 0, away from the RGB LCD flush on core 1 -
// the ESP32-S3's RGB panel DMA is sensitive to bus contention from I2C activity on the same core.
SemaphoreHandle_t touchMutex;
SemaphoreHandle_t sharedI2cMutex = NULL;
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
            if (sharedI2cMutex != NULL) {
                xSemaphoreTake(sharedI2cMutex, portMAX_DELAY);
            }
            hardwareTouch->readData();
            bool pressed = hardwareTouch->getTouchState();
            TouchPoint point = pressed ? hardwareTouch->getPoint() : TouchPoint();
            if (sharedI2cMutex != NULL) {
                xSemaphoreGive(sharedI2cMutex);
            }

            xSemaphoreTake(touchMutex, portMAX_DELAY);
            touchPressedShared = pressed;
            touchXShared = point.x;
            touchYShared = point.y;
            xSemaphoreGive(touchMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Brings up the CH422G expander over Arduino's Wire library (NOT the raw esp-idf i2c driver) -
// ESP_Panel_Library's GT911 touch/CH422G backends also talk over Wire, so initializing the bus
// with the raw i2c_driver_install() left Wire never begin()'d and touch silently never worked.
static bool expander_write_io(uint8_t outputLevels) {
    if (sharedI2cMutex != NULL) {
        xSemaphoreTake(sharedI2cMutex, portMAX_DELAY);
    }
    uint8_t outputEnable = 0x01;
    Wire.beginTransmission(0x24);
    Wire.write(outputEnable);
    if (Wire.endTransmission() != 0) {
        if (sharedI2cMutex != NULL) {
            xSemaphoreGive(sharedI2cMutex);
        }
        log_error("EXPANDER: failed to write output enable");
        return false;
    }
    Wire.beginTransmission(CH422G_IO_WRITE_ADDRESS);
    Wire.write(outputLevels);
    bool success = Wire.endTransmission() == 0;
    if (sharedI2cMutex != NULL) {
        xSemaphoreGive(sharedI2cMutex);
    }
    return success;
}

static bool expander_init() {
    if (!expander_write_io(0xFF)) {
        log_error("EXPANDER: output level write failed");
        return false;
    }
    return true;
}

static bool decode_bcd(uint8_t rawValue, int &decodedValue) {
    uint8_t ones = rawValue & 0x0F;
    uint8_t tens = (rawValue >> 4) & 0x0F;
    if (ones > 9 || tens > 9) {
        return false;
    }
    decodedValue = tens * 10 + ones;
    return true;
}

static uint8_t encode_bcd(int value) {
    return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

static int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned adjustedMonth = month > 2 ? month - 3 : month + 9;
    const unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<int64_t>(dayOfEra) - 719468;
}

static void civil_from_days(int64_t days, int &year, unsigned &month, unsigned &day) {
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned dayOfEra = static_cast<unsigned>(days - era * 146097);
    const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 -
                                dayOfEra / 146096) / 365;
    const int64_t yearBase = static_cast<int64_t>(yearOfEra) + era * 400;
    const unsigned dayOfYear = dayOfEra -
        (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const unsigned monthPrime = (5 * dayOfYear + 2) / 153;
    day = dayOfYear - (153 * monthPrime + 2) / 5 + 1;
    month = monthPrime < 10 ? monthPrime + 3 : monthPrime - 9;
    year = static_cast<int>(yearBase + (month <= 2 ? 1 : 0));
}

static bool is_valid_calendar_date(int year, unsigned month, unsigned day) {
    if (year < 1970 || year > 2069 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    static const uint8_t daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t monthLength = daysPerMonth[month - 1];
    if (month == 2 && (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0))) {
        ++monthLength;
    }
    return day <= monthLength;
}

static bool rtc_sync_system_time() {
    if (sharedI2cMutex != NULL) {
        xSemaphoreTake(sharedI2cMutex, portMAX_DELAY);
    }
    Wire.beginTransmission(PCF85063A_I2C_ADDRESS);
    Wire.write(PCF85063A_SECONDS_REGISTER);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(PCF85063A_I2C_ADDRESS, static_cast<uint8_t>(7)) != 7) {
        if (sharedI2cMutex != NULL) {
            xSemaphoreGive(sharedI2cMutex);
        }
        log_error("RTC: PCF85063A read failed");
        return false;
    }

    uint8_t registers[7];
    for (uint8_t &value : registers) {
        value = static_cast<uint8_t>(Wire.read());
    }
    if (sharedI2cMutex != NULL) {
        xSemaphoreGive(sharedI2cMutex);
    }

    int second, minute, hour, day, month, year;
    if ((registers[0] & 0x80) != 0 ||
        !decode_bcd(registers[0] & 0x7F, second) ||
        !decode_bcd(registers[1] & 0x7F, minute) ||
        !decode_bcd(registers[2] & 0x3F, hour) ||
        !decode_bcd(registers[3] & 0x3F, day) ||
        !decode_bcd(registers[5] & 0x1F, month) ||
        !decode_bcd(registers[6], year)) {
        log_error("RTC: invalid or unset PCF85063A time");
        return false;
    }

    year += PCF85063A_YEAR_OFFSET;
    if (second > 59 || minute > 59 || hour > 23 ||
        !is_valid_calendar_date(year, static_cast<unsigned>(month), static_cast<unsigned>(day))) {
        log_error("RTC: out-of-range PCF85063A time");
        return false;
    }

    int64_t epoch = days_from_civil(year, month, day) * 86400 +
                    hour * 3600 + minute * 60 + second;
    timeval systemTime = {static_cast<time_t>(epoch), 0};
    if (settimeofday(&systemTime, nullptr) != 0) {
        log_error("RTC: settimeofday failed");
        return false;
    }
    rtcSyncEpoch = epoch;
    rtcSyncUptimeMs = millis();
    rtcTimeValid = true;
    log_error("RTC: system clock synchronized from PCF85063A");
    return true;
}

static void rtc_write_time(int year, unsigned month, unsigned day, int hour, int minute) {
    rtcTimeValid = false;
    if (sharedI2cMutex != NULL) {
        xSemaphoreTake(sharedI2cMutex, portMAX_DELAY);
    }
    Wire.beginTransmission(PCF85063A_I2C_ADDRESS);
    Wire.write(PCF85063A_CTRL1_REGISTER);
    Wire.write(static_cast<uint8_t>(0x00));
    bool controlWriteSucceeded = Wire.endTransmission() == 0;
    if (!controlWriteSucceeded) {
        log_error("RTC: failed to clear PCF85063A control register");
    }

    Wire.beginTransmission(PCF85063A_I2C_ADDRESS);
    Wire.write(PCF85063A_SECONDS_REGISTER);
    Wire.write(encode_bcd(0));
    Wire.write(encode_bcd(minute));
    Wire.write(encode_bcd(hour));
    Wire.write(encode_bcd(static_cast<int>(day)));
    Wire.write(0x00);
    Wire.write(encode_bcd(static_cast<int>(month)));
    Wire.write(encode_bcd(year - PCF85063A_YEAR_OFFSET));
    bool timeWriteSucceeded = Wire.endTransmission() == 0;
    if (!timeWriteSucceeded) {
        log_error("RTC: PCF85063A write failed");
    }
    if (sharedI2cMutex != NULL) {
        xSemaphoreGive(sharedI2cMutex);
    }

    int64_t epoch = days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60;
    timeval systemTime = {static_cast<time_t>(epoch), 0};
    if (settimeofday(&systemTime, nullptr) != 0) {
        log_error("RTC: settimeofday failed after time edit");
        return;
    }
    if (controlWriteSucceeded && timeWriteSucceeded) {
        rtcSyncEpoch = static_cast<time_t>(epoch);
        rtcSyncUptimeMs = millis();
        rtcTimeValid = true;
        log_error("RTC: PCF85063A time updated");
    }
}

static int64_t current_clock_epoch() {
    uint32_t elapsedMs = lv_tick_get() - clock_tick_base;
    return clock_epoch_base + static_cast<int64_t>(elapsedMs) / 1000;
}

static void format_clock_epoch(int64_t epoch, char *buffer, size_t bufferSize) {
    int64_t days = epoch >= 0 ? epoch / 86400 : (epoch - 86399) / 86400;
    int64_t secondsOfDay = epoch - days * 86400;
    int year;
    unsigned month, day;
    civil_from_days(days, year, month, day);
    int hour = static_cast<int>(secondsOfDay / 3600);
    int minute = static_cast<int>((secondsOfDay % 3600) / 60);
    int second = static_cast<int>(secondsOfDay % 60);
    snprintf(buffer, bufferSize, "%04d-%02u-%02u  %02d:%02d:%02d",
             year, month, day, hour, minute, second);
}

static void refresh_clock_label() {
    if (clock_label == nullptr) {
        return;
    }
    static char lastText[32] = "";
    char text[32];
    format_clock_epoch(current_clock_epoch(), text, sizeof(text));
    if (strcmp(text, lastText) != 0) {
        lv_label_set_text(clock_label, text);
        snprintf(lastText, sizeof(lastText), "%s", text);
    }
}

static void hardware_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap) {
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

    hardwarePanel.init();
    hardwarePanel.begin();
    hardwareLcd = hardwarePanel.getLcd();
    if (hardwareLcd == nullptr) {
        log_error("DISPLAY: LCD initialization failed");
        return false;
    }

    lv_init();
    hardwareDisplay = lv_display_create(800, 480);
    lv_display_set_color_format(hardwareDisplay, LV_COLOR_FORMAT_RGB565);
    lvglFrameBuffer = static_cast<uint8_t *>(
        heap_caps_malloc(800 * BUFFER_LINES * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    lvglFrameBuffer2 = static_cast<uint8_t *>(
        heap_caps_malloc(800 * BUFFER_LINES * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (lvglFrameBuffer == nullptr || lvglFrameBuffer2 == nullptr) {
        log_error("DISPLAY: partial-frame buffer allocation failed");
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
        log_error("DISPLAY: touch initialization failed, screen will be view-only");
    } else {
        hardwareTouchIndev = lv_indev_create();
        lv_indev_set_type(hardwareTouchIndev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(hardwareTouchIndev, hardware_touch_read_cb);
    }
    return true;
}

static bool sd_log_init() {
    uint8_t outputLevels = static_cast<uint8_t>(0xFF & ~(1U << PIN_SD_CS_EXPANDER));
    if (!expander_write_io(outputLevels)) {
        log_error("SD: failed to assert card-select through CH422G");
        return false;
    }

    if (!sdSpiStarted) {
        SPI.setHwCs(false);
        SPI.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_DUMMY_CS);
        sdSpiStarted = true;
    }

    if (!SD.begin(PIN_SD_DUMMY_CS, SPI, 20000000, "/sdcard", 5, false)) {
        log_error("SD: mount failed; error logging unavailable");
        SD.end();
        return false;
    }

    sdCardMounted = true;
    errorLogFile = SD.open(ERROR_LOG_PATH, FILE_APPEND);
    if (!errorLogFile) {
        log_error("SD: failed to open error.txt; queued errors retained for retry");
    } else if (!rtcTimeValid) {
        log_error("SD error log started");
    } else {
        char timestamp[32];
        format_log_timestamp(rtcSyncEpoch, rtcSyncUptimeMs, timestamp, sizeof(timestamp));
        errorLogFile.printf("[%s] SD error log started\n", timestamp);
        errorLogFile.flush();
    }

    return true;
}

static bool open_error_log() {
    if (errorLogFile) {
        return true;
    }
    errorLogFile = SD.open(ERROR_LOG_PATH, FILE_APPEND);
    return static_cast<bool>(errorLogFile);
}

void SDLoggerLoop(void *) {
    if (errorLogQueue == NULL) {
        vTaskDelete(NULL);
        return;
    }

    unsigned long now = millis();
    unsigned long lastMountAttempt = now - SD_LOG_RETRY_INTERVAL_MS;
    unsigned long lastErrorOpenAttempt = now - SD_LOG_RETRY_INTERVAL_MS;
    ErrorLogRecord errorRecord;

    while (true) {
        now = millis();
        if (!sdCardMounted && now - lastMountAttempt >= SD_LOG_RETRY_INTERVAL_MS) {
            lastMountAttempt = now;
            sd_log_init();
        }

        if (sdCardMounted) {
            if (!errorLogFile && now - lastErrorOpenAttempt >= SD_LOG_RETRY_INTERVAL_MS) {
                lastErrorOpenAttempt = now;
                open_error_log();
            }

            if (errorLogFile && rtcTimeValid &&
                xQueueReceive(errorLogQueue, &errorRecord, pdMS_TO_TICKS(100)) == pdTRUE) {
                time_t timestampEpoch = errorRecord.timestamp;
                if (timestampEpoch <= 0) {
                    int32_t deltaMs = static_cast<int32_t>(errorRecord.timestampMs - rtcSyncUptimeMs);
                    int32_t deltaSeconds = deltaMs >= 0 ? deltaMs / 1000 : (deltaMs - 999) / 1000;
                    timestampEpoch = rtcSyncEpoch + deltaSeconds;
                }
                char timestamp[32];
                format_log_timestamp(timestampEpoch, errorRecord.timestampMs, timestamp, sizeof(timestamp));
                errorLogFile.printf("[%s] %s\n", timestamp, errorRecord.message);
                errorLogFile.flush();
                continue;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
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
static bool can_init() {
    twai_general_config_t generalConfig =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&generalConfig, &timingConfig, &filterConfig) != ESP_OK) {
        log_error("TWAI: driver installation failed");
        return false;
    }
    if (twai_start() != ESP_OK) {
        log_error("TWAI: driver start failed");
        twai_driver_uninstall();
        return false;
    }

    uint32_t alerts = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED |
                      TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_TX_FAILED |
                      TWAI_ALERT_BUS_ERROR;
    if (twai_reconfigure_alerts(alerts, nullptr) != ESP_OK) {
        log_error("TWAI: alert configuration failed");
    }
    log_error("TWAI: started at 500 kbps, TX GPIO15, RX GPIO16, normal mode");
    return true;
}

void sendUDSPIDRequest(uint8_t pid) {
    twai_message_t tx_msg = {};
    tx_msg.identifier = 0x7DF;       
    tx_msg.extd = 0;                 
    tx_msg.rtr = 0;                  
    tx_msg.data_length_code = 8;     
    
    tx_msg.data[0] = 0x02;           
    tx_msg.data[1] = 0x01;           
    tx_msg.data[2] = pid;            
    for (int i = 3; i < 8; i++) tx_msg.data[i] = 0xAA; 

    if (twai_transmit(&tx_msg, pdMS_TO_TICKS(10)) != ESP_OK) {
        log_error("TWAI: UDS request transmit failed");
    }
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
    bool driverStarted = false;
    bool busOffRecoveryStarted = false;
    unsigned long lastInitAttempt = millis() - CAN_INIT_RETRY_INTERVAL_MS;
    unsigned long lastUDSPoll = 0;
    unsigned long lastTimeCheck = millis();
    unsigned long lastFuelRateCheck = 0;
    unsigned long lastValidCanResponse = 0;
    uint8_t pollIndex = 0;
    bool canWasActiveThisBoot = false;
    bool fuelLossWriteComplete = false;

    while(true) {
        unsigned long currentMillis = millis();
        if (!driverStarted) {
            if (currentMillis - lastInitAttempt >= CAN_INIT_RETRY_INTERVAL_MS) {
                lastInitAttempt = currentMillis;
                driverStarted = can_init();
            }
            if (!driverStarted) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        uint32_t alerts = 0;
        if (twai_read_alerts(&alerts, 0) == ESP_OK) {
            if ((alerts & TWAI_ALERT_BUS_OFF) != 0 && !busOffRecoveryStarted) {
                log_error("TWAI: bus-off; starting recovery");
                if (twai_initiate_recovery() == ESP_OK) {
                    busOffRecoveryStarted = true;
                }
            }
            if ((alerts & TWAI_ALERT_BUS_RECOVERED) != 0 && busOffRecoveryStarted) {
                if (twai_start() == ESP_OK) {
                    log_error("TWAI: bus recovery completed");
                } else {
                    log_error("TWAI: failed to restart after bus recovery");
                }
                busOffRecoveryStarted = false;
            }
            if ((alerts & TWAI_ALERT_RX_QUEUE_FULL) != 0) {
                log_error("TWAI: receive queue full; frames were dropped");
            }
        }

        twai_message_t rx_msg;

        if (!busOffRecoveryStarted && currentMillis - lastUDSPoll >= UDS_POLL_INTERVAL_MS) {
            const uint8_t polledPids[] = {0x05, 0x0C, 0x0D, 0x5E};
            sendUDSPIDRequest(polledPids[pollIndex]);
            pollIndex = (pollIndex + 1) % 3;
            lastUDSPoll = currentMillis;
        }

        if (!busOffRecoveryStarted && twai_receive(&rx_msg, pdMS_TO_TICKS(5)) == ESP_OK &&
            !rx_msg.extd && !rx_msg.rtr) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);

            if ((rx_msg.identifier == 0x130 || rx_msg.identifier == 0x136) &&
                rx_msg.data_length_code >= 2) {
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
            else if (rx_msg.identifier == 0x7E8 && rx_msg.data_length_code >= 4 &&
                     rx_msg.data[1] == 0x41) {
                canWasActiveThisBoot = true;
                lastValidCanResponse = millis();
                fuelLossWriteComplete = false;
                liveData.canActive = true;

                if (rx_msg.data[2] == 0x05) {
                    liveData.coolantTemp = (int16_t)rx_msg.data[3] - 40;
                }
                else if (rx_msg.data[2] == 0x0C && rx_msg.data_length_code >= 5) {
                    uint16_t rawRpm = (static_cast<uint16_t>(rx_msg.data[3]) << 8) |
                                      rx_msg.data[4];
                    liveData.engineRPM = rawRpm / 4;
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
                else if (rx_msg.data[2] == 0x5E && rx_msg.data_length_code >= 5) {
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

static void close_time_settings_overlay() {
    if (time_settings_overlay != nullptr) {
        lv_obj_delete(time_settings_overlay);
        time_settings_overlay = nullptr;
        time_error_label = nullptr;
    }
}

static void spin_increment_cb(lv_event_t *event) {
    lv_spinbox_increment(static_cast<lv_obj_t *>(lv_event_get_user_data(event)));
}

static void spin_decrement_cb(lv_event_t *event) {
    lv_spinbox_decrement(static_cast<lv_obj_t *>(lv_event_get_user_data(event)));
}

static lv_obj_t *create_time_spin_field(lv_obj_t *parent, const char *title,
                                        int minimum, int maximum, int digits, int value) {
    lv_obj_t *field = lv_obj_create(parent);
    lv_obj_remove_style_all(field);
    lv_obj_set_size(field, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(field, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(field, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(field, 6, 0);
    lv_obj_set_scrollable(field, false);

    lv_obj_t *titleLabel = lv_label_create(field);
    lv_label_set_text(titleLabel, title);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x65D6FF), 0);

    lv_obj_t *stepper = lv_obj_create(field);
    lv_obj_remove_style_all(stepper);
    lv_obj_set_size(stepper, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(stepper, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stepper, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(stepper, 6, 0);
    lv_obj_set_scrollable(stepper, false);

    lv_obj_t *minusButton = lv_button_create(stepper);
    lv_obj_set_size(minusButton, 48, 48);
    lv_obj_t *minusLabel = lv_label_create(minusButton);
    lv_label_set_text(minusLabel, "-");
    lv_obj_set_style_text_font(minusLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(minusLabel);

    lv_obj_t *spinbox = lv_spinbox_create(stepper);
    lv_spinbox_set_range(spinbox, minimum, maximum);
    lv_spinbox_set_digit_format(spinbox, static_cast<uint32_t>(digits), 0);
    lv_spinbox_set_value(spinbox, value);
    lv_obj_set_style_text_font(spinbox, &lv_font_montserrat_32, 0);
    lv_obj_set_size(spinbox, digits > 2 ? 104 : 82, 52);
    lv_obj_set_clickable(spinbox, false);

    lv_obj_t *plusButton = lv_button_create(stepper);
    lv_obj_set_size(plusButton, 48, 48);
    lv_obj_t *plusLabel = lv_label_create(plusButton);
    lv_label_set_text(plusLabel, "+");
    lv_obj_set_style_text_font(plusLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(plusLabel);

    lv_obj_add_event_cb(minusButton, spin_decrement_cb, LV_EVENT_CLICKED, spinbox);
    lv_obj_add_event_cb(plusButton, spin_increment_cb, LV_EVENT_CLICKED, spinbox);
    return spinbox;
}

static void apply_time_settings_cb(lv_event_t *) {
    int year = lv_spinbox_get_value(year_spinbox);
    unsigned month = static_cast<unsigned>(lv_spinbox_get_value(month_spinbox));
    unsigned day = static_cast<unsigned>(lv_spinbox_get_value(day_spinbox));
    int hour = lv_spinbox_get_value(hour_spinbox);
    int minute = lv_spinbox_get_value(minute_spinbox);
    if (!is_valid_calendar_date(year, month, day) || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59) {
        lv_label_set_text(time_error_label, "Invalid date or time");
        return;
    }

    clock_epoch_base = days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60;
    clock_tick_base = lv_tick_get();
    rtc_write_time(year, month, day, hour, minute);
    refresh_clock_label();
    close_time_settings_overlay();
}

static void cancel_time_settings_cb(lv_event_t *) {
    close_time_settings_overlay();
}

static void open_time_settings_overlay(lv_event_t *) {
    if (time_settings_overlay != nullptr) {
        return;
    }

    int64_t epoch = current_clock_epoch();
    int64_t days = epoch >= 0 ? epoch / 86400 : (epoch - 86399) / 86400;
    int64_t secondsOfDay = epoch - days * 86400;
    int year;
    unsigned month, day;
    civil_from_days(days, year, month, day);
    int hour = static_cast<int>(secondsOfDay / 3600);
    int minute = static_cast<int>((secondsOfDay % 3600) / 60);

    time_settings_overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(time_settings_overlay);
    lv_obj_set_size(time_settings_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(time_settings_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(time_settings_overlay, LV_OPA_60, 0);
    lv_obj_set_scrollable(time_settings_overlay, false);
    lv_obj_set_clickable(time_settings_overlay, true);

    lv_obj_t *panel = lv_obj_create(time_settings_overlay);
    lv_obj_set_size(panel, 770, 440);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1A2530), 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(panel, false);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Set date & time");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x65D6FF), 0);

    time_error_label = lv_label_create(panel);
    lv_label_set_text(time_error_label, "");
    lv_obj_set_style_text_color(time_error_label, lv_color_hex(0xFF6B6B), 0);

    lv_obj_t *fieldsRow = lv_obj_create(panel);
    lv_obj_remove_style_all(fieldsRow);
    lv_obj_set_size(fieldsRow, 720, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(fieldsRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(fieldsRow, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(fieldsRow, 12, 0);
    lv_obj_set_style_pad_row(fieldsRow, 12, 0);
    lv_obj_set_scrollable(fieldsRow, false);

    year_spinbox = create_time_spin_field(fieldsRow, "Year", 1970, 2069, 4, year);
    month_spinbox = create_time_spin_field(fieldsRow, "Month", 1, 12, 2, static_cast<int>(month));
    day_spinbox = create_time_spin_field(fieldsRow, "Day", 1, 31, 2, static_cast<int>(day));
    hour_spinbox = create_time_spin_field(fieldsRow, "Hour", 0, 23, 2, hour);
    minute_spinbox = create_time_spin_field(fieldsRow, "Min", 0, 59, 2, minute);

    lv_obj_t *buttonsRow = lv_obj_create(panel);
    lv_obj_remove_style_all(buttonsRow);
    lv_obj_set_size(buttonsRow, 720, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(buttonsRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttonsRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(buttonsRow, false);

    lv_obj_t *cancelButton = lv_button_create(buttonsRow);
    lv_obj_set_size(cancelButton, 150, 56);
    lv_obj_t *cancelLabel = lv_label_create(cancelButton);
    lv_label_set_text(cancelLabel, "Cancel");
    lv_obj_set_style_text_font(cancelLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(cancelLabel);
    lv_obj_add_event_cb(cancelButton, cancel_time_settings_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *setButton = lv_button_create(buttonsRow);
    lv_obj_set_size(setButton, 150, 56);
    lv_obj_t *setLabel = lv_label_create(setButton);
    lv_label_set_text(setLabel, "Set");
    lv_obj_set_style_text_font(setLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(setLabel);
    lv_obj_add_event_cb(setButton, apply_time_settings_cb, LV_EVENT_CLICKED, nullptr);
}

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

    time_t currentTime = time(nullptr);
    clock_epoch_base = currentTime > 0 ? static_cast<int64_t>(currentTime) : 0;
    clock_tick_base = lv_tick_get();
    clock_label = lv_label_create(screen);
    lv_obj_add_style(clock_label, &style_sub_header, 0);
    lv_obj_align(clock_label, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_clickable(clock_label, true);
    lv_obj_add_event_cb(clock_label, open_time_settings_overlay, LV_EVENT_CLICKED, nullptr);
    refresh_clock_label();
}

#ifndef PC_SIMULATOR
void UIProcessingLoop(void * pvParameters) {
    log_error("UI: task entered");
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

        refresh_clock_label();

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

    dataMutex = xSemaphoreCreateMutex();
    touchMutex = xSemaphoreCreateMutex();
    sharedI2cMutex = xSemaphoreCreateMutex();
    errorLogQueue = xQueueCreate(ERROR_LOG_QUEUE_LENGTH, sizeof(ErrorLogRecord));
    if (sharedI2cMutex == NULL) {
        log_error("I2C: failed to create shared bus mutex");
    }

    // Arduino Wire (not the raw esp-idf i2c driver) must own this bus - ESP_Panel_Library's
    // CH422G/GT911 backends communicate over Wire, and BUS_SKIP_INIT_HOST expects it pre-begun.
    if (!Wire.begin(ESP_PANEL_LCD_TOUCH_I2C_IO_SDA, ESP_PANEL_LCD_TOUCH_I2C_IO_SCL)) {
        log_error("I2C: Wire.begin failed");
    } else {
        Wire.setClock(400000);
    }

    log_error("BOOT: setup");
#if ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM
    log_error("CONFIG: custom board enabled");
#else
    log_error("CONFIG: custom board disabled");
#endif
#if ESP_PANEL_BOARD_USE_LCD
    log_error("CONFIG: LCD enabled");
#else
    log_error("CONFIG: LCD disabled");
#endif

    if (!expander_init()) {
        log_error("EXPANDER: initialization failed, touch/backlight may misbehave");
    }
    rtc_sync_system_time();

    if (!hardware_display_init()) {
        log_error("DISPLAY: fatal initialization failure, halting");
        while (true) {
            delay(1000);
        }
    }

    UI_Layout_Init();
    lv_display_enable_invalidation(hardwareDisplay, true);
    lv_obj_invalidate(lv_screen_active());
    log_error("BOOT: UI ready");

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
    if (uiTaskResult != pdPASS) {
        log_error("BOOT: UI task creation failed");
    }

    xTaskCreatePinnedToCore(TouchProcessingLoop, "TouchTask", 3072, NULL, 1, &TouchTaskHandle, 0);
    BaseType_t canTaskResult = xTaskCreatePinnedToCore(
        CANProcessingLoop, "CANTask", 6144, NULL, 1, &CANTaskHandle, 0);
    if (canTaskResult != pdPASS) {
        log_error("BOOT: CAN task creation failed");
    }
    xTaskCreatePinnedToCore(SDLoggerLoop, "SDLogger", 4096, NULL, 0, &SDLoggerTaskHandle, 1);
}

void loop() {
    vTaskDelete(NULL); 
}
#endif
