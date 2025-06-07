#include "MilkSensor.h"

#ifndef MILK_SENSOR_EXTERNAL_UART
volatile uint32_t MilkSensor::_pulseCount = 0;
#endif

void MilkSensor::begin(uint8_t pulsePin, float litersPerPulse) {
#ifndef MILK_SENSOR_EXTERNAL_UART
    _pulsePin = pulsePin;
    _litersPerPulse = litersPerPulse;
    _pulseCount = 0;

    pinMode(_pulsePin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pulsePin), onPulse, RISING);
#else
    _uart = &Serial2; // или другой порт, если нужно
    _uart->begin(9600);  // скорость по STM
    _uartBuffer = "";
#endif
    _lastUpdate = millis();
    _volumeLiters = 0.0;
    _flowRateLps = 0.0;
}

void MilkSensor::update() {
#ifndef MILK_SENSOR_EXTERNAL_UART
    _updateFromPulseCounter();
#else
    _updateFromUART();
#endif
}

void MilkSensor::reset() {
#ifndef MILK_SENSOR_EXTERNAL_UART
    noInterrupts();
    _pulseCount = 0;
    interrupts();
#endif
    _volumeLiters = 0.0;
    _flowRateLps = 0.0;
    _lastPulseCount = 0;
}

float MilkSensor::getVolumeLiters() const {
    return _volumeLiters;
}

float MilkSensor::getFlowRateLps() const {
    return _flowRateLps;
}

#ifndef MILK_SENSOR_EXTERNAL_UART
void IRAM_ATTR MilkSensor::onPulse() {
    _pulseCount++;
}

void MilkSensor::_updateFromPulseCounter() {
    unsigned long now = millis();
    unsigned long elapsed = now - _lastUpdate;
    if (elapsed < 500) return; // обновляем не чаще, чем раз в 500 мс

    noInterrupts();
    uint32_t currentCount = _pulseCount;
    interrupts();

    uint32_t delta = currentCount - _lastPulseCount;
    _lastPulseCount = currentCount;

    float deltaLiters = delta * _litersPerPulse;
    _volumeLiters += deltaLiters;

    if (elapsed > 0) {
        _flowRateLps = (deltaLiters * 1000.0f) / elapsed;
    }

    _lastUpdate = now;
}
#else
void MilkSensor::_updateFromUART() {
    while (_uart->available()) {
        char c = _uart->read();
        if (c == '\n') {
            // Пример строки: V:1.23,F:0.45\n
            float vol = 0.0, flow = 0.0;
            int vIdx = _uartBuffer.indexOf("V:");
            int fIdx = _uartBuffer.indexOf("F:");

            if (vIdx != -1 && fIdx != -1) {
                vol = _uartBuffer.substring(vIdx + 2, fIdx - 1).toFloat();
                flow = _uartBuffer.substring(fIdx + 2).toFloat();

                _volumeLiters = vol;
                _flowRateLps = flow;
            }

            _uartBuffer = "";
        } else {
            _uartBuffer += c;
            if (_uartBuffer.length() > 50) _uartBuffer = ""; // защита от мусора
        }
    }
}
#endif
