/**
 * LoRaWAN Signal Scanner for Heltec LoRa32 V3
 *
 * Scans LoRaWAN network coverage and sends GPS coordinates via LoRaWAN.
 * Signal strength (RSSI, SNR) is captured by Helium network metadata.
 *
 * Hardware:
 * - Heltec LoRa32 V3 (ESP32-S3 + SX1262)
 * - GT-U7 GPS Module
 * - Built-in OLED SSD1306 128x64
 * - Built-in Li-ion battery support
 *
 * Author: Claude
 * License: MIT
 */

#include <Arduino.h>
#include "config.h"
#include "credentials_generator.h"
#include "button_handler.h"
#include "battery_manager.h"
#include "gps_manager.h"
#include "display_manager.h"
#include "lora_manager.h"
#include "led_manager.h"
#include "payload_encoder.h"

// =============================================================================
// GLOBAL OBJECTS
// =============================================================================
ButtonHandler button;
BatteryManager battery;
GPSManager gps;
DisplayManager display;
LoRaManager lora;
LEDManager led;

// GPS Serial
HardwareSerial GPSSerial(1);

// LoRaWAN Credentials (generated from ESP32 MAC)
LoRaWANCredentials credentials;

// =============================================================================
// APPLICATION STATE
// =============================================================================
struct AppState {
    OperationMode mode = DEFAULT_MODE;
    uint16_t interval = CONTINUOUS_INTERVAL_DEFAULT;
    uint16_t gpsDistance = AUTO_DISTANCE_DEFAULT;

    bool loraJoined = false;
    uint32_t lastTxTime = 0;
    bool pendingTx = false;

    uint32_t txCount = 0;
    uint32_t txFailed = 0;
};

AppState state;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================
void handleButton(ButtonEvent event);
void handleScreenNavigation(ButtonEvent event);
void handleMenuNavigation(ButtonEvent event);
void handleValueEdit(ButtonEvent event);

void triggerMeasurement();
bool sendMeasurement();
void checkAutoTrigger();
void checkContinuousTrigger();

void updateDisplayData();
void saveSettings();
void loadSettings();

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    // Initialize debug serial
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    delay(1000);
    DEBUG_PRINTLN("\n\n=================================");
    DEBUG_PRINTLN("  LoRaWAN Signal Scanner v" FIRMWARE_VERSION);
    DEBUG_PRINTLN("=================================\n");

    // Initialize components
    DEBUG_PRINTLN("[INIT] Starting initialization...");

    // LED - initialize first for status indication
    led.begin(LED_PIN);
    led.setPattern(LEDManager::PATTERN_FAST_BLINK);  // Fast blink during init

    // Button
    button.begin(USER_BUTTON);
    button.setCallback(handleButton);

    // Battery
    battery.begin();

    // Display
    display.begin();
    display.setBattery(&battery);
    display.setGPS(&gps);
    display.setLoRa(&lora);

    delay(1500);  // Show boot screen

    // Generate LoRaWAN credentials from ESP32 MAC
    DEBUG_PRINTLN("[INIT] Generating LoRaWAN credentials...");
    CredentialsGenerator::generate(credentials);

    // Set QR code content for display
    display.setQRContent(CredentialsGenerator::getQRCodeContent());

    // GPS
    display.clear();
    display.update();

    DEBUG_PRINTLN("[INIT] Initializing GPS...");
    gps.begin(GPSSerial, GPS_RX_PIN, GPS_TX_PIN);

    // LoRa
    DEBUG_PRINTLN("[INIT] Initializing LoRa...");
    if (!lora.begin()) {
        DEBUG_PRINTLN("[INIT] LoRa init failed!");
        // Continue anyway, will retry join later
    }

    // Load saved settings
    loadSettings();

    // Update display with initial state
    display.setMode(state.mode);
    display.setInterval(state.interval);
    display.setGpsDistance(state.gpsDistance);
    display.setTxPower(lora.getTxPower());
    display.setSpreadingFactor(lora.getSpreadingFactor());

    // Attempt to join LoRaWAN network
    DEBUG_PRINTLN("[INIT] Joining LoRaWAN network...");
    display.setState(UIState::SCREEN_STATUS);

    if (lora.join()) {
        state.loraJoined = true;
        display.setJoined(true);
        DEBUG_PRINTLN("[INIT] Network joined successfully!");
        led.blink(100, 100, 3);  // Success indication
    } else {
        DEBUG_PRINTLN("[INIT] Join failed - will retry later");
    }

    // Set LED pattern based on status
    if (state.loraJoined) {
        led.setPattern(LEDManager::PATTERN_HEARTBEAT);  // Heartbeat = joined and ready
    } else {
        led.setPattern(LEDManager::PATTERN_SLOW_BLINK);  // Slow blink = not joined
    }

    DEBUG_PRINTLN("[INIT] Initialization complete!\n");
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    // Update all components
    button.update();
    battery.update();
    gps.update();
    lora.update();
    led.update();

    // Check if we need to trigger a measurement based on mode
    switch (state.mode) {
        case OperationMode::MANUAL:
            // Only trigger via button
            break;

        case OperationMode::CONTINUOUS:
            checkContinuousTrigger();
            break;

        case OperationMode::AUTO:
            checkAutoTrigger();
            break;

        case OperationMode::DEEP_SLEEP:
            // Handle in separate function when implemented
            break;
    }

    // Process pending transmission
    if (state.pendingTx && !lora.isBusy()) {
        if (sendMeasurement()) {
            state.pendingTx = false;
        }
    }

    // Update display data
    updateDisplayData();
    display.update();

    // Small delay to prevent tight loop
    delay(10);
}

