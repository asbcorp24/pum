# Milk Monitoring System on ESP32-S3

Система учёта выдачи молока на основе ESP32-S3 с режимами **Server** и **Client**, реализованная на PlatformIO (Arduino Framework). Подходит для небольших фермерских хозяйств или Zill-scale установок: сервер собирает данные от клиентов по RS-485, архивирует их и пересылает в облако по MQTT/REST, клиент сканирует RFID - метки коров и меряет объём молока через тахометр, а также поддерживает OTA-обновления по RS-485.

---

## 🔧 Основные возможности

- **Два режима**:  
  - **Server Mode** (ESP32 в центральном пункте):  
    – Приём данных от клиентов по RS-485  
    – Локальный архив в LittleFS  
    – Пересылка данных по MQTT (или REST)  
    – Веб-интерфейс конфигурации (Mongoose)  
    – HTTP-OTA обновления прошивки  
    – Отправка OTA-прошивок клиентам по RS-485  
    – Дисплей LVGL (TFT) с текущим статусом

  - **Client Mode** (ESP32 на доильном пункте):  
    – Сканирование RFID-меток коров (UART или BLE)  
    – Измерение объёма молока (тахометр + счётчик пульсаций)  
    – Измерение электропроводимости (EC)  
    – Архив локальных замеров  
    – Передача данных серверу по RS-485  
    – Приём OTA-обновлений по RS-485  
    – Дисплей LVGL с прогрессом и статистикой

- **Модули**:
  - `ConfigManager` — хранит настройки (Wi-Fi, MQTT, REST, RS-485 ID) в Preferences
  - `RS485Manager` — надёжный обмен бинарными пакетами (CRC-8, Start/Len/CRC/End)
  - `MQTTManager` — PubSubClient-обёртка для подключения и публикации
  - `RESTManager` — HTTPClient + ArduinoJson для загрузки конфигурации и HTTP-OTA
  - `RFIDManager` — чтение меток через UART/BLE
  - `MilkSensor` — подсчёт литров и потока (или приём по UART)
  - `ArchiveManager` — запись структур в LittleFS, поддержка статусов pending/sent
  - `DisplayManager` — LVGL-интерфейс для разных экранов
  - `RS485OTAUpdater` — отправка бинарника прошивки через RS-485 чанками
  - `OTAReceiver` — приём чанков, запись в FS и вызов Update API

---

## 📋 Аппаратные компоненты

- **ESP32-S3 DevKitC-1** (N8R2)  
- **RS-485 трансивер** (DE/RE управление)  
- **TFT-дисплей** (ILI9341 + LVGL)  
- **RFID-считыватель** (UART или BLE)  
- **Тахометр** + **IR-датчик** для подсчёта пульсаций  
- **Датчик электропроводимости** (аналоговый вход)  

---

## ⚙️ ПО и зависимости

- **PlatformIO** (VSCode / CLI)  
- **Framework**: Arduino for ESP32  
- Библиотеки:
  - `PubSubClient` (MQTT)  
  - `ArduinoJson` (JSON)  
  - `mongoose` (встроенный HTTP-сервер)  
  - `Preferences` (Flash-настройки)  
  - `LittleFS` (SPIFFS-хранилище)  
  - `LVGL` + `TFT_eSPI` (GUI)  
  - `Update` (OTA)  

---

## 📂 Структура проекта
├── include/

│ └── (заголовки, если нужны)

├── src/

│ ├── main.cpp

│ └── utils/

│ ├── ConfigManager.h/.cpp

│ ├── RS485Manager.h/.cpp

│ ├── MQTTManager.h/.cpp

│ ├── RESTManager.h/.cpp

│ ├── RFIDManager.h/.cpp

│ ├── MilkSensor.h/.cpp

│ ├── ArchiveManager.h/.cpp

│ ├── DisplayManager.h/.cpp

│ ├── RS485OTAUpdater.h/.cpp

