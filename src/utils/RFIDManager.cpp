#include "RFIDManager.h"

#define RFID_BUFFER_MAX 32
#define RFID_TIMEOUT_MS 3000

RFIDManager::RFIDManager(RFIDMode mode) : _mode(mode) {}

void RFIDManager::begin(int rx, int tx, uint32_t baud) {
    _baud = baud;

    if (_mode == RFIDMode::UART) {
        _rx = rx;
        _tx = tx;

        if (rx != -1 && tx != -1) {
            _serial = new HardwareSerial(2); // Serial2
            _serial->begin(baud, SERIAL_8N1, rx, tx);
        } else {
            _serial = &Serial1;
            _serial->begin(baud);
        }
    } else if (_mode == RFIDMode::BLE) {
        // Заглушка — инициализация BLE клиента
        _bleInitialized = true;
    }
}

bool RFIDManager::available() {
    if (_mode == RFIDMode::UART) {
        _handleUART();
        return !_lastRFID.isEmpty();
    } else if (_mode == RFIDMode::BLE) {
        _handleBLE();
        return !_bleLastRFID.isEmpty();
    }

    return false;
}

String RFIDManager::readRFID() {
    String result;
    if (_mode == RFIDMode::UART) {
        result = _lastRFID;
        _lastRFID = ""; // очистка после чтения
    } else if (_mode == RFIDMode::BLE) {
        result = _bleLastRFID;
        _bleLastRFID = "";
    }
    return result;
}

void RFIDManager::configureBLE(const String& bleServiceName) {
    _bleServiceName = bleServiceName;
}

RFIDMode RFIDManager::getMode() const {
    return _mode;
}

void RFIDManager::_handleUART() {
    static String buffer = "";
    static unsigned long lastRead = 0;

    while (_serial && _serial->available()) {
        char c = _serial->read();
        if (isPrintable(c)) {
            buffer += c;
            lastRead = millis();

            if (buffer.length() >= RFID_BUFFER_MAX) {
                buffer = "";  // защита от мусора
            }
        }

        // если пришёл символ окончания (например, '\n' или фиксированная длина)
        if (c == '\n' || c == '\r' || buffer.length() == 10) {
            buffer.trim();
            if (!buffer.isEmpty()) {
                if (buffer != _lastRFID) {  // антидублирование
                    _lastRFID = buffer;
                }
            }
            buffer = "";
        }
    }

    // тайм-аут: если за 3 сек ничего не пришло — очистить буфер
    if (!buffer.isEmpty() && millis() - lastRead > RFID_TIMEOUT_MS) {
        buffer = "";
    }
}

void RFIDManager::_handleBLE() {
    // заглушка — интеграция через NimBLEClient или BLE UART
    // _bleLastRFID = "ABCDEF1234"; // пример
}
