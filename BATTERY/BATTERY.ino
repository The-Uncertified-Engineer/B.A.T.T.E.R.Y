/*  =========================================================================
 *  B.A.T.T.E.R.Y.  —  v1.0
 *  Basically A Toy That Embodies Rechargeable Yet-again-batteries
 *  =========================================================================
*/

#include <Wire.h>
#include <U8g2lib.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include "charging_battery_64_64_28f.h"

/* =======================================================================
 *  PIN & DISPLAY CONFIGURATION
 * ===================================================================== */
#define BAT_PIN          34          // ADC1_CH6
#define I2C_SDA          21
#define I2C_SCL          22

/* =======================================================================
 *  ADC / SAMPLING CONFIGURATION
 * ===================================================================== */
#define ADC_SAMPLES      32          // oversample count per reading
#define MA_SAMPLES       10          // moving-average window
#define READ_INTERVAL_MS 50          // sample every 50 ms
#define VOLTAGE_CALIBRATION  1.0f

/* =======================================================================
 *  AA ALKALINE THRESHOLDS (volts)
 * ===================================================================== */
#define V_FULL           1.60        // >= 100%
#define V_EMPTY          1.00        // <= 0%
#define V_NO_BATTERY     0.20
#define V_DEAD           1.05
#define V_LOW            1.35

/* =======================================================================
 *  U8G2 DISPLAY OBJECT
 * ===================================================================== */
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0,
                                          /* reset=*/U8X8_PIN_NONE,
                                          /* clock=*/I2C_SCL,
                                          /* data=*/I2C_SDA);

/* =======================================================================
 *  BATTERY DATA STRUCT
 * ===================================================================== */
enum BatteryStatus {
    STATUS_NO_BATTERY = 0,
    STATUS_DEAD,
    STATUS_LOW,
    STATUS_HEALTHY
};

struct BatteryData {
    /* --- Raw numeric values --- */
    int           rawAdc;
    float         voltage;
    int           percent;
    BatteryStatus status;

    /* --- Pre-formatted strings for direct u8g2.drawStr() --- */
    char  voltageStr[12];   // "1.54V"
    char  percentStr[8];    // "88%"
    char  statusStr[16];    // "Good" / "Low" / "Replace" / "No Battery"
    char  rawAdcStr[10];
    bool  adcCalibrated;
};

/* =======================================================================
 *  GLOBAL STATE
 * ===================================================================== */
BatteryData data;

esp_adc_cal_characteristics_t adcChars;
bool                          adcCalibrated = false;

float maBuffer[MA_SAMPLES];
int   maIndex        = 0;
bool  maBufferFilled = false;

unsigned long lastReadTime = 0;

/* =======================================================================
 *  ADC CALIBRATION (one-time, in setup)
 * ===================================================================== */
void calibrateADC() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_11,
        ADC_WIDTH_BIT_12,
        1100,
        &adcChars);

    switch (val_type) {
        case ESP_ADC_CAL_VAL_EFUSE_VREF:
            Serial.println(F("ADC: calibrated using eFuse Vref"));
            adcCalibrated = true;
            break;
        case ESP_ADC_CAL_VAL_EFUSE_TP:
            Serial.println(F("ADC: calibrated using eFuse Two-Point"));
            adcCalibrated = true;
            break;
        default:
            Serial.println(F("ADC: no eFuse calibration - using default Vref."));
            Serial.println(F("    Tune VOLTAGE_CALIBRATION if readings are off."));
            adcCalibrated = false;
            break;
    }
}

/* =======================================================================
 *  READ CALIBRATED VOLTAGE (32x oversample + min/max rejection)
 * ===================================================================== */
float readCalibratedVoltage() {
    long sum = 0;
    int  minV = 4095;
    int  maxV = 0;

    for (int i = 0; i < ADC_SAMPLES; i++) {
        int raw = adc1_get_raw(ADC1_CHANNEL_6);
        if (raw < minV) minV = raw;
        if (raw > maxV) maxV = raw;
        sum += raw;
        delayMicroseconds(150);
    }

    sum -= minV;
    sum -= maxV;
    int avgRaw = (int)(sum / (ADC_SAMPLES - 2));

    data.rawAdc = avgRaw;

    float v;
    if (adcCalibrated) {
        uint32_t mv = esp_adc_cal_raw_to_voltage(avgRaw, &adcChars);
        v = (float)mv / 1000.0f;
    } else {
        v = ((float)avgRaw / 4095.0f) * 3.3f;
    }
    v *= VOLTAGE_CALIBRATION;
    return v;
}

/* =======================================================================
 *  MOVING AVERAGE
 * ===================================================================== */
