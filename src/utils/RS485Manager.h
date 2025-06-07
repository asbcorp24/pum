#ifndef RS485_MANAGER_H
#define RS485_MANAGER_H

#include <Arduino.h>

/**
 * @brief Пакет данных (payload) для бинарного протокола RS485.
 */
struct RS485Packet {
    uint8_t client_id;
    uint32_t cow_id;
    float liters;
    uint32_t timestamp;
    float ec;

    RS485Packet() 
        : client_id(0), cow_id(0), liters(0.0f), timestamp(0), ec(0.0f) {}
};

/**
 * @brief Менеджер RS485 с бинарным протоколом.
 */
class RS485Manager {
public:
    RS485Manager() = default;
    ~RS485Manager() = default;

    /**
     * @brief Инициализация UART и DE/RE пина.
     * 
     * @param rxPin  Пин RX для UART (например, 16).
     * @param txPin  Пин TX для UART (например, 17).
     * @param baud   Скорость (9600, 115200 и т.д.).
     * @param dePin  Пин DE/RE для управления трансивером RS485.
     */
    void begin(uint8_t rxPin, uint8_t txPin, uint32_t baud, uint8_t dePin);
   /* **
    * @brief Отправить «сырые» данные по RS485, обёрнутые в Start/Len/CRC/End
    */
   bool sendRaw(const uint8_t* buf, size_t len);

   /**
    * @brief Прочитать «сырые» данные из RS485.
    * @param outBuf Буфер для payload (без Start/Len/CRC/End).
    * @param outLen Сюда запишется длина payload.
    * @return true, если пакет успешно считан и CRC проверен.
    */
   bool readRaw(uint8_t* outBuf, size_t& outLen);



    /**
     * @brief Проверяет, есть ли хотя бы один полный пакет в буфере.
     * 
     * @return true, если хотя бы один байт доступен (пакет может быть не полный).
     * @return false, если буфер пуст.
     */
    bool available();

    /**
     * @brief Читает один пакет из UART-потока.
     * 
     * Ждёт последовательности байтов:
     *   0xAA | Length | payload... | CRC8 | 0x55
     * Если пакет не валиден (CRC, End-байт), продолжает поиск следующего 0xAA.
     * 
     * @param out_pkt Сюда записывается разобранный RS485Packet (payload).
     * @return true, если успешно прочитан и разобран целый пакет.
     * @return false, если за отведённый timeout пакет не собрался.
     */
    bool readPacket(RS485Packet& out_pkt);

    /**
     * @brief Отправляет один пакет на UART (RS485).
     * 
     * Формирует заголовок (Start, Length), payload (13 байт), CRC8 и End.
     * Управляет пином DE: включает передачу, отправляет байты, ждёт окончания,
     * возвращает приёмный режим.
     * 
     * @param pkt Структура с данными (payload).
     * @return true, если отправлено успешно (данные записаны в UART).
     * @return false, если возникла ошибка.
     */
    bool sendPacket(const RS485Packet& pkt);

    /**
     * @brief Проверяет, доступен ли канал (UART настроен).
     * 
     * В простейшем варианте всегда возвращает true, если begin() уже вызывался.
     * 
     * @return true, если UART инициализирован.
     */
    bool isConnected() const;

    /**
     * @brief Устанавливает timeout для чтения одного полного пакета.
     * 
     * @param ms Таймаут в миллисекундах.
     */
    void setTimeout(uint16_t ms);

private:
    HardwareSerial* _serial = nullptr; ///< Указатель на UART (Serial2)
    uint8_t         _dePin   = 0;      ///< Пин DE/RE трансивера
    uint16_t        _timeout = 100;    ///< Таймаут чтения (ms)

    /**
     * @brief Вычисляет CRC8 для массива байтов.
     * 
     * Использует полином 0x07 (стандарт CRC-8).
     * 
     * @param data Указатель на байты.
     * @param len  Длина в байтах.
     * @return uint8_t — значение CRC8.
     */
    uint8_t _calcCRC8(const uint8_t* data, size_t len) const;

    /**
     * @brief Включает режим передачи: DE = HIGH.
     */
    void _enableTransmit();

    /**
     * @brief Включает режим приёма: DE = LOW.
     */
    void _enableReceive();
};

#endif // RS485_MANAGER_H
