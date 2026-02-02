// Node-RED Function Node for LoRaWAN Signal Scanner
// Processes ChirpStack MQTT uplink events and formats data for InfluxDB v2
//
// Input:  ChirpStack JSON event (from MQTT node subscribed to application/+/device/+/event/up)
// Output: [fields, tags] array for InfluxDB v2 node

let data = msg.payload;

// Parse JSON string if needed
if (typeof data === "string") {
    try {
        data = JSON.parse(data);
    } catch (e) {
        node.error("Invalid JSON payload", msg);
        return null;
    }
}

// ---- Device metadata from ChirpStack event ----
const devEui = data?.deviceInfo?.devEui || "unknown";
const deviceName = data?.deviceInfo?.deviceName || "unknown";
const appName = data?.deviceInfo?.applicationName || "unknown";
const fCnt = (typeof data?.fCnt === "number") ? data.fCnt : null;
const fPort = (typeof data?.fPort === "number") ? data.fPort : null;
const dr = (typeof data?.dr === "number") ? data.dr : null;
const confirmed = !!data?.confirmed;

// Decoded object from ChirpStack codec
const o = data?.object || {};

// ---- Device ID mapping ----
// Map known devEui values to friendly names
let devID;
if (devEui === "fcad565b79569dde") {
    devID = "HELIUM_TRACKER_1";
} else {
    devID = deviceName !== "unknown" ? deviceName : "UNKNOWN";
}

// ---- Fields (values stored in InfluxDB) ----
const fields = {};

// GPS data from decoded payload
if (typeof o.latitude === "number")  fields.latitude = o.latitude;
if (typeof o.longitude === "number") fields.longitude = o.longitude;
if (typeof o.altitude === "number")  fields.altitude = o.altitude;
if (typeof o.hdop === "number")      fields.hdop = o.hdop;

// Satellites - always integer
if (o.satellites !== undefined && o.satellites !== null) {
    const sats = parseInt(o.satellites, 10);
    if (Number.isFinite(sats)) fields.satellites = sats;
}

// Device status from decoded payload
if (typeof o.battery === "number")   fields.battery = parseInt(o.battery, 10);
if (typeof o.txPower === "number")   fields.txPower = parseInt(o.txPower, 10);

if (typeof o.mapUrl === "string")    fields.mapUrl = o.mapUrl;

// LoRaWAN metadata
if (fCnt !== null)  fields.fCnt = fCnt;
if (fPort !== null) fields.fPort = fPort;
if (dr !== null)    fields.dr = dr;
fields.confirmed = confirmed ? 1 : 0;

// ---- Gateway signal data (from rxInfo) ----
if (Array.isArray(data?.rxInfo) && data.rxInfo.length > 0) {
    let bestRssi = -999;
    let bestSnr = -99;
    let bestGatewayName = "unknown";

    for (const rx of data.rxInfo) {
        if (rx.rssi > bestRssi) {
            bestRssi = rx.rssi;
            bestSnr = (typeof rx.snr === "number") ? rx.snr : 0;
            // Gateway name from Helium metadata or gatewayId as fallback
            bestGatewayName = rx.metadata?.gateway_name
                || rx.gatewayId
                || "unknown";
        }
    }

    fields.rssi = bestRssi;
    fields.snr = bestSnr;
    fields.gatewayCount = data.rxInfo.length;
    fields.bestGateway = bestGatewayName;
}

// Skip if nothing to write
if (Object.keys(fields).length === 0) {
    node.warn("No fields extracted from payload; skipping write");
    return null;
}

// ---- Tags (indexed in InfluxDB) ----
const tags = {
    devEui: devEui,
    devID: devID,
    application: appName
};

// Final payload for InfluxDB v2 node
msg.payload = [fields, tags];

return msg;
