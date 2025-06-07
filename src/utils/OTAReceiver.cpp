#include "OTAReceiver.h"
#include <LittleFS.h>
OTAReceiver::OTAReceiver(RS485Manager& rs485)
  : _rs485(rs485) {}

void OTAReceiver::handle() {
    uint8_t buf[150];
    size_t  len;
    if (!_rs485.readRaw(buf, len)) return;
    processPayload(buf, len);
}

void OTAReceiver::processPayload(const uint8_t* buf, size_t len) {
    if (len < 1) return;
    uint8_t type = buf[0];

    if (type == 0x10 && !_updating && len == 1 + 4 + 2 + 2) {
        // Header
        struct {
            uint8_t  t;
            uint32_t totalSize;
            uint16_t chunkSize;
            uint16_t totalChunks;
        } hdr;
        memcpy(&hdr, buf, sizeof(hdr));
        _fileSize    = hdr.totalSize;
        _chunkSize   = hdr.chunkSize;
        _totalChunks = hdr.totalChunks;
        _recvChunks  = 0;
        // Открываем файл на запись
        _binFile = LittleFS.open("/fw.bin", FILE_WRITE);
        if (!_binFile) return;
        _updating = true;
    }
    else if (type == 0x11 && _updating && len >= 1 + 2 + 2) {
        // Chunk
        uint16_t idx   = 0;
        uint16_t clen  = 0;
        memcpy(&idx,  buf + 1, 2);
        memcpy(&clen, buf + 3, 2);
        const uint8_t* data = buf + 5;
        // Позиционируемся в файле
        _binFile.seek((size_t)idx * _chunkSize);
        _binFile.write(data, clen);
        _recvChunks++;

        if (_recvChunks == _totalChunks) {
            _binFile.close();
            // Запускаем OTA из файла
            File f = LittleFS.open("/fw.bin", "r");
            if (!f) return;
            if (Update.begin(f.size())) {
                size_t w = Update.writeStream(f);
                if (w == f.size() && Update.end(true)) {
                    ESP.restart();
                }
            }
            _updating = false;
        }
    }
}
