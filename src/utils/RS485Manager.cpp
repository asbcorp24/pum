#include "RS485Manager.h"

// Если используем Serial2:
#define RS485_UART_NUM 2

void RS485Manager::begin(uint8_t rxPin, uint8_t txPin, uint32_t baud, uint8_t dePin) {
    _dePin = dePin;
    pinMode(_dePin, OUTPUT);
    digitalWrite(_dePin, LOW); // режим приёма

    // Инициализируем Serial2 (UART2) на заданных пинах
    Serial2.begin(baud, SERIAL_8N1, rxPin, txPin);
    _serial = &Serial2;
}

bool RS485Manager::available() {
    if (!_serial) return false;
    return (_serial->available() > 0);
}

void RS485Manager::setTimeout(uint16_t ms) {
    _timeout = ms;
}

bool RS485Manager::isConnected() const {
    return (_serial != nullptr);
}

void RS485Manager::_enableTransmit() {
    digitalWrite(_dePin, HIGH);
    delayMicroseconds(10); // небольшой буфер, чтобы трансивер переключился
}

void RS485Manager::_enableReceive() {
    delayMicroseconds(10); 
    digitalWrite(_dePin, LOW);
}

// CRC8 (полином 0x07) для массива байт
uint8_t RS485Manager::_calcCRC8(const uint8_t* data, size_t len) const {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Отправка одного пакета
bool RS485Manager::sendPacket(const RS485Packet& pkt) {
    if (!_serial) return false;

    uint8_t payload[17];
    size_t idx = 0;

    payload[idx++] = pkt.client_id;

    uint32_t cow = pkt.cow_id;
    payload[idx++] = (cow >> 24) & 0xFF;
    payload[idx++] = (cow >> 16) & 0xFF;
    payload[idx++] = (cow >> 8) & 0xFF;
    payload[idx++] = (cow >> 0) & 0xFF;

    memcpy(&payload[idx], &pkt.liters, 4);
    idx += 4;

    uint32_t ts = pkt.timestamp;
    payload[idx++] = (ts >> 24) & 0xFF;
    payload[idx++] = (ts >> 16) & 0xFF;
    payload[idx++] = (ts >> 8) & 0xFF;
    payload[idx++] = (ts >> 0) & 0xFF;

    memcpy(&payload[idx], &pkt.ec, 4);
    idx += 4;

    if (idx != 17) return false;

    uint8_t packet[1 + 1 + 17 + 1 + 1];
    size_t pidx = 0;
    packet[pidx++] = 0xAA;
    packet[pidx++] = 17;
    memcpy(&packet[pidx], payload, 17);
    pidx += 17;

    uint8_t crc = _calcCRC8(packet, 1 + 1 + 17);
    packet[pidx++] = crc;
    packet[pidx++] = 0x55;

    _enableTransmit();
    _serial->write(packet, pidx);
    _serial->flush();
    _enableReceive();

    return true;
}

// Чтение и парсинг одного полного пакета
bool RS485Manager::readPacket(RS485Packet& out_pkt) {
    if (!_serial) return false;
    uint32_t startTime = millis();

    while (millis() - startTime < _timeout) {
        if (_serial->available() && _serial->read() == 0xAA) break;
    }
    if (millis() - startTime >= _timeout) return false;

    uint8_t length = 0;
    if (!_serial->readBytes(&length, 1)) return false;
    if (length != 17) return false;

    uint8_t payload[17];
    if (_serial->readBytes(payload, 17) < 17) return false;

    uint8_t receivedCRC = 0;
    if (!_serial->readBytes(&receivedCRC, 1)) return false;

    uint8_t endByte = 0;
    if (!_serial->readBytes(&endByte, 1) == 0 || endByte != 0x55) return false;

    uint8_t tmpForCRC[1 + 1 + 17];
    tmpForCRC[0] = 0xAA;
    tmpForCRC[1] = 17;
    memcpy(&tmpForCRC[2], payload, 17);
    if (_calcCRC8(tmpForCRC, sizeof(tmpForCRC)) != receivedCRC) return false;

    size_t idx = 0;
    out_pkt.client_id = payload[idx++];

    out_pkt.cow_id =  ((uint32_t)payload[idx++] << 24);
    out_pkt.cow_id |= ((uint32_t)payload[idx++] << 16);
    out_pkt.cow_id |= ((uint32_t)payload[idx++] << 8);
    out_pkt.cow_id |= ((uint32_t)payload[idx++]);

    memcpy(&out_pkt.liters, &payload[idx], 4); idx += 4;

    out_pkt.timestamp =  ((uint32_t)payload[idx++] << 24);
    out_pkt.timestamp |= ((uint32_t)payload[idx++] << 16);
    out_pkt.timestamp |= ((uint32_t)payload[idx++] << 8);
    out_pkt.timestamp |= ((uint32_t)payload[idx++]);

    memcpy(&out_pkt.ec, &payload[idx], 4); idx += 4;

    return true;
}
