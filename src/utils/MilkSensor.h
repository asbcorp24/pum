#ifndef MILK_SENSOR_H
#define MILK_SENSOR_H

#include <Arduino.h>

/// Установи этот флаг, если данные приходят с STM
//#define MILK_SENSOR_EXTERNAL_UART

#ifdef MILK_SENSOR_EXTERNAL_UART
#include <HardwareSerial.h>
#endif

class MilkSensor {
public:
    /**
     * @brief Инициализация сенсора
     * @param pulsePin — пин, подключенный к тахометру (если используется напрямую)
     * @param litersPerPulse — коэффициент: сколько литров на 1 импульс
     */
    void begin(uint8_t pulsePin = 4, float litersPerPulse = 0.0025f);

    /**
     * @brief Вызывается в loop() — обновляет скорость и объём
     */
    void update();

    /**
     * @brief Сброс накопленного значения (новая дойка)
     */
    void reset();

    /**
     * @brief Получить объём, литры
     */
    float getVolumeLiters() const;

    /**
     * @brief Получить скорость потока, литров в секунду
     */
    float getFlowRateLps() const;

private:
#ifdef MILK_SENSOR_EXTERNAL_UART
    HardwareSerial* _uart = nullptr;
    String _uartBuffer;
#else
    static volatile uint32_t _pulseCount;
    static void IRAM_ATTR onPulse();
    uint8_t _pulsePin = 4;
    float _litersPerPulse = 0.0025f;
#endif

    float _volumeLiters = 0.0;
    float _flowRateLps = 0.0;
    unsigned long _lastUpdate = 0;
    uint32_t _lastPulseCount = 0;

    void _updateFromPulseCounter();
    void _updateFromUART();
};

#endif // MILK_SENSOR_H
