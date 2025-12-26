#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>
#include <RadioLib.h>
#include "config.h"
#include "credentials_generator.h"

// =============================================================================
// LORAWAN STATE
// =============================================================================
enum class LoRaState : uint8_t {
    IDLE,
    JOINING,
    JOINED,
    SENDING,
    WAITING_RX,
    ERROR
};

// Join configuration
#define JOIN_MAX_ATTEMPTS       3       // Max attempts before giving up
#define JOIN_RETRY_DELAY_MS     10000   // 10 seconds between attempts
#define JOIN_TIMEOUT_MS         30000   // 30 seconds timeout per attempt

// =============================================================================
// LORA MANAGER CLASS
// =============================================================================
class LoRaManager {
public:
    bool begin();
    void update();

    // Join network
    bool join(bool force = false);
    bool isJoined() const { return _joined; }

    // Send data
    bool send(const uint8_t* data, size_t len, uint8_t port = LORAWAN_PORT);
    bool isBusy() const { return _state == LoRaState::JOINING || _state == LoRaState::SENDING || _state == LoRaState::WAITING_RX; }

    // Configuration
    void setTxPower(int8_t power);
    void setSpreadingFactor(uint8_t sf);
    int8_t getTxPower() const { return _txPower; }
    uint8_t getSpreadingFactor() const { return _sf; }

    // Status
    LoRaState getState() const { return _state; }
    int16_t getLastError() const { return _lastError; }
    uint32_t getLastTxTime() const { return _lastTxTime; }
    bool getLastTxSuccess() const { return _lastTxSuccess; }

    // Statistics
    uint32_t getTxCount() const { return _txCount; }
    uint32_t getTxFailed() const { return _txFailed; }

private:
    SX1262* _radio = nullptr;
    LoRaWANNode* _node = nullptr;

    LoRaState _state = LoRaState::IDLE;
    int16_t _lastError = 0;

    int8_t _txPower = LORA_DEFAULT_POWER;
    uint8_t _sf = LORA_DEFAULT_SF;

    uint32_t _lastTxTime = 0;
    bool _lastTxSuccess = false;
    uint32_t _txCount = 0;
    uint32_t _txFailed = 0;

    bool _joined = false;
    uint8_t _joinAttempts = 0;
    uint32_t _lastJoinAttempt = 0;
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline bool LoRaManager::begin() {
    DEBUG_PRINTLN("[LORA] Initializing SX1262...");

    // Create radio instance with Heltec V3 pins
    _radio = new SX1262(new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY));

    // Initialize radio
    int16_t state = _radio->begin(
        LORA_FREQ_1,        // frequency
        LORA_DEFAULT_BW,    // bandwidth
        LORA_DEFAULT_SF,    // spreading factor
        LORA_DEFAULT_CR,    // coding rate
        LORA_SYNC_WORD,     // sync word
        _txPower,           // output power
        LORA_PREAMBLE_LEN   // preamble length
    );

    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[LORA] Radio init failed: %d\n", state);
        _lastError = state;
        _state = LoRaState::ERROR;
        return false;
    }

    // Configure for LoRaWAN
    _radio->setDio1Action([]() {
        // DIO1 interrupt handler - managed by RadioLib
    });

    // Set TCXO voltage (Heltec V3 uses TCXO)
    state = _radio->setTCXO(1.8);
    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[LORA] TCXO config warning: %d\n", state);
    }

    // Set DIO2 as RF switch
    state = _radio->setDio2AsRfSwitch(true);
    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[LORA] DIO2 switch warning: %d\n", state);
    }

    // Create LoRaWAN node with EU868 band
    _node = new LoRaWANNode(_radio, &EU868);

    DEBUG_PRINTLN("[LORA] Radio initialized successfully");
    _state = LoRaState::IDLE;
    return true;
}

inline void LoRaManager::update() {
    // Handle async operations if needed
    // RadioLib handles most of this internally
}