// =============================================================================
// BUTTON HANDLING
// =============================================================================
void handleButton(ButtonEvent event) {
    if (event == ButtonEvent::NONE) return;

    // Wake display on any button press
    display.on();
    display.resetActivityTimer();

    UIState uiState = display.getState();

    // Handle based on current UI state
    if (uiState <= UIState::SCREEN_QR) {
        handleScreenNavigation(event);
    } else if (uiState == UIState::MENU_MAIN) {
        handleMenuNavigation(event);
    } else if (uiState == UIState::MENU_EDIT) {
        handleValueEdit(event);
    }
}

void handleScreenNavigation(ButtonEvent event) {
    switch (event) {
        case ButtonEvent::SINGLE_CLICK:
            // Next screen
            display.nextScreen();
            break;

        case ButtonEvent::LONG_PRESS:
            // Enter menu
            display.enterMenu();
            break;

        case ButtonEvent::VERY_LONG_PRESS:
            // Force LoRa TX
            DEBUG_PRINTLN("[BTN] Force TX triggered");
            state.pendingTx = true;
            display.showNotification("TX Sent!", 2000);
            break;

        default:
            break;
    }
}

void handleMenuNavigation(ButtonEvent event) {
    switch (event) {
        case ButtonEvent::SINGLE_CLICK:
            // Next menu item
            display.menuNext();
            break;

        case ButtonEvent::LONG_PRESS:
            // Select item or execute action
            {
                MenuItem item = display.getSelectedItem();

                if (item == MenuItem::FORCE_JOIN) {
                    // Force rejoin
                    DEBUG_PRINTLN("[MENU] Force rejoin");
                    display.showNotification("Joining...", 2000);
                    lora.join(true);
                    state.loraJoined = lora.isJoined();
                    display.setJoined(state.loraJoined);
                    if (state.loraJoined) {
                        display.showNotification("Joined!", 1500);
                    } else {
                        display.showNotification("Join Failed", 1500);
                    }
                } else if (item == MenuItem::SHOW_QR) {
                    // Show QR code screen
                    DEBUG_PRINTLN("[MENU] Show QR code");
                    display.showQRScreen();
                } else if (item == MenuItem::ABOUT) {
                    // Show info screen and exit menu
                    display.setState(UIState::SCREEN_INFO);
                } else {
                    // Enter edit mode for settings
                    display.menuSelect();
                }
            }
            break;

        case ButtonEvent::VERY_LONG_PRESS:
            // Exit menu and save
            display.exitMenu();
            saveSettings();
            display.showNotification("Saved!", 1000);
            break;

        default:
            break;
    }
}

void handleValueEdit(ButtonEvent event) {
    MenuItem item = display.getSelectedItem();

    switch (event) {
        case ButtonEvent::SINGLE_CLICK:
            // Increment value
            switch (item) {
                case MenuItem::MODE:
                    state.mode = (OperationMode)(((uint8_t)state.mode + 1) % 4);
                    display.setMode(state.mode);
                    break;

                case MenuItem::INTERVAL:
                    state.interval += 10;
                    if (state.interval > CONTINUOUS_INTERVAL_MAX) {
                        state.interval = CONTINUOUS_INTERVAL_MIN;
                    }
                    display.setInterval(state.interval);
                    break;

                case MenuItem::TX_POWER:
                    {
                        int8_t power = lora.getTxPower() + 2;
                        if (power > 14) power = 2;
                        lora.setTxPower(power);
                        display.setTxPower(lora.getTxPower());
                    }
                    break;

                case MenuItem::SPREADING_FACTOR:
                    {
                        uint8_t sf = lora.getSpreadingFactor() + 1;
                        if (sf > 12) sf = 7;
                        lora.setSpreadingFactor(sf);
                        display.setSpreadingFactor(lora.getSpreadingFactor());
                    }
                    break;

                case MenuItem::GPS_DISTANCE:
                    state.gpsDistance += 10;
                    if (state.gpsDistance > AUTO_DISTANCE_MAX) {
                        state.gpsDistance = AUTO_DISTANCE_MIN;
                    }
                    display.setGpsDistance(state.gpsDistance);
                    break;

                default:
                    break;
            }
            break;

        case ButtonEvent::LONG_PRESS:
            // Confirm and exit edit mode
            display.menuSelect();
            break;

        default:
            break;
    }
}

