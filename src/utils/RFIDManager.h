#ifndef RFID_MANAGER_H
#define RFID_MANAGER_H

#include <Arduino.h>

enum class RFIDMode {
    NONE,
    UART,
    BLE
};

class RFIDManager {
public:
    /**
     * @brief Конструктор RFIDManager
     * @param mode — режим (UART / BLE)
     */
    RFIDManager(RFIDMode mode = RFIDMode::UART);

    /**
     * @brief Инициализация RFID в выбранном режиме
     * @param rx — RX-пин для UART
     * @param tx — TX-пин для UART
     * @param baud — скорость UART, по умолчанию 9600
     */
    void begin(int rx = -1, int tx = -1, uint32_t baud = 9600);

    /**
     * @brief Проверка: есть ли новая RFID-метка
     * @return true, если доступна новая метка
     */
    bool available();

    /**
     * @brief Получить считанный RFID-код
     * @return строка RFID (например, "1234567890ABCDEF")
     */
    String readRFID();

    /**
     * @brief Установить BLE UUID или имя сервиса (если используется BLE)
     */
    void configureBLE(const String& bleServiceName);

    /**
     * @brief Получить текущий режим
     */
    RFIDMode getMode() const;

private:
    RFIDMode _mode = RFIDMode::NONE;

    // UART
    HardwareSerial* _serial = nullptr;
    int _rx = -1, _tx = -1;
    uint32_t _baud = 9600;
    String _lastRFID = "";

    // BLE
    String _bleServiceName = "";
    String _bleLastRFID = "";
    bool _bleInitialized = false;

    /**
     * @brief Внутренний парсинг UART
     */
    void _handleUART();

    /**
     * @brief Внутренний парсинг BLE (эмуляция)
     */
    void _handleBLE();
};

#endif // RFID_MANAGER_H
