#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>
#include <RadioLib.h>
#include "config.h"
#include "credentials.h"

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

// =============================================================================
// LORA MANAGER CLASS
// =============================================================================
class LoRaManager {
public:
    bool begin();
    void update();

    // Join network
    bool join(bool force = false);
    bool isJoined() const { return _state == LoRaState::JOINED || _state == LoRaState::IDLE; }

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

    // Session persistence
    uint8_t _nwkSKey[16];
    uint8_t _appSKey[16];
    uint32_t _devAddr;
    uint32_t _fCntUp;
    uint32_t _fCntDown;
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

    // Create LoRaWAN node
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
    if (_joined && !force) {
        DEBUG_PRINTLN("[LORA] Already joined");
        return true;
    }

    DEBUG_PRINTLN("[LORA] Starting OTAA join...");
    _state = LoRaState::JOINING;

    // Begin OTAA join
    int16_t state = _node->beginOTAA(APPEUI, DEVEUI, APPKEY);

    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[LORA] Begin OTAA failed: %d\n", state);
        _lastError = state;
        _state = LoRaState::ERROR;
        return false;
    }

    // Attempt to join (blocking, with timeout)
    DEBUG_PRINTLN("[LORA] Sending join request...");

    // Try to activate - this sends join request and waits for accept
    state = _node->activateOTAA();

    if (state == RADIOLIB_ERR_NONE) {
        DEBUG_PRINTLN("[LORA] Join successful!");
        _joined = true;
        _state = LoRaState::JOINED;

        // Save session keys for potential restoration
        // Note: In production, save to NVS for persistence across reboots

        return true;
    } else {
        DEBUG_PRINTF("[LORA] Join failed: %d\n", state);
        _lastError = state;
        _state = LoRaState::ERROR;
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