inline bool LoRaManager::join(bool force) {
    // Already joined?
    if (_joined && !force) {
        DEBUG_PRINTLN("[LORA] Already joined");
        return true;
    }

    // Check if we're still in joining state (prevent concurrent joins)
    if (_state == LoRaState::JOINING) {
        DEBUG_PRINTLN("[LORA] Join already in progress");
        return false;
    }

    // Rate limit join attempts
    uint32_t now = millis();
    if (!force && _joinAttempts > 0 && (now - _lastJoinAttempt) < JOIN_RETRY_DELAY_MS) {
        DEBUG_PRINTF("[LORA] Waiting before next join attempt (%lu ms remaining)\n",
            JOIN_RETRY_DELAY_MS - (now - _lastJoinAttempt));
        return false;
    }

    // Check max attempts
    if (!force && _joinAttempts >= JOIN_MAX_ATTEMPTS) {
        DEBUG_PRINTLN("[LORA] Max join attempts reached. Use Force Join to retry.");
        return false;
    }

    // Reset attempts if forcing
    if (force) {
        _joinAttempts = 0;
        _joined = false;
    }

    _joinAttempts++;
    _lastJoinAttempt = now;
    _state = LoRaState::JOINING;

    DEBUG_PRINTF("[LORA] Starting OTAA join (attempt %d/%d)...\n", _joinAttempts, JOIN_MAX_ATTEMPTS);

    // Get credentials from generator
    const LoRaWANCredentials& creds = CredentialsGenerator::getCredentials();

    // Convert byte arrays to uint64_t (RadioLib expects this format)
    // EUIs are stored LSB first, need to convert to uint64_t
    uint64_t joinEUI = 0;
    uint64_t devEUI = 0;
    for (int i = 0; i < 8; i++) {
        joinEUI |= ((uint64_t)creds.appEui[i]) << (i * 8);
        devEUI |= ((uint64_t)creds.devEui[i]) << (i * 8);
    }

    DEBUG_PRINTF("[LORA] DevEUI: %s\n", CredentialsGenerator::getDevEuiStr());
    DEBUG_PRINTF("[LORA] JoinEUI: %s\n", CredentialsGenerator::getAppEuiStr());

    // Begin OTAA join
    // For LoRaWAN 1.0.x, nwkKey and appKey are the same
    _node->beginOTAA(joinEUI, devEUI, (uint8_t*)creds.appKey, (uint8_t*)creds.appKey);

    DEBUG_PRINTLN("[LORA] Sending join request...");

    // Try to activate - this sends ONE join request and waits for accept
    int16_t state = _node->activateOTAA();

    if (state == RADIOLIB_ERR_NONE) {
        DEBUG_PRINTLN("[LORA] Join successful!");
        _joined = true;
        _state = LoRaState::JOINED;
        _joinAttempts = 0;  // Reset on success
        return true;
    } else {
        DEBUG_PRINTF("[LORA] Join failed with error: %d\n", state);

        // Decode common errors
        switch (state) {
            case RADIOLIB_ERR_NETWORK_NOT_JOINED:
                DEBUG_PRINTLN("[LORA] -> No Join Accept received");
                break;
            case RADIOLIB_ERR_RX_TIMEOUT:
                DEBUG_PRINTLN("[LORA] -> RX timeout waiting for response");
                break;
            default:
                DEBUG_PRINTLN("[LORA] -> Unknown error");
                break;
        }

        _lastError = state;
        _state = LoRaState::IDLE;  // Go back to IDLE, not ERROR
        _joined = false;
        return false;
    }
}

inline bool LoRaManager::send(const uint8_t* data, size_t len, uint8_t port) {
    if (!_joined) {
        DEBUG_PRINTLN("[LORA] Cannot send - not joined");
        return false;
    }

    if (isBusy()) {
        DEBUG_PRINTLN("[LORA] Cannot send - busy");
        return false;
    }

    DEBUG_PRINTF("[LORA] Sending %d bytes on port %d...\n", len, port);
    _state = LoRaState::SENDING;

    // Send uplink (confirmed = false for now)
    int16_t state = _node->sendReceive(
        (uint8_t*)data,
        len,
        port,
        nullptr,    // No downlink buffer
        nullptr,    // No downlink length
        false       // Unconfirmed uplink
    );

    _lastTxTime = millis();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NO_DOWNLINK) {
        DEBUG_PRINTLN("[LORA] Send successful");
        _lastTxSuccess = true;
        _txCount++;
        _state = LoRaState::JOINED;
        return true;
    } else {
        DEBUG_PRINTF("[LORA] Send failed: %d\n", state);
        _lastError = state;
        _lastTxSuccess = false;
        _txFailed++;
        _state = LoRaState::JOINED;  // Still joined, just failed to send
        return false;
    }
}

inline void LoRaManager::setTxPower(int8_t power) {
    // EU868 max is 14 dBm
    _txPower = constrain(power, 2, 14);

    if (_radio) {
        _radio->setOutputPower(_txPower);
        DEBUG_PRINTF("[LORA] TX power set to %d dBm\n", _txPower);
    }
}

inline void LoRaManager::setSpreadingFactor(uint8_t sf) {
    _sf = constrain(sf, 7, 12);

    if (_radio) {
        _radio->setSpreadingFactor(_sf);
        DEBUG_PRINTF("[LORA] SF set to %d\n", _sf);
    }
}

#endif // LORA_MANAGER_H
