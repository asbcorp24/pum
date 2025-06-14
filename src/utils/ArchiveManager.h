#ifndef ARCHIVE_MANAGER_H
#define ARCHIVE_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>

struct ArchiveRecord {
    uint32_t   client_id;   // номер ПУМ
    uint32_t cow_id;
    uint32_t timestamp;
    float    volume;
    float    ec;       // электр.проводимость, мСм/см
    uint8_t  status; // 0 = pending, 1 = sent, 2 = error
};

class ArchiveManager {
public:
    static const uint16_t EEPROM_SIZE = 4096;
    static const uint8_t  RECORD_SIZE = sizeof(ArchiveRecord); // 13
    static const uint16_t MAX_RECORDS = EEPROM_SIZE / RECORD_SIZE;
    /**
     * @brief Получить весь архив в виде JSON-массива.
     * @return Строка вида [{"client_id":..., "cow_id":..., "timestamp":..., "volume":..., "ec":..., "status":...}, ...]
     */
    String getArchiveJson();
    void begin();

    /**
     * @brief Добавить новую запись в архив (циклически).
     */
    void add(const ArchiveRecord& record);

     /**
     * @brief Найти первую запись со статусом pending и вернуть её индекс.
     * @param outIndex индекс найденной записи
     * @param outRec   сама запись
     * @return true, если такая запись есть
     */
    bool getNextPending(uint16_t &outIndex, ArchiveRecord &outRec);
    
    /**
     * @brief Обновить статус записи по индексу.
     * @param index индекс записи (0..MAX_RECORDS-1)
     * @param status новый статус (например, 1 = sent)
     */
    void updateStatus(uint16_t index, uint8_t status);

    /**
     * @brief Экспорт всех записей (например, для JSON или MQTT).
     */
    void dumpAll(Stream& out);

private:
    uint16_t write_index = 0;

    void writeRecord(uint16_t index, const ArchiveRecord& record);
    bool readRecord(uint16_t index, ArchiveRecord& record);
};

#endif
