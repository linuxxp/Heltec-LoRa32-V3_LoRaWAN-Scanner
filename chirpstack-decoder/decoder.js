// ChirpStack Payload Decoder for LoRaWAN Signal Scanner
// Device: Heltec LoRa32 V3 with GT-U7 GPS
//
// Payload: 14 bytes on fPort 1
// Bytes 0-3:  Latitude  (int32, ×10^7, big-endian)
// Bytes 4-7:  Longitude (int32, ×10^7, big-endian)
// Bytes 8-9:  Altitude  (int16, meters, big-endian)
// Byte  10:   HDOP      (uint8, ×10)
// Byte  11:   Satellites (uint8)
// Byte  12:   Battery   (uint8, %)
// Byte  13:   TX Power  (int8, dBm)

function decodeUplink(input) {
    var bytes = input.bytes;
    var port = input.fPort;
    var decoded = {};

    if (port === 1 && bytes.length >= 14) {
        // Latitude (bytes 0-3, signed int32, ×10^7)
        var lat = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        if (lat > 0x7FFFFFFF) lat -= 0x100000000;
        decoded.latitude = lat / 1e7;

        // Longitude (bytes 4-7, signed int32, ×10^7)
        var lon = (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7];
        if (lon > 0x7FFFFFFF) lon -= 0x100000000;
        decoded.longitude = lon / 1e7;

        // Altitude (bytes 8-9, signed int16, meters)
        var alt = (bytes[8] << 8) | bytes[9];
        if (alt > 0x7FFF) alt -= 0x10000;
        decoded.altitude = alt;

        // HDOP (byte 10, ×10)
        decoded.hdop = bytes[10] / 10;

        // Satellites (byte 11)
        decoded.satellites = bytes[11];

        // Battery (byte 12, %)
        decoded.battery = bytes[12];

        // TX Power (byte 13, signed int8, dBm)
        var txp = bytes[13];
        if (txp > 127) txp -= 256;
        decoded.txPower = txp;
    }

    return { data: decoded };
}
