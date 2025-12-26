#ifndef CREDENTIALS_GENERATOR_H
#define CREDENTIALS_GENERATOR_H

#include <Arduino.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include "config.h"

// =============================================================================
// LORAWAN CREDENTIALS GENERATOR
// =============================================================================
// Supports two modes:
// 1. Manual mode (USE_MANUAL_CREDENTIALS=true): Uses credentials from config.h
// 2. Auto mode (USE_MANUAL_CREDENTIALS=false): Generates from ESP32 MAC address
//
// Manual credentials are taken from Helium Console in MSB format.
// Auto-generated credentials:
//   DevEUI:  Derived from WiFi MAC address (unique per chip)
//   AppEUI:  Fixed for the project (all devices share this)
//   AppKey:  SHA256 hash of MAC + secret seed (unique per chip)

// Secret seed for AppKey generation (only used in auto mode)
#define APPKEY_SEED "LoRaWAN-Scanner-2024-Secret"

// Fixed AppEUI for auto-generated credentials
static const uint8_t FIXED_APPEUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Manual credentials from config.h (MSB format)
#if USE_MANUAL_CREDENTIALS
static const uint8_t MANUAL_DEV_EUI_ARRAY[8] = MANUAL_DEV_EUI;
static const uint8_t MANUAL_JOIN_EUI_ARRAY[8] = MANUAL_JOIN_EUI;
static const uint8_t MANUAL_APP_KEY_ARRAY[16] = MANUAL_APP_KEY;
#endif

// =============================================================================
// CREDENTIALS STRUCTURE
// =============================================================================
struct LoRaWANCredentials {
    uint8_t devEui[8];      // LSB first (for RadioLib)
    uint8_t appEui[8];      // LSB first (for RadioLib)
    uint8_t appKey[16];     // MSB (as shown in console)

    char devEuiStr[24];     // "XX:XX:XX:XX:XX:XX:XX:XX"
    char appEuiStr[24];     // "XX:XX:XX:XX:XX:XX:XX:XX"
    char appKeyStr[48];     // "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
};

// =============================================================================
// CREDENTIALS GENERATOR CLASS
// =============================================================================
class CredentialsGenerator {
public:
    // Generate credentials based on ESP32 MAC
    static void generate(LoRaWANCredentials& creds);

    // Get pointers for RadioLib (returns LSB format)
    static const uint8_t* getDevEui() { return _creds.devEui; }
    static const uint8_t* getAppEui() { return _creds.appEui; }
    static const uint8_t* getAppKey() { return _creds.appKey; }

    // Get string representations (for display/QR)
    static const char* getDevEuiStr() { return _creds.devEuiStr; }
    static const char* getAppEuiStr() { return _creds.appEuiStr; }
    static const char* getAppKeyStr() { return _creds.appKeyStr; }

    // Get full credentials struct
    static const LoRaWANCredentials& getCredentials() { return _creds; }

    // Generate QR code content string
    static String getQRCodeContent();

    // Check if credentials have been generated
    static bool isGenerated() { return _generated; }

private:
    static LoRaWANCredentials _creds;
    static bool _generated;

    static void getMacAddress(uint8_t* mac);
    static void generateDevEui(const uint8_t* mac);
    static void generateAppKey(const uint8_t* mac);
    static void formatStrings();
    static void bytesToHexString(const uint8_t* bytes, size_t len, char* str, bool colonSeparated);
};