float updateMovingAverage(float v) {
    maBuffer[maIndex] = v;
    maIndex = (maIndex + 1) % MA_SAMPLES;
    if (maIndex == 0) maBufferFilled = true;

    int count = maBufferFilled ? MA_SAMPLES : maIndex;
    float sum = 0.0f;
    for (int i = 0; i < count; i++) sum += maBuffer[i];
    return sum / (float)count;
}

/* =======================================================================
 *  BATTERY MATH
 * ===================================================================== */
int calcPercent(float v) {
    if (v >= V_FULL)  return 100;
    if (v <= V_EMPTY) return 0;
    return (int)(((v - V_EMPTY) / (V_FULL - V_EMPTY)) * 100.0f);
}

BatteryStatus calcStatus(float v) {
    if (v <  V_NO_BATTERY) return STATUS_NO_BATTERY;
    if (v <  V_DEAD)       return STATUS_DEAD;
    if (v <  V_LOW)        return STATUS_LOW;
    return STATUS_HEALTHY;
}

/* =======================================================================
 *  UPDATE BATTERY DATA  —  populates the global `data` struct
 * ===================================================================== */
void updateBatteryData() {
    float v = readCalibratedVoltage();
    float smoothed = updateMovingAverage(v);

    data.voltage = smoothed;
    data.percent = calcPercent(smoothed);
    data.status  = calcStatus(smoothed);
    data.adcCalibrated = adcCalibrated;

    snprintf(data.voltageStr, sizeof(data.voltageStr), "%.2fV", data.voltage);
    snprintf(data.percentStr, sizeof(data.percentStr), "%d%%", data.percent);
    snprintf(data.rawAdcStr,  sizeof(data.rawAdcStr),  "%d",   data.rawAdc);

    switch (data.status) {
        case STATUS_NO_BATTERY: strncpy(data.statusStr, "No Battery", sizeof(data.statusStr)); break;
        case STATUS_DEAD:       strncpy(data.statusStr, "Replace",    sizeof(data.statusStr)); break;
        case STATUS_LOW:        strncpy(data.statusStr, "Low",        sizeof(data.statusStr)); break;
        case STATUS_HEALTHY:    strncpy(data.statusStr, "Good",       sizeof(data.statusStr)); break;
    }
    data.statusStr[sizeof(data.statusStr) - 1] = '\0';
}

/* =======================================================================
 *  ANIMATION FRAME COUNTER
 * ===================================================================== */
void updateAnimations() {
    charging_battery_64_64_28f_frame = (millis() / 42) % 28;
}

/* =======================================================================
 *  ANIMATION DRAW
 * ===================================================================== */
void drawAnimation_charging_battery_64_64_28f(void) {
    u8g2.setDrawColor(1);
    u8g2.drawXBMP(-4, 8, 64, 64,
                  charging_battery_64_64_28f_frames[charging_battery_64_64_28f_frame]);
}

/* =======================================================================
 *  MAIN SCREEN  —  Lopaka layout, with REAL battery data substituted
 * ===================================================================== */
void drawScreen_1(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);           

    // --- Title ---
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(44, 9, "BATTERY");
    u8g2.drawLine(40, 12, 87, 12);

    // --- Subtle divider ---
    u8g2.drawLine(45, 15, 83, 15);

    // --- Health label + percent value ---
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(64, 28, "Health");
    u8g2.drawStr(96, 28, data.percentStr);   // <-- real percent, e.g. "88%"

    // --- Voltage ---
    u8g2.drawStr(64, 38, data.voltageStr);   // <-- real voltage, e.g. "1.54V"

    // --- Status framed box ---
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawFrame(61, 47, 58, 12);
    u8g2.drawStr(70 , 56, data.statusStr);    // <-- real status, e.g. "Good"

    drawAnimation_charging_battery_64_64_28f();
}

/* =======================================================================
 *  SETUP
 * ===================================================================== */
void setup() {
    Serial.begin(115200);
    Serial.println(F("B.A.T.T.E.R.Y v1.0 booting..."));

    // --- ADC calibration ---
    calibrateADC();

    // --- U8g2 init ---
    u8g2.begin();

    // --- Prime the moving-average buffer ---
    for (int i = 0; i < MA_SAMPLES; i++) {
        updateMovingAverage(readCalibratedVoltage());
    }
    updateBatteryData();

    lastReadTime = millis();
}

/* =======================================================================
 *  MAIN LOOP
 * ===================================================================== */
void loop() {
    unsigned long now = millis();

    // --- 1. Sample ADC ---
    if (now - lastReadTime >= READ_INTERVAL_MS) {
        lastReadTime = now;
        updateBatteryData();
    }

    // --- 2. Animation frame advance ---
    updateAnimations();

    // --- 3. Render (page-buffer mode) ---
    u8g2.firstPage();
    do {
        drawScreen_1();
    } while (u8g2.nextPage());
}
