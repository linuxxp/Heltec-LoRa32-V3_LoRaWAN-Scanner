#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// DEVICE IDENTIFICATION
// =============================================================================
#define DEVICE_NAME         "LoRa-Scanner"
#define FIRMWARE_VERSION    "1.0.0"

// =============================================================================
// GPS CONFIGURATION (GT-U7 Module)
// =============================================================================
// TODO: Update these pins based on your wiring
#define GPS_RX_PIN          46    // ESP32 RX <- GPS TX
#define GPS_TX_PIN          45    // ESP32 TX -> GPS RX
#define GPS_BAUD            9600

// GPS validity thresholds
#define GPS_MIN_SATELLITES  4
#define GPS_MAX_HDOP        5.0f
#define GPS_FIX_TIMEOUT_MS  120000  // 2 minutes to get fix

// =============================================================================
// LORAWAN CONFIGURATION (EU868)
// =============================================================================
// Frequency plan - EU868
#define LORA_REGION         EU868
#define LORA_FREQ_1         868.1f  // MHz
#define LORA_FREQ_2         868.3f
#define LORA_FREQ_3         868.5f

// Default TX parameters
#define LORA_DEFAULT_SF     7       // Spreading Factor (7-12)
#define LORA_DEFAULT_BW     125.0f  // Bandwidth kHz
#define LORA_DEFAULT_CR     5       // Coding Rate (5=4/5, 8=4/8)
#define LORA_DEFAULT_POWER  14      // dBm (max 14 for EU868)
#define LORA_PREAMBLE_LEN   8
#define LORA_SYNC_WORD      0x34    // LoRaWAN public network

// LoRaWAN port for uplink
#define LORAWAN_PORT        1

// =============================================================================
// OPERATION MODES
// =============================================================================
enum class OperationMode : uint8_t {
    MANUAL      = 0,    // Button press triggers measurement
    CONTINUOUS  = 1,    // Periodic measurements
    AUTO        = 2,    // Movement-based measurements
    DEEP_SLEEP  = 3     // Low power periodic wake
};

// Default mode
#define DEFAULT_MODE        OperationMode::AUTO

// =============================================================================
// TIMING CONFIGURATION
// =============================================================================
// Continuous mode interval (seconds)
#define CONTINUOUS_INTERVAL_MIN     10
#define CONTINUOUS_INTERVAL_MAX     3600
#define CONTINUOUS_INTERVAL_DEFAULT 30

// Auto mode - minimum distance to trigger new measurement (meters)
#define AUTO_DISTANCE_MIN           10
#define AUTO_DISTANCE_MAX           500
#define AUTO_DISTANCE_DEFAULT       50

// Deep sleep interval (seconds)
#define DEEP_SLEEP_INTERVAL_MIN     60
#define DEEP_SLEEP_INTERVAL_MAX     3600
#define DEEP_SLEEP_INTERVAL_DEFAULT 300

// =============================================================================
// BUTTON CONFIGURATION
// =============================================================================
#define BTN_DEBOUNCE_MS         50
#define BTN_DOUBLE_CLICK_MS     300
#define BTN_LONG_PRESS_MS       1000
#define BTN_VERY_LONG_PRESS_MS  3000

// =============================================================================
// DISPLAY CONFIGURATION
// =============================================================================
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          64
#define DISPLAY_UPDATE_MS       250     // Refresh rate
#define DISPLAY_TIMEOUT_MS      30000   // Auto-off after inactivity (0=never)
#define DISPLAY_CONTRAST        255     // 0-255

// Number of main screens
#define NUM_SCREENS             4

// Screen indices
#define SCREEN_STATUS           0
#define SCREEN_GPS              1
#define SCREEN_NETWORK          2
#define SCREEN_INFO             3

// =============================================================================
// BATTERY CONFIGURATION
// =============================================================================
// Heltec V3 battery ADC calibration
#define BATTERY_FULL_MV         4200
#define BATTERY_EMPTY_MV        3200
#define BATTERY_ADC_SAMPLES     32
#define BATTERY_UPDATE_MS       10000

// =============================================================================
// PAYLOAD CONFIGURATION
// =============================================================================
// Payload size: 14 bytes
// Byte 0-3:   Latitude (int32, ×10^7)
// Byte 4-7:   Longitude (int32, ×10^7)
// Byte 8-9:   Altitude (int16, meters)
// Byte 10:    HDOP (uint8, ×10)
// Byte 11:    Satellites (uint8)
// Byte 12:    Battery (uint8, %)
// Byte 13:    TX Power (int8, dBm)
#define PAYLOAD_SIZE            14

// =============================================================================
// TASK PRIORITIES (FreeRTOS)
// =============================================================================
#define TASK_PRIORITY_GPS       2
#define TASK_PRIORITY_LORA      3
#define TASK_PRIORITY_DISPLAY   1
#define TASK_PRIORITY_BUTTON    2

// Task stack sizes (words)
#define TASK_STACK_GPS          4096
#define TASK_STACK_LORA         8192
#define TASK_STACK_DISPLAY      4096
#define TASK_STACK_BUTTON       2048

// =============================================================================
// DEBUG CONFIGURATION
// =============================================================================
#define DEBUG_SERIAL            Serial
#define DEBUG_BAUD              115200

#ifdef CORE_DEBUG_LEVEL
    #if CORE_DEBUG_LEVEL >= 3
        #define DEBUG_PRINT(x)      DEBUG_SERIAL.print(x)
        #define DEBUG_PRINTLN(x)    DEBUG_SERIAL.println(x)
        #define DEBUG_PRINTF(...)   DEBUG_SERIAL.printf(__VA_ARGS__)
    #else
        #define DEBUG_PRINT(x)
        #define DEBUG_PRINTLN(x)
        #define DEBUG_PRINTF(...)
    #endif
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H