// =============================================================================
// MEASUREMENT FUNCTIONS
// =============================================================================
void triggerMeasurement() {
    if (!gps.hasValidFix()) {
        DEBUG_PRINTLN("[MEAS] No GPS fix - cannot measure");
        return;
    }

    if (!state.loraJoined) {
        DEBUG_PRINTLN("[MEAS] Not joined - attempting join...");
        if (!lora.join()) {
            return;
        }
        state.loraJoined = true;
        display.setJoined(true);
    }

    state.pendingTx = true;
    DEBUG_PRINTLN("[MEAS] Measurement triggered");
}

bool sendMeasurement() {
    if (!gps.hasValidFix()) {
        DEBUG_PRINTLN("[TX] No GPS fix");
        return false;
    }

    if (!state.loraJoined) {
        DEBUG_PRINTLN("[TX] Not joined");
        return false;
    }

    // Get current data
    GPSData gpsData = gps.getData();
    uint8_t batteryPct = battery.getPercent();
    int8_t txPower = lora.getTxPower();

    // Encode payload
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t len = PayloadEncoder::encode(gpsData, batteryPct, txPower, payload, sizeof(payload));

    if (len == 0) {
        DEBUG_PRINTLN("[TX] Payload encoding failed");
        return false;
    }

    // Debug: print payload
    PayloadEncoder::printPayload(payload, len);

    // Send via LoRaWAN
    bool success = lora.send(payload, len);

    // Update state
    state.lastTxTime = millis();
    display.setLastTxTime(state.lastTxTime);
    display.setLastTxSuccess(success);

    if (success) {
        state.txCount++;
        display.setTxCount(state.txCount);

        // Save position for AUTO mode distance calculation
        gps.savePosition();
    } else {
        state.txFailed++;
        display.setTxFailed(state.txFailed);
    }

    return success;
}

void checkContinuousTrigger() {
    if (state.lastTxTime == 0) {
        // First measurement
        triggerMeasurement();
        return;
    }

    uint32_t elapsed = (millis() - state.lastTxTime) / 1000;
    if (elapsed >= state.interval) {
        triggerMeasurement();
    }
}

void checkAutoTrigger() {
    if (!gps.hasValidFix()) return;

    float distance = gps.distanceFromSaved();

    // First measurement or no saved position
    if (distance < 0) {
        triggerMeasurement();
        return;
    }

    // Check if moved enough
    if (distance >= state.gpsDistance) {
        DEBUG_PRINTF("[AUTO] Distance: %.1fm >= %dm - triggering\n", distance, state.gpsDistance);
        triggerMeasurement();
    }
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================
void updateDisplayData() {
    // Sync joined status from LoRa manager (authoritative source)
    state.loraJoined = lora.isJoined();
    display.setJoined(state.loraJoined);
    display.setTxCount(state.txCount);
    display.setTxFailed(state.txFailed);
    display.setLastTxTime(state.lastTxTime);
    display.setLastTxSuccess(lora.getLastTxSuccess());

    // Update LED pattern based on current status
    if (lora.isBusy()) {
        led.setPattern(LEDManager::PATTERN_FAST_BLINK);  // Fast = busy (joining/sending)
    } else if (!state.loraJoined) {
        led.setPattern(LEDManager::PATTERN_SLOW_BLINK);  // Slow = not joined
    } else if (!gps.hasValidFix()) {
        led.setPattern(LEDManager::PATTERN_SLOW_BLINK);  // Slow = waiting for GPS
    } else {
        led.setPattern(LEDManager::PATTERN_HEARTBEAT);   // Heartbeat = ready
    }
}

void saveSettings() {
    // TODO: Save to NVS (Non-Volatile Storage)
    // Preferences library can be used for this
    DEBUG_PRINTLN("[SAVE] Settings saved (TODO: implement NVS)");
}

void loadSettings() {
    // TODO: Load from NVS
    // For now, use defaults
    DEBUG_PRINTLN("[LOAD] Using default settings (TODO: implement NVS)");
}
