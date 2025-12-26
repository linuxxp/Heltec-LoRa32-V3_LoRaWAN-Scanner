#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <qrcode.h>
#include "config.h"
#include "gps_manager.h"
#include "battery_manager.h"

// Forward declarations
class LoRaManager;

// =============================================================================
// UI STATE MACHINE
// =============================================================================
enum class UIState : uint8_t {
    SCREEN_STATUS,
    SCREEN_GPS,
    SCREEN_NETWORK,
    SCREEN_INFO,
    SCREEN_QR,          // QR code with LoRaWAN credentials
    MENU_MAIN,
    MENU_EDIT
};

// =============================================================================
// MENU ITEMS
// =============================================================================
enum class MenuItem : uint8_t {
    MODE = 0,
    INTERVAL,
    TX_POWER,
    SPREADING_FACTOR,
    GPS_DISTANCE,
    FORCE_JOIN,
    SHOW_QR,
    ABOUT,
    MENU_COUNT
};

// =============================================================================
// DISPLAY MANAGER CLASS
// =============================================================================
class DisplayManager {
public:
    void begin();
    void update();
    void clear();
    void on();
    void off();

    // State management
    UIState getState() const { return _state; }
    void setState(UIState state);
    void nextScreen();
    void enterMenu();
    void exitMenu();

    // Menu navigation
    void menuNext();
    void menuSelect();
    MenuItem getSelectedItem() const { return _selectedItem; }

    // Data sources (set from main)
    void setGPS(GPSManager* gps) { _gps = gps; }
    void setBattery(BatteryManager* battery) { _battery = battery; }
    void setLoRa(LoRaManager* lora) { _lora = lora; }

    // Status info (set from main loop)
    void setMode(OperationMode mode) { _mode = mode; }
    void setJoined(bool joined) { _joined = joined; }
    void setLastTxTime(uint32_t time) { _lastTxTime = time; }
    void setLastTxSuccess(bool success) { _lastTxSuccess = success; }
    void setTxCount(uint32_t count) { _txCount = count; }
    void setTxFailed(uint32_t count) { _txFailed = count; }
    void setTxPower(int8_t power) { _txPower = power; }
    void setSpreadingFactor(uint8_t sf) { _sf = sf; }
    void setInterval(uint16_t interval) { _interval = interval; }
    void setGpsDistance(uint16_t distance) { _gpsDistance = distance; }

    // Activity tracking
    void resetActivityTimer() { _lastActivity = millis(); }

    // QR Code
    void setQRContent(const String& content) { _qrContent = content; }
    void showQRScreen() { _state = UIState::SCREEN_QR; resetActivityTimer(); }

private:
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C* _display = nullptr;

    UIState _state = UIState::SCREEN_STATUS;
    MenuItem _selectedItem = MenuItem::MODE;
    bool _editing = false;

    // Data sources
    GPSManager* _gps = nullptr;
    BatteryManager* _battery = nullptr;
    LoRaManager* _lora = nullptr;

    // Status info
    OperationMode _mode = DEFAULT_MODE;
    bool _joined = false;
    uint32_t _lastTxTime = 0;
    bool _lastTxSuccess = true;
    uint32_t _txCount = 0;
    uint32_t _txFailed = 0;
    int8_t _txPower = LORA_DEFAULT_POWER;
    uint8_t _sf = LORA_DEFAULT_SF;
    uint16_t _interval = CONTINUOUS_INTERVAL_DEFAULT;
    uint16_t _gpsDistance = AUTO_DISTANCE_DEFAULT;

    // Display state
    bool _displayOn = true;
    uint32_t _lastUpdate = 0;
    uint32_t _lastActivity = 0;

    // QR Code content
    String _qrContent;

    // Drawing functions
    void drawStatusScreen();
    void drawGPSScreen();
    void drawNetworkScreen();
    void drawInfoScreen();
    void drawQRScreen();
    void drawMenu();
    void drawEditValue();