// Static member initialization
LoRaWANCredentials CredentialsGenerator::_creds;
bool CredentialsGenerator::_generated = false;

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void CredentialsGenerator::generate(LoRaWANCredentials& creds) {
#if USE_MANUAL_CREDENTIALS
    // Use manually configured credentials from config.h
    DEBUG_PRINTLN("[CRED] Using MANUAL credentials from config.h");

    // Copy DevEUI (MSB from config -> LSB for RadioLib)
    for (int i = 0; i < 8; i++) {
        _creds.devEui[i] = MANUAL_DEV_EUI_ARRAY[7 - i];
    }

    // Copy JoinEUI/AppEUI (MSB from config -> LSB for RadioLib)
    for (int i = 0; i < 8; i++) {
        _creds.appEui[i] = MANUAL_JOIN_EUI_ARRAY[7 - i];
    }

    // Copy AppKey (MSB, stays the same)
    memcpy(_creds.appKey, MANUAL_APP_KEY_ARRAY, 16);

#else
    // Auto-generate credentials from ESP32 MAC
    DEBUG_PRINTLN("[CRED] Using AUTO-GENERATED credentials from MAC");

    uint8_t mac[6];
    getMacAddress(mac);

    DEBUG_PRINTF("[CRED] MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Generate DevEUI from MAC
    generateDevEui(mac);

    // Copy fixed AppEUI (LSB first for RadioLib)
    for (int i = 0; i < 8; i++) {
        _creds.appEui[i] = FIXED_APPEUI[7 - i];
    }

    // Generate AppKey from MAC + seed
    generateAppKey(mac);
#endif

    // Format string representations
    formatStrings();

    // Copy to output
    memcpy(&creds, &_creds, sizeof(LoRaWANCredentials));
    _generated = true;

    DEBUG_PRINTLN("[CRED] Credentials ready:");
    DEBUG_PRINTF("[CRED] DevEUI: %s\n", _creds.devEuiStr);
    DEBUG_PRINTF("[CRED] AppEUI: %s\n", _creds.appEuiStr);
    DEBUG_PRINTF("[CRED] AppKey: %s\n", _creds.appKeyStr);
}

inline void CredentialsGenerator::getMacAddress(uint8_t* mac) {
    // Get the base MAC address from ESP32
    esp_efuse_mac_get_default(mac);
}

inline void CredentialsGenerator::generateDevEui(const uint8_t* mac) {
    // DevEUI is 8 bytes. We use MAC (6 bytes) + 0xFF padding
    // Format: MAC[0:2] + 0xFF + 0xFE + MAC[3:5] (EUI-64 format)
    // Store in LSB first order for RadioLib

    uint8_t devEuiMsb[8];
    devEuiMsb[0] = mac[0];
    devEuiMsb[1] = mac[1];
    devEuiMsb[2] = mac[2];
    devEuiMsb[3] = 0xFF;
    devEuiMsb[4] = 0xFE;
    devEuiMsb[5] = mac[3];
    devEuiMsb[6] = mac[4];
    devEuiMsb[7] = mac[5];

    // Convert to LSB first for RadioLib
    for (int i = 0; i < 8; i++) {
        _creds.devEui[i] = devEuiMsb[7 - i];
    }
}

inline void CredentialsGenerator::generateAppKey(const uint8_t* mac) {
    // Generate AppKey using SHA256(MAC + SEED)
    // Take first 16 bytes of the hash

    uint8_t hash[32];
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (not SHA224)

    // Add MAC address
    mbedtls_sha256_update(&ctx, mac, 6);

    // Add seed
    mbedtls_sha256_update(&ctx, (const uint8_t*)APPKEY_SEED, strlen(APPKEY_SEED));

    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    // AppKey is first 16 bytes of hash (MSB order, as shown in console)
    memcpy(_creds.appKey, hash, 16);
}

inline void CredentialsGenerator::formatStrings() {
    // DevEUI string (MSB order for display, colon separated)
    uint8_t devEuiMsb[8];
    for (int i = 0; i < 8; i++) {
        devEuiMsb[i] = _creds.devEui[7 - i];
    }
    bytesToHexString(devEuiMsb, 8, _creds.devEuiStr, true);

    // AppEUI string (MSB order for display, colon separated)
    uint8_t appEuiMsb[8];
    for (int i = 0; i < 8; i++) {
        appEuiMsb[i] = _creds.appEui[7 - i];
    }
    bytesToHexString(appEuiMsb, 8, _creds.appEuiStr, true);

    // AppKey string (already MSB, no colons)
    bytesToHexString(_creds.appKey, 16, _creds.appKeyStr, false);
}

inline void CredentialsGenerator::bytesToHexString(const uint8_t* bytes, size_t len, char* str, bool colonSeparated) {
    const char hex[] = "0123456789ABCDEF";
    size_t strIdx = 0;

    for (size_t i = 0; i < len; i++) {
        str[strIdx++] = hex[(bytes[i] >> 4) & 0x0F];
        str[strIdx++] = hex[bytes[i] & 0x0F];

        if (colonSeparated && i < len - 1) {
            str[strIdx++] = ':';
        }
    }
    str[strIdx] = '\0';
}

inline String CredentialsGenerator::getQRCodeContent() {
    // Create a compact string for QR code
    // Format: LORA:DevEUI;AppEUI;AppKey
    // Using semicolon separator and no colons in EUIs for compactness

    String content = "LORA:";

    // DevEUI without colons
    uint8_t devEuiMsb[8];
    for (int i = 0; i < 8; i++) {
        devEuiMsb[i] = _creds.devEui[7 - i];
    }
    for (int i = 0; i < 8; i++) {
        char buf[3];
        sprintf(buf, "%02X", devEuiMsb[i]);
        content += buf;
    }

    content += ";";

    // AppEUI without colons
    uint8_t appEuiMsb[8];
    for (int i = 0; i < 8; i++) {
        appEuiMsb[i] = _creds.appEui[7 - i];
    }
    for (int i = 0; i < 8; i++) {
        char buf[3];
        sprintf(buf, "%02X", appEuiMsb[i]);
        content += buf;
    }

    content += ";";

    // AppKey
    content += _creds.appKeyStr;

    return content;
}

#endif // CREDENTIALS_GENERATOR_H
