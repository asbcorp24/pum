#ifndef RS485_OTA_UPDATER_H
#define RS485_OTA_UPDATER_H

#include <LittleFS.h>
#include "RS485Manager.h"

class RS485OTAUpdater {
public:
    RS485OTAUpdater(RS485Manager& rs485);
    /**
     * @brief Начать OTA: открыть файл и отправить заголовок.
     * @param path Путь к прошивке в LittleFS, например "/firmware.bin"
     * @return true, если файл открыт и заголовок отправлен.
     */
    bool begin(const String& path);

    /**
     * @brief Отправить следующий чанк. Вызывать в цикле,
     * пока он возвращает true.
     */
    bool sendNextChunk();

private:
    RS485Manager& _rs485;
    File          _fwFile;
    uint32_t      _totalSize    = 0;
    uint16_t      _chunkSize    = 128; // размер одного чанка
    uint16_t      _totalChunks  = 0;
    uint16_t      _currentChunk = 0;
    uint8_t       _buffer[128];

    void sendHeader();
};

#endif // RS485_OTA_UPDATER_H
