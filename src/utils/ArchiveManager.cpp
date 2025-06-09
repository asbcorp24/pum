#include "ArchiveManager.h"

void ArchiveManager::begin() {
    EEPROM.begin(EEPROM_SIZE);
    write_index = 0; // можно загрузить из EEPROM, если нужно сохранять между перезагрузками
}

void ArchiveManager::add(const ArchiveRecord& record) {
    writeRecord(write_index, record);
    write_index = (write_index + 1) % MAX_RECORDS;
    EEPROM.commit();
}

void ArchiveManager::writeRecord(uint16_t index, const ArchiveRecord& record) {
    int addr = index * RECORD_SIZE;
    EEPROM.put(addr, record);
}

bool ArchiveManager::readRecord(uint16_t index, ArchiveRecord& record) {
    if (index >= MAX_RECORDS) return false;
    int addr = index * RECORD_SIZE;
    EEPROM.get(addr, record);
    return true;
}

bool ArchiveManager::getNextPending(uint16_t &outIndex, ArchiveRecord &outRec) {
    // Перебираем все возможные индексы
    for (uint16_t i = 0; i < MAX_RECORDS; i++) {
        ArchiveRecord r;
        if (readRecord(i, r) && r.status == 0) {  // 0 = pending
            outIndex = i;
            outRec   = r;
            return true;
        }
    }
    return false;
}
String ArchiveManager::getArchiveJson() {
    String json = "[";
    ArchiveRecord rec;
    bool first = true;
    // Перебираем все возможные слоты архива
    for (uint16_t i = 0; i < MAX_RECORDS; i++) {
        // readRecord у вас приватный, сделаем его публичным или друг — см. ниже
        if (readRecord(i, rec)) {
            if (!first) json += ",";
            first = false;
            json += "{";
            json += "\"client_id\":" + String(rec.client_id) + ",";
            json += "\"cow_id\":"    + String(rec.cow_id)    + ",";
            json += "\"timestamp\":" + String(rec.timestamp) + ",";
            json += "\"volume\":"    + String(rec.volume, 2) + ",";
            json += "\"ec\":"        + String(rec.ec, 2)     + ",";
            json += "\"status\":"    + String(rec.status);
            json += "}";
        }
    }
    json += "]";
    return json;
}
void ArchiveManager::updateStatus(uint16_t index, uint8_t status) {
    ArchiveRecord record;
    if (!readRecord(index, record)) return;
    record.status = status;
    writeRecord(index, record);
    EEPROM.commit();
}

void ArchiveManager::dumpAll(Stream& out) {
    for (uint16_t i = 0; i < MAX_RECORDS; i++) {
        ArchiveRecord r;
        if (readRecord(i, r)) {
            out.printf("ID: %lu, Time: %lu, Volume: %.2f, EC: %.2f, Status: %u\n",r.cow_id, r.timestamp, r.volume, r.ec, r.status);
        }
    }
}
