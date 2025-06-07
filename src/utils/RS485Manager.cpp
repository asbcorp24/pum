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

    const size_t PAYLOAD_LEN = 20;
    uint8_t payload[PAYLOAD_LEN];
    size_t idx = 0;

    // 1) client_id (4 байта, big-endian)
    payload[idx++] = (pkt.client_id >> 24) & 0xFF;
    payload[idx++] = (pkt.client_id >> 16) & 0xFF;
    payload[idx++] = (pkt.client_id >>  8) & 0xFF;
    payload[idx++] = (pkt.client_id >>  0) & 0xFF;

    // 2) cow_id (4 байта, big-endian)
    payload[idx++] = (pkt.cow_id >> 24) & 0xFF;
    payload[idx++] = (pkt.cow_id >> 16) & 0xFF;
    payload[idx++] = (pkt.cow_id >>  8) & 0xFF;
    payload[idx++] = (pkt.cow_id >>  0) & 0xFF;

    // 3) liters (4 байта, IEEE754 little-endian)
    uint8_t* pLit = (uint8_t*)&pkt.liters;
    payload[idx++] = pLit[0];
    payload[idx++] = pLit[1];
    payload[idx++] = pLit[2];
    payload[idx++] = pLit[3];

    // 4) timestamp (4 байта, big-endian)
    payload[idx++] = (pkt.timestamp >> 24) & 0xFF;
    payload[idx++] = (pkt.timestamp >> 16) & 0xFF;
    payload[idx++] = (pkt.timestamp >>  8) & 0xFF;
    payload[idx++] = (pkt.timestamp >>  0) & 0xFF;

    // 5) ec (4 байта, IEEE754 little-endian)
    uint8_t* pEc = (uint8_t*)&pkt.ec;
    payload[idx++] = pEc[0];
    payload[idx++] = pEc[1];
    payload[idx++] = pEc[2];
    payload[idx++] = pEc[3];

    // Проверяем, что мы упаковали ровно 20 байт
    if (idx != PAYLOAD_LEN) return false;

    // Собираем весь пакет: [Start|Len|payload|CRC|End]
    uint8_t packet[1 + 1 + PAYLOAD_LEN + 1 + 1];
    size_t p = 0;
    packet[p++] = 0xAA;
    packet[p++] = PAYLOAD_LEN;
    memcpy(&packet[p], payload, PAYLOAD_LEN);
    p += PAYLOAD_LEN;
    uint8_t crc = _calcCRC8(packet, 2 + PAYLOAD_LEN);
    packet[p++] = crc;
    packet[p++] = 0x55;

    // TX → send → RX
    _enableTransmit();
    _serial->write(packet, p);
    _serial->flush();
    _enableReceive();

    return true;
}

bool RS485Manager::sendRaw(const uint8_t* buf, size_t len) {
    if (!_serial) return false;
    size_t total = 1 + 1 + len + 1 + 1;
    uint8_t* pkt = (uint8_t*)malloc(total);
    if (!pkt) return false;

    size_t i = 0;
    pkt[i++] = 0xAA;          // Start
    pkt[i++] = (uint8_t)len;  // Length
    memcpy(pkt + i, buf, len);
    i += len;
    uint8_t crc = _calcCRC8(pkt, 2 + len);
    pkt[i++] = crc;           // CRC
    pkt[i++] = 0x55;          // End

    _enableTransmit();
    _serial->write(pkt, total);
    _serial->flush();
    _enableReceive();

    free(pkt);
    return true;
}

bool RS485Manager::readRaw(uint8_t* outBuf, size_t& outLen) {
    if (!_serial) return false;
    uint32_t start = millis();
    // Ждём Start
    while (millis() - start < _timeout) {
        if (_serial->available() && _serial->read() == 0xAA) break;
    }
    if (millis() - start >= _timeout) return false;

    // Читаем Length
    uint8_t len;
    if (!_serial->readBytes(&len, 1)) return false;
    outLen = len;
    if (len > 250) return false; // защита

    // Читаем payload
    if (_serial->readBytes(outBuf, len) < len) return false;

    // Читаем CRC
    uint8_t recvCrc = 0;
    if (!_serial->readBytes(&recvCrc, 1)) return false;

    // Читаем End
    uint8_t endByte = 0;
    if (!_serial->readBytes(&endByte, 1) || endByte != 0x55) return false;

    // Проверяем CRC
    uint8_t* tmp = (uint8_t*)malloc(2 + len);
    tmp[0] = 0xAA;
    tmp[1] = len;
    memcpy(tmp + 2, outBuf, len);
    uint8_t calc = _calcCRC8(tmp, 2 + len);
    free(tmp);
    return calc == recvCrc;
}
// Чтение и парсинг одного полного пакета
bool RS485Manager::readPacket(RS485Packet& out_pkt) {
    if (!_serial) return false;
    uint32_t start = millis();

    // 1) Ждём Start
    while (millis() - start < _timeout) {
        if (_serial->available() && _serial->read() == 0xAA) break;
    }
    if (millis() - start >= _timeout) return false;

    // 2) Читаем Length
    uint8_t length;
    if (!_serial->readBytes(&length, 1)) return false;
    if (length != 20) return false;  // теперь 20 байт payload!

    // 3) Читаем payload
    uint8_t payload[20];
    if (_serial->readBytes(payload, 20) < 20) return false;

    // 4) Читаем CRC и End
    uint8_t recvCrc = 0, endByte = 0;
    if (!_serial->readBytes(&recvCrc, 1)) return false;
    if (_serial->readBytes(&endByte, 1) != 1 || endByte != 0x55) return false;

    // 5) Проверяем CRC
    uint8_t tmp[2 + 20];
    tmp[0] = 0xAA;
    tmp[1] = 20;
    memcpy(tmp + 2, payload, 20);
    if (_calcCRC8(tmp, sizeof(tmp)) != recvCrc) return false;

    // 6) Распаковываем
    size_t i = 0;
    out_pkt.client_id = ((uint32_t)payload[i++] << 24)
                      | ((uint32_t)payload[i++] << 16)
                      | ((uint32_t)payload[i++] <<  8)
                      | ((uint32_t)payload[i++] <<  0);

    out_pkt.cow_id = ((uint32_t)payload[i++] << 24)
                   | ((uint32_t)payload[i++] << 16)
                   | ((uint32_t)payload[i++] <<  8)
                   | ((uint32_t)payload[i++] <<  0);

    memcpy(&out_pkt.liters, payload + i, 4);
    i += 4;

    out_pkt.timestamp = ((uint32_t)payload[i++] << 24)
                      | ((uint32_t)payload[i++] << 16)
                      | ((uint32_t)payload[i++] <<  8)
                      | ((uint32_t)payload[i++] <<  0);

    memcpy(&out_pkt.ec, payload + i, 4);
    i += 4;

    return true;
}

