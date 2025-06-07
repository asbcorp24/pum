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

    // 1. Собираем payload в буфер (13 байт)
    uint8_t payload[13];
    size_t idx = 0;

    payload[idx++] = pkt.client_id;

    // cow_id (4 байта, big-endian)
    uint32_t cow = pkt.cow_id;
    payload[idx++] = (cow >> 24) & 0xFF;
    payload[idx++] = (cow >> 16) & 0xFF;
    payload[idx++] = (cow >>  8) & 0xFF;
    payload[idx++] = (cow >>  0) & 0xFF;

    // liters (4 байта, IEEE754) — копируем байтово
    uint8_t* flt_ptr = (uint8_t*)&pkt.liters;
    // Предположим, что плата little-endian, тогда:
    payload[idx++] = flt_ptr[0];
    payload[idx++] = flt_ptr[1];
    payload[idx++] = flt_ptr[2];
    payload[idx++] = flt_ptr[3];

    // timestamp (4 байта, big-endian)
    uint32_t ts = pkt.timestamp;
    payload[idx++] = (ts >> 24) & 0xFF;
    payload[idx++] = (ts >> 16) & 0xFF;
    payload[idx++] = (ts >>  8) & 0xFF;
    payload[idx++] = (ts >>  0) & 0xFF;

    // idx должно быть ровно 13
    if (idx != 13) return false;

    // 2. Собираем весь пакет: [Start | Length | payload(13) | CRC | End]
    uint8_t packet[1 + 1 + 13 + 1 + 1];
    size_t pidx = 0;
    packet[pidx++] = 0xAA;        // Start
    packet[pidx++] = 13;          // Length

    // Копируем payload
    memcpy(&packet[pidx], payload, 13);
    pidx += 13;

    // CRC8: считаем по всем байтам от packet[0] до packet[1+13-1] (то есть 2 + 13 байт?
    // на самом деле, CRC считается от Start и Length и payload)
    uint8_t crc = _calcCRC8(packet, 1 + 1 + 13);
    packet[pidx++] = crc;

    packet[pidx++] = 0x55;        // End

    // 3. Переводим трансивер в режим TX
    _enableTransmit();

    // 4. Пишем в UART
    _serial->write(packet, pidx);
    _serial->flush(); // ждём, пока байты уйдут

    // 5. Возвращаем режим приёма
    _enableReceive();

    return true;
}

// Чтение и парсинг одного полного пакета
bool RS485Manager::readPacket(RS485Packet& out_pkt) {
    if (!_serial) return false;
    uint32_t startTime = millis();

    // 1) Сначала ждём байт 0xAA (Start)
    while (millis() - startTime < _timeout) {
        if (_serial->available()) {
            int b = _serial->read();
            if (b < 0) continue;
            if ((uint8_t)b == 0xAA) {
                // Нашли начало
                break;
            }
        }
    }
    if (millis() - startTime >= _timeout) return false;

    // 2) Считаем Byte Length (следующий байт)
    uint8_t length = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < _timeout) {
        if (_serial->available()) {
            length = (uint8_t)_serial->read();
            break;
        }
    }
    if (millis() - t0 >= _timeout) return false;

    if (length != 13) {
        // Неправильный Length (ожидаем 13). Отказываемся и возвращаем false.
        return false;
    }

    // 3) Считываем payload (13 байт)
    uint8_t payload[13];
    size_t readLen = 0;
    t0 = millis();
    while (millis() - t0 < _timeout && readLen < 13) {
        if (_serial->available()) {
            payload[readLen++] = (uint8_t)_serial->read();
        }
    }
    if (readLen < 13) return false;

    // 4) Считываем CRC8
    uint8_t receivedCRC = 0;
    t0 = millis();
    while (millis() - t0 < _timeout) {
        if (_serial->available()) {
            receivedCRC = (uint8_t)_serial->read();
            break;
        }
    }
    if (millis() - t0 >= _timeout) return false;

    // 5) Считываем End (0x55)
    uint8_t endByte = 0;
    t0 = millis();
    while (millis() - t0 < _timeout) {
        if (_serial->available()) {
            endByte = (uint8_t)_serial->read();
            break;
        }
    }
    if (millis() - t0 >= _timeout) return false;
    if (endByte != 0x55) {
        // Неверный End
        return false;
    }

    // 6) Проверяем CRC: нужно заново собрать буфер [Start|Length|payload] и пересчитать
    // Для этого используем временный массив
    uint8_t tmpForCRC[1 + 1 + 13];
    tmpForCRC[0] = 0xAA;
    tmpForCRC[1] = 13;
    memcpy(&tmpForCRC[2], payload, 13);
    uint8_t calc = _calcCRC8(tmpForCRC, sizeof(tmpForCRC));
    if (calc != receivedCRC) {
        // CRC не совпадает
        return false;
    }

    // 7) Распаковываем payload в out_pkt
    size_t idx = 0;
    out_pkt.client_id = payload[idx++];

    // cow_id (4 байта, big-endian)
    out_pkt.cow_id =  (uint32_t)payload[idx++] << 24;
    out_pkt.cow_id |= (uint32_t)payload[idx++] << 16;
    out_pkt.cow_id |= (uint32_t)payload[idx++] << 8;
    out_pkt.cow_id |= (uint32_t)payload[idx++];

    // liters (4 байта) — копируем напрямую в float
    uint8_t fltBuf[4];
    fltBuf[0] = payload[idx++];
    fltBuf[1] = payload[idx++];
    fltBuf[2] = payload[idx++];
    fltBuf[3] = payload[idx++];
    memcpy(&out_pkt.liters, fltBuf, 4);

    // timestamp (4 байта, big-endian)
    out_pkt.timestamp =  ((uint32_t)payload[idx++] << 24);
    out_pkt.timestamp |= ((uint32_t)payload[idx++] << 16);
    out_pkt.timestamp |= ((uint32_t)payload[idx++] << 8);
    out_pkt.timestamp |= ((uint32_t)payload[idx++]);

    // Всё удачно распаковали
    return true;
}