│ └── OTAReceiver.h/.cpp

├── platformio.ini

└── README.md



---

## 🚀 Алгоритм работы

### `main.cpp`

 
  // В setup():
  pinMode(GPIO_TOGGLE_PIN, INPUT_PULLDOWN);
  if (digitalRead(GPIO_TOGGLE_PIN) == HIGH) {
    startServerMode();
  } else {
    startClientMode();
  }
  Server Mode (startServerMode())
  ConfigManager.begin("milk_cfg")

DisplayManager.showStartupScreen("Server Mode")

Wi-Fi STA/AP:

Если есть SSID/Pass → WiFi.begin() → при успехе ONLINE, иначе AP Mode

В AP Mode WiFi.softAP(AP_SSID, AP_PASS)

Mongoose:
 
mg_mgr_init(&mgr, NULL);
mg_http_listen(&mgr, "http://0.0.0.0:80", mongooseEventHandler, NULL);
MQTTManager.begin(&wifiClient, broker, port, clientId, user, pass)

RS485Manager.begin(rx, tx, baud, dePin)

RESTManager.begin(restUrl)

ArchiveManager.begin()

RS485OTAUpdater.begin("/firmware.bin")

FreeRTOS-таски (на разных ядрах):

serverMongooseTask: mg_mgr_poll()

serverRS485Task: читать RS485Packet, сохранять в ArchiveManager, DisplayManager.showMessage()

serverMQTTTask: каждые 30 с искать одну pending-запись, формировать JSON с полями

 
{
  "pum_id": …,
  "cow_id": …,
  "timestamp": …,
  "volume": …,
  "ec": …
}
и MQTTManager.publish(), затем ArchiveManager.updateStatus(idx,1)

serverDisplayTask: DisplayManager.update()

Client Mode (startClientMode())
ConfigManager.begin("milk_cfg")

DisplayManager.showStartupScreen("Client Mode")

RFIDManager.begin()

MilkSensor.begin(pulsePin, litersPerPulse)

RS485Manager.begin(rx, tx, baud, dePin) + otaReceiver = new OTAReceiver(rs485)

ArchiveManager.begin()

DisplayManager.showClientStatus(0,"—",0.0f)

FreeRTOS-таски:

clientRFIDTask: при rfid.available() → lastCowID = rfid.readRFID() → DisplayManager.showMessage() → clientState = SCANNING

clientMilkTask:
 
if (clientState==SCANNING) {
  milkSensor.update();
  float vol = milkSensor.getVolumeLiters();
  lastVolume = vol;
  archiveMgr.add({cfgManager.getClientID(),
                  lastCowID.toInt(),
                  millis()/1000, vol, /*ec=*/0, 0});
  DisplayManager.showMessage("Milk: "+String(vol,2)+"L");
  clientState = MEASURING;
}
clientRS485Task:

 
otaReceiver->handle();  // приём OTA-чанков
if (state==MEASURING||state==SENDING) {
  ArchiveRecord r; uint16_t idx;
  if (archiveMgr.getNextPending(idx,r)) {
    RS485Packet p{r.client_id, r.cow_id, r.volume, r.timestamp, r.ec};
    if (rs485.sendPacket(p)) archiveMgr.updateStatus(idx,1);
  }
  clientState = IDLE;
}
clientDisplayTask:
 
uint16_t pend=0; while(archiveMgr.getNextPending(r)) pend++;
displayMgr.showClientStatus(pend, lastCowID, lastVolume);
displayMgr.update();
🔄 OTA-обновления
HTTP-OTA (Server Mode) через RESTManager.checkForFirmwareUpdate("/api/device/update", currentVer)

RS-485 OTA:

Server Mode: RS485OTAUpdater читает "/firmware.bin", шлёт чанки через sendRaw()

Client Mode: OTAReceiver.handle() принимает чанки, сохраняет "/fw.bin", вызывает Update.writeStream() → Update.end() → ESP.restart()
