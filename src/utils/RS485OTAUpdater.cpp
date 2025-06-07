#include "RS485OTAUpdater.h"
#include <LittleFS.h>
RS485OTAUpdater::RS485OTAUpdater(RS485Manager& rs485)
    : _rs485(rs485) {}

bool RS485OTAUpdater::begin(const String& path) {
    _fwFile = LittleFS.open(path, "r");
    if (!_fwFile) return false;
    _totalSize   = _fwFile.size();
    _totalChunks = (_totalSize + _chunkSize - 1) / _chunkSize;
    _currentChunk = 0;
    sendHeader();
    return true;
}

void RS485OTAUpdater::sendHeader() {
    // Пакет типа 0x10 — заголовок OTA
    struct {
        uint8_t  type;         // 0x10
        uint32_t totalSize;
        uint16_t chunkSize;
        uint16_t totalChunks;
    } hdr;
    hdr.type        = 0x10;
    hdr.totalSize   = _totalSize;
    hdr.chunkSize   = _chunkSize;
    hdr.totalChunks = _totalChunks;

    _rs485.sendRaw((uint8_t*)&hdr, sizeof(hdr));
}

bool RS485OTAUpdater::sendNextChunk() {
    if (_currentChunk >= _totalChunks) {
        _fwFile.close();
        return false;
    }
    size_t len = _fwFile.read(_buffer, _chunkSize);
    // Пакет типа 0x11 — чанк OTA
    struct {
        uint8_t  type;        // 0x11
        uint16_t chunkIndex;
        uint16_t length;
    } pktHdr;
    pktHdr.type       = 0x11;
    pktHdr.chunkIndex = _currentChunk;
    pktHdr.length     = len;

    // отправляем hdr + данные
    _rs485.sendRaw((uint8_t*)&pktHdr, sizeof(pktHdr));
    _rs485.sendRaw(_buffer, len);

    _currentChunk++;
    return true;
}
