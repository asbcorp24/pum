#ifndef MILK_SENSOR_H
#define MILK_SENSOR_H

#include <Arduino.h>

class MilkSensor {
public:
    /**
     * @brief Инициализация сенсора
     * @param pulsePin — пин тахометра
     * @param litersPerPulse — сколько литров на один импульс
     */
    void begin(uint8_t pulsePin, float litersPerPulse);

    /**
     * @brief Установить аналоговый пин для EC-сенсора
     * @param pin — аналоговый пин (например A0 или GPIO34)
     * @param factor — коэффициент перевода из raw в мСм/см
     */
    void setECPin(uint8_t pin, float factor = 1.0f);

    /**
     * @brief Обновить данные (рассчитать объём, поток, EC)
     */
    void update();

    /**
     * @brief Сбросить данные
     */
    void reset();

    /**
     * @return Общий объём молока в литрах
     */
    float getVolumeLiters() const;

    /**
     * @return Скорость потока в литрах/секунду
     */
    float getFlowRateLps() const;

    /**
     * @return Электропроводность в мСм/см
     */
    float getEC() const;

#ifndef MILK_SENSOR_EXTERNAL_UART
    static void IRAM_ATTR onPulse(); // для прерываний
#endif

private:
#ifndef MILK_SENSOR_EXTERNAL_UART
    void _updateFromPulseCounter();
    static volatile uint32_t _pulseCount;
    uint32_t _lastPulseCount = 0;
    uint8_t _pulsePin;
    float _litersPerPulse;
#else
    void _updateFromUART();
    HardwareSerial* _uart;
    String _uartBuffer;
#endif

    unsigned long _lastUpdate = 0;
    float _volumeLiters = 0.0f;
    float _flowRateLps = 0.0f;

    // EC sensor
    uint8_t _ecPin = 255;
    float _ecFactor = 1.0f;
    float _ecValue = 0.0f;
};

#endif // MILK_SENSOR_H
