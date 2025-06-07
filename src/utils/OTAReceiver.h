#ifndef OTA_RECEIVER_H
#define OTA_RECEIVER_H

#include <LittleFS.h>
#include <Update.h>
#include "RS485Manager.h"

class OTAReceiver {
public:
    OTAReceiver(RS485Manager& rs485);
    /**
     * @brief Вызывать в клиентском RS485-задаче:
     * читает «сырые» пакеты, обрабатывает OTA.
     */
    void handle();

private:
    RS485Manager& _rs485;
    bool    _updating     = false;
    uint32_t _fileSize    = 0;
    uint16_t _chunkSize   = 0;
    uint16_t _totalChunks = 0;
    uint16_t _recvChunks  = 0;
    File    _binFile;

    void processPayload(const uint8_t* buf, size_t len);
};

#endif // OTA_RECEIVER_H