    // Helper functions
    void drawHeader(const char* title);
    void drawBattery(int x, int y);
    void drawProgressBar(int x, int y, int w, int h, int percent);
    const char* getModeString(OperationMode mode);
    const char* getMenuItemName(MenuItem item);
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void DisplayManager::begin() {
    // Enable Vext power for OLED (Heltec V3: LOW = ON)
    pinMode(VEXT_CTRL, OUTPUT);
    digitalWrite(VEXT_CTRL, LOW);
    delay(100);  // Wait for power to stabilize

    // Reset display
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(50);

    // Initialize display with software I2C (more reliable on Heltec V3)
    _display = new U8G2_SSD1306_128X64_NONAME_F_SW_I2C(U8G2_R0, OLED_SCL, OLED_SDA, OLED_RST);
    _display->begin();
    _display->setContrast(DISPLAY_CONTRAST);
    _display->setFont(u8g2_font_6x10_tf);

    // Show boot screen
    _display->clearBuffer();
    _display->drawStr(20, 30, "LoRa Scanner");
    _display->drawStr(35, 45, FIRMWARE_VERSION);
    _display->sendBuffer();

    _lastActivity = millis();

    DEBUG_PRINTLN("[DISP] Initialized");
}

inline void DisplayManager::update() {
    uint32_t now = millis();

    // Check for display timeout
    if (DISPLAY_TIMEOUT_MS > 0 && _displayOn) {
        if ((now - _lastActivity) > DISPLAY_TIMEOUT_MS) {
            off();
            return;
        }
    }

    // Rate limit updates
    if ((now - _lastUpdate) < DISPLAY_UPDATE_MS) {
        return;
    }
    _lastUpdate = now;

    if (!_displayOn) return;

    _display->clearBuffer();

    switch (_state) {
        case UIState::SCREEN_STATUS:
            drawStatusScreen();
            break;
        case UIState::SCREEN_GPS:
            drawGPSScreen();
            break;
        case UIState::SCREEN_NETWORK:
            drawNetworkScreen();
            break;
        case UIState::SCREEN_INFO:
            drawInfoScreen();
            break;
        case UIState::SCREEN_QR:
            drawQRScreen();
            break;
        case UIState::MENU_MAIN:
            drawMenu();
            break;
        case UIState::MENU_EDIT:
            drawEditValue();
            break;
    }

    _display->sendBuffer();
}

inline void DisplayManager::clear() {
    if (_display) {
        _display->clearBuffer();
        _display->sendBuffer();
    }
}

inline void DisplayManager::on() {
    if (_display && !_displayOn) {
        _display->setPowerSave(0);
        _displayOn = true;
        _lastActivity = millis();
    }
}

inline void DisplayManager::off() {
    if (_display && _displayOn) {
        _display->setPowerSave(1);
        _displayOn = false;
    }
}

inline void DisplayManager::setState(UIState state) {
    _state = state;
    resetActivityTimer();
}

inline void DisplayManager::nextScreen() {
    if (_state <= UIState::SCREEN_QR) {
        uint8_t next = ((uint8_t)_state + 1) % NUM_MAIN_SCREENS;
        _state = (UIState)next;
        resetActivityTimer();
    }
}

inline void DisplayManager::enterMenu() {
    _state = UIState::MENU_MAIN;
    _selectedItem = MenuItem::MODE;
    _editing = false;
    resetActivityTimer();
}

inline void DisplayManager::exitMenu() {
    _state = UIState::SCREEN_STATUS;
    _editing = false;
    resetActivityTimer();
}

inline void DisplayManager::menuNext() {
    uint8_t next = ((uint8_t)_selectedItem + 1) % (uint8_t)MenuItem::MENU_COUNT;
    _selectedItem = (MenuItem)next;
    resetActivityTimer();
}

inline void DisplayManager::menuSelect() {
    if (_state == UIState::MENU_MAIN) {
        if (_selectedItem == MenuItem::FORCE_JOIN || _selectedItem == MenuItem::ABOUT) {
            // Action items - handled in main
        } else {
            _state = UIState::MENU_EDIT;
            _editing = true;
        }
    } else if (_state == UIState::MENU_EDIT) {
        _state = UIState::MENU_MAIN;
        _editing = false;
    }
    resetActivityTimer();
}

inline void DisplayManager::drawHeader(const char* title) {
    _display->setFont(u8g2_font_6x10_tf);
    _display->drawStr(0, 10, title);
    drawBattery(100, 0);
    _display->drawHLine(0, 13, 128);
}

inline void DisplayManager::drawBattery(int x, int y) {
    if (!_battery) return;

    uint8_t pct = _battery->getPercent();
    int w = 24;
    int h = 10;

    // Battery outline
    _display->drawFrame(x, y, w, h);
    _display->drawBox(x + w, y + 2, 2, h - 4);

    // Fill level
    int fill = (w - 4) * pct / 100;
    if (fill > 0) {
        _display->drawBox(x + 2, y + 2, fill, h - 4);
    }

    // Percentage text
    char buf[5];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    _display->setFont(u8g2_font_5x7_tf);
    _display->drawStr(x + 5, y + 8, buf);
    _display->setFont(u8g2_font_6x10_tf);
}

inline void DisplayManager::drawStatusScreen() {
    drawHeader("LoRa Scanner");

    char buf[32];

    // Mode
    _display->drawStr(0, 26, "Mode:");
    _display->drawStr(40, 26, getModeString(_mode));

    // GPS Status
    _display->drawStr(0, 38, "GPS:");
    if (_gps && _gps->hasValidFix()) {
        GPSData data = _gps->getData();
        snprintf(buf, sizeof(buf), "%.4f,%.4f", data.latitude, data.longitude);
        _display->drawStr(28, 38, buf);
    } else {
        _display->drawStr(28, 38, "No fix");
    }

    // LoRa Status
    _display->drawStr(0, 50, "LoRa:");
    if (_joined) {
        snprintf(buf, sizeof(buf), "SF%d %ddBm", _sf, _txPower);
        _display->drawStr(32, 50, buf);
    } else {
        _display->drawStr(32, 50, "Not joined");
    }

    // Last TX
    _display->drawStr(0, 62, "TX:");
    if (_lastTxTime > 0) {
        uint32_t ago = (millis() - _lastTxTime) / 1000;
        snprintf(buf, sizeof(buf), "%lus ago %s #%lu", ago, _lastTxSuccess ? "OK" : "FAIL", _txCount);
        _display->drawStr(20, 62, buf);
    } else {
        _display->drawStr(20, 62, "Never");
    }
}

inline void DisplayManager::drawGPSScreen() {
    drawHeader("GPS Details");

    char buf[32];

    if (!_gps) {
        _display->drawStr(0, 35, "GPS not available");
        return;
    }

    GPSData data = _gps->getData();

    // Coordinates
    snprintf(buf, sizeof(buf), "Lat: %.6f", data.latitude);
    _display->drawStr(0, 26, buf);

    snprintf(buf, sizeof(buf), "Lon: %.6f", data.longitude);
    _display->drawStr(0, 38, buf);

    // Altitude
    snprintf(buf, sizeof(buf), "Alt: %.1fm", data.altitude);
    _display->drawStr(0, 50, buf);

    // Satellites and HDOP
    snprintf(buf, sizeof(buf), "Sats:%d HDOP:%.1f", data.satellites, data.hdop);
    _display->drawStr(0, 62, buf);

    // Fix indicator
    if (data.valid) {
        _display->drawStr(100, 62, "FIX");
    } else {
        _display->drawStr(90, 62, "NO FIX");
    }
}

inline void DisplayManager::drawNetworkScreen() {
    drawHeader("Network Stats");

    char buf[32];

    // Join status
    _display->drawStr(0, 26, "Status:");
    _display->drawStr(50, 26, _joined ? "Joined" : "Not joined");

    // TX stats
    snprintf(buf, sizeof(buf), "TX Count: %lu", _txCount);
    _display->drawStr(0, 38, buf);

    snprintf(buf, sizeof(buf), "TX Failed: %lu", _txFailed);
    _display->drawStr(0, 50, buf);

    // Success rate
    if (_txCount > 0) {
        float rate = 100.0f * (_txCount - _txFailed) / _txCount;
        snprintf(buf, sizeof(buf), "Success: %.1f%%", rate);
        _display->drawStr(0, 62, buf);
    }
}

inline void DisplayManager::drawInfoScreen() {
    drawHeader("Device Info");

    char buf[32];

    _display->drawStr(0, 26, DEVICE_NAME);
    snprintf(buf, sizeof(buf), "FW: %s", FIRMWARE_VERSION);
    _display->drawStr(0, 38, buf);

    // Uptime
    uint32_t uptime = millis() / 1000;
    uint32_t hours = uptime / 3600;
    uint32_t mins = (uptime % 3600) / 60;
    uint32_t secs = uptime % 60;
    snprintf(buf, sizeof(buf), "Up: %02lu:%02lu:%02lu", hours, mins, secs);
    _display->drawStr(0, 50, buf);

    // Battery voltage
    if (_battery) {
        snprintf(buf, sizeof(buf), "Bat: %dmV", _battery->getVoltage());
        _display->drawStr(0, 62, buf);
    }
}

inline void DisplayManager::drawMenu() {
    drawHeader("Settings");

    const int startY = 24;
    const int lineHeight = 10;

    for (int i = 0; i < (int)MenuItem::MENU_COUNT && i < 5; i++) {
        MenuItem item = (MenuItem)i;
        int y = startY + i * lineHeight;

        // Highlight selected item
        if (item == _selectedItem) {
            _display->drawStr(0, y, ">");
        }

        _display->drawStr(8, y, getMenuItemName(item));

        // Show current value
        char val[16] = "";
        switch (item) {
            case MenuItem::MODE:
                snprintf(val, sizeof(val), "[%s]", getModeString(_mode));
                break;
            case MenuItem::INTERVAL:
                snprintf(val, sizeof(val), "[%ds]", _interval);
                break;
            case MenuItem::TX_POWER:
                snprintf(val, sizeof(val), "[%ddBm]", _txPower);
                break;
            case MenuItem::SPREADING_FACTOR:
                snprintf(val, sizeof(val), "[SF%d]", _sf);
                break;
            case MenuItem::GPS_DISTANCE:
                snprintf(val, sizeof(val), "[%dm]", _gpsDistance);
                break;
            default:
                break;
        }
        _display->drawStr(70, y, val);
    }
}

inline void DisplayManager::drawEditValue() {
    drawHeader("Edit Value");

    char buf[32];
    snprintf(buf, sizeof(buf), "%s:", getMenuItemName(_selectedItem));
    _display->drawStr(0, 30, buf);

    // Current value (large)
    _display->setFont(u8g2_font_10x20_tf);

    switch (_selectedItem) {
        case MenuItem::MODE: {
            uint8_t modeIdx = (uint8_t)_mode;
            _display->drawStr(20, 55, getModeString(_mode));
            break;
        }
        case MenuItem::INTERVAL:
            snprintf(buf, sizeof(buf), "%d sec", _interval);
            _display->drawStr(20, 55, buf);
            break;
        case MenuItem::TX_POWER:
            snprintf(buf, sizeof(buf), "%d dBm", _txPower);
            _display->drawStr(20, 55, buf);
            break;
        case MenuItem::SPREADING_FACTOR:
            snprintf(buf, sizeof(buf), "SF%d", _sf);
            _display->drawStr(20, 55, buf);
            break;
        case MenuItem::GPS_DISTANCE:
            snprintf(buf, sizeof(buf), "%d m", _gpsDistance);
            _display->drawStr(20, 55, buf);
            break;
        default:
            break;
    }

    _display->setFont(u8g2_font_6x10_tf);
}

inline const char* DisplayManager::getModeString(OperationMode mode) {
    switch (mode) {
        case OperationMode::MANUAL: return "MANUAL";
        case OperationMode::CONTINUOUS: return "CONT";
        case OperationMode::AUTO: return "AUTO";
        case OperationMode::DEEP_SLEEP: return "SLEEP";
        default: return "?";
    }
}

inline const char* DisplayManager::getMenuItemName(MenuItem item) {
    switch (item) {
        case MenuItem::MODE: return "Mode";
        case MenuItem::INTERVAL: return "Interval";
        case MenuItem::TX_POWER: return "TX Power";
        case MenuItem::SPREADING_FACTOR: return "SF";
        case MenuItem::GPS_DISTANCE: return "GPS Dist";
        case MenuItem::FORCE_JOIN: return "Force Join";
        case MenuItem::SHOW_QR: return "Show QR";
        case MenuItem::ABOUT: return "About";
        default: return "?";
    }
}

inline void DisplayManager::drawQRScreen() {
    // Title at top
    _display->setFont(u8g2_font_5x7_tf);
    _display->drawStr(0, 7, "Scan to register:");

    if (_qrContent.length() == 0) {
        _display->setFont(u8g2_font_6x10_tf);
        _display->drawStr(10, 35, "No credentials");
        return;
    }

    // Generate QR code
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];  // Version 3: 29x29 modules

    int8_t err = qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, _qrContent.c_str());

    if (err != 0) {
        _display->setFont(u8g2_font_6x10_tf);
        _display->drawStr(10, 35, "QR Error");
        return;
    }

    // Calculate size and position
    // QR code version 3 = 29x29 modules
    // Display is 128x64, need to fit QR code nicely
    // Scale: 2 pixels per module = 58x58 pixels (too big)
    // Scale: 1 pixel per module = 29x29 pixels (good, leaves room for text)

    const int scale = 2;  // 2 pixels per module
    const int qrSize = qrcode.size * scale;

    // Center QR code horizontally, position below title
    const int offsetX = (128 - qrSize) / 2;
    const int offsetY = 10;

    // Draw QR code
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                if (scale == 1) {
                    _display->drawPixel(offsetX + x, offsetY + y);
                } else {
                    _display->drawBox(offsetX + x * scale, offsetY + y * scale, scale, scale);
                }
            }
        }
    }
}

#endif // DISPLAY_MANAGER_H
