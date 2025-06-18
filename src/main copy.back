#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <PubSubClient.h>
 #include "mongoose/mongoose_glue.h"
//#include "mongoose/mongoose_impl.c"
//#include "mongoose/mongoose.h"
 
#include "utils/ConfigManager.h"
#include "utils/RS485Manager.h"
#include "utils/MQTTManager.h"
#include "utils/RESTManager.h"
#include "utils/RFIDManager.h"
#include "utils/MilkSensor.h"
#include "utils/ArchiveManager.h"
#include "utils/DisplayManager.h"
#include "utils/RS485OTAUpdater.h"
#include "utils/OTAReceiver.h"
#include <LittleFS.h>
// -----------------------------------------------------------------------------
// === ПИНЫ ===
#define GPIO_TOGGLE_PIN     2    // Переключатель Server/Client
#define LED_STATUS_PIN      13   // Индикатор статуса
#define RS485_RX_PIN        16   // пример пин для RS485 RX
#define RS485_TX_PIN        17   // пример пин для RS485 TX
#define RS485_DE_PIN        18   // пин DE на трансивере RS485

#define AP_SSID             "ESP32_MILK_SERVER"
#define AP_PASSWORD         "12345678"


static String lastCowID = "";  // будет хранить последний отсканированный ID

static float  lastVolume = 0.0f;

static RS485OTAUpdater* otaUpdater = nullptr;
static OTAReceiver* otaReceiver = nullptr;
// -----------------------------------------------------------------------------
// === Глобальные объекты ===
Preferences        preferences;     // Preferences вместо EEPROM
ConfigManager      cfgManager;      // Класс для работы с настройками
RS485Manager       rs485;           // Класс для RS485
MQTTManager        mqttClient;      // Класс для MQTT
RESTManager        restClient;      // Класс для REST-загрузки конфигурации
RFIDManager        rfid;            // Класс для RFID-считывания
MilkSensor         milkSensor;      // Класс для датчика молока
ArchiveManager     archiveMgr;      // Класс для архива (SPIFFS)
DisplayManager     displayMgr;      // Класс для LVGL-экрана

WiFiClient         wifiClient;
PubSubClient       clientMQTT(wifiClient);

// Mongoose (для веб-интерфейса)
static struct mg_mgr    mongoose_mgr;
static struct mg_connection *s_http_server = nullptr;

// Состояние системы
enum SystemMode { MODE_UNDEFINED, MODE_SERVER, MODE_CLIENT };
SystemMode systemMode = MODE_UNDEFINED;

enum ServerState { SERVER_OFFLINE, SERVER_AP_MODE, SERVER_ONLINE };
ServerState serverState = SERVER_OFFLINE;

enum ClientState { CLIENT_IDLE, CLIENT_SCANNING, CLIENT_MEASURING, CLIENT_SENDING };
static volatile ClientState clientState = CLIENT_IDLE;

// Таймеры/флаги
static volatile bool wifiConnected = false;
static volatile unsigned long lastMQTTSend = 0;
static const unsigned long MQTT_SEND_INTERVAL = 30 * 1000UL; // каждые 30 секунд

// -----------------------------------------------------------------------------
// === Декларации функций (задач) ===

// === Главные «routing»-функции ===
void startServerMode();
void startClientMode();

// === Задачи для Server Mode ===
void serverMongooseTask(void *pvParameters);
void serverRS485Task(void *pvParameters);
void serverMQTTTask(void *pvParameters);
void serverDisplayTask(void *pvParameters);

// === Задачи для Client Mode ===
void clientRFIDTask(void *pvParameters);
void clientMilkTask(void *pvParameters);
void clientRS485Task(void *pvParameters);
void clientDisplayTask(void *pvParameters);

// Обработчик HTTP-запросов Mongoose
void mongooseEventHandler(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

// -----------------------------------------------------------------------------
// === Функция setup() ===
void setup() {
  // 1) Настраиваем пины
  pinMode(GPIO_TOGGLE_PIN, INPUT_PULLDOWN);
  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, LOW);

  Serial.begin(115200);
  delay(500);

  // 2) Монтируем файловую систему (SPIFFS / LittleFS)
bool fs_ok = LittleFS.begin(/*formatIfFailed=*/ true);
if (!fs_ok) {
  Serial.println("FATAL: LittleFS не смонтировался даже после форматирования");
  while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }  // останавливаемся
}
Serial.println("LittleFS смонтирован успешно");

  // 3) Считываем переключатель, чтобы определить режим
  bool toggle =1;// digitalRead(GPIO_TOGGLE_PIN);

  if (toggle) {
    systemMode = MODE_SERVER;
    Serial.println("=== Переходим в Server Mode ===");
    startServerMode();
  } else {
    systemMode = MODE_CLIENT;
    Serial.println("=== Переходим в Client Mode ===");
    startClientMode();
  }

  // После создания всех задач мы можем завершить setup() и «убить» loop():
}

// -----------------------------------------------------------------------------
// === «Пустой» loop: вся логика вынесена в FreeRTOS-задачи ===
void loop() {
  vTaskDelay(portMAX_DELAY);
}

// -----------------------------------------------------------------------------
// === РЕАЛИЗАЦИЯ Server Mode ===

void startServerMode() {
  // ─────────────────────────────────────────────────────────────────────────────
  // 1. Инициализация настроек из Preferences
  cfgManager.begin("milk_cfg");
  if (!cfgManager.hasSavedConfig()) {
    Serial.println("[Server] Настройки Wi-Fi/MQTT/REST/RS485 не найдены в Preferences.");
  }
 Serial.println("Инициализация дисплея (LVGL)");
  // 2. Инициализация дисплея (LVGL)
  displayMgr.begin();         
     Serial.println("Инициализация дисплея прошла успешно");             // Конфигурируем LVGL, дисплей
  //displayMgr.showSplash("Server Mode");    // Заставка
  displayMgr.showStartupScreen("Server Mode");

  // 3. Подключение к Wi-Fi (если есть сохранённые креды)
  bool gotSSID = cfgManager.getWiFiCredentials();
  if (gotSSID) {
    Serial.println("[Server] Попытка подключения к Wi-Fi: " + cfgManager.savedSSID);
   // displayMgr.showStatus("Wi-Fi: Connecting...");
   displayMgr.showWiFiStatus(cfgManager.savedSSID,WiFi.localIP().toString(),wifiConnected);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfgManager.savedSSID.c_str(), cfgManager.savedPassword.c_str());
    unsigned long start = millis();
    while (millis() - start < 15000) {
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("[Server] Подключились к Wi-Fi: " + WiFi.localIP().toString());
       // displayMgr.showStatus("Wi-Fi: Connected");
       displayMgr.showWiFiStatus(cfgManager.savedSSID,WiFi.localIP().toString(), true);
        serverState = SERVER_ONLINE;
        break;
      }
      delay(200);
    }
    if (!wifiConnected) {
      serverState = SERVER_AP_MODE;
      Serial.println("[Server] Не удалось подключиться к Wi-Fi.");
    }
  } else {
    serverState = SERVER_AP_MODE;
    Serial.println("[Server] Нет сохранённых SSID/Pass → стартуем AP Mode.");
  }

  // 4. Если AP Mode → запускаем точку доступа
  if (serverState == SERVER_AP_MODE) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[Server][AP] Точка доступа: SSID=%s, IP=%s\n", AP_SSID, ip.toString().c_str());
   // displayMgr.showAPInfo(AP_SSID, AP_PASSWORD, ip.toString());
   displayMgr.showWiFiStatus(AP_SSID,ip.toString(),false);
  }

  // 5. Инициализация Mongoose (для обоих состояний: AP и ONLINE)
 // mg_mgr_init(&mongoose_mgr);
 /*mg_mgr_init(&mongoose_mgr );
  s_http_server = mg_http_listen(&mongoose_mgr, "http://0.0.0.0:80", mongooseEventHandler, NULL);
  //s_http_server = mg_http_listen_http(&mongoose_mgr,"0.0.0.0:80",mongooseEventHandler,NULL);*/
   mongoose_init();
 mongoose_set_http_handlers("wifi",  glue_get_wifi,  glue_set_wifi);
 mongoose_set_http_handlers("mqtt",  glue_get_mqtt,  glue_set_mqtt);
 mongoose_set_http_handlers("rs485", glue_get_rs485, glue_set_rs485);
 mongoose_set_http_handlers("uchet", glue_get_uchet,  glue_set_uchet);
 mongoose_set_http_handlers("rest",  glue_get_rest,   glue_set_rest);
 // (при необходимости можно добавить кастомные file/ota/action handlers)
 Serial.println("[Server][MongooseGlue] HTTP-API зарегистрированы.");
// Регистрация ручных кастомных эндпоинтов (если нужно):
mongoose_set_http_handlers("getConfig", glue_get_wifi, glue_set_wifi);
mongoose_set_http_handlers("setConfig", glue_set_mqtt, glue_set_mqtt);
// … добавьте все необходимые что генерировал wiz­ard.json …

// Если у вас есть POST-файловая загрузка OTA:
mongoose_set_http_handlers("ota", glue_get_rest, glue_set_rest);
/*
  if (s_http_server == NULL) {
    Serial.println("[Server][Mongoose] Ошибка запуска веб-сервера.");
  } else {
    Serial.println("[Server][Mongoose] Веб-сервер запущен на порт 80.");
  }*/

  // 6. Инициализация MQTT (кем бы мы ни были: ONLINE или AP)
  //mqttClient.begin(cfgManager.getMQTTServer(), cfgManager.getMQTTPort(), wifiClient);
  //mqttClient.setCredentials(cfgManager.getMQTTUser(), cfgManager.getMQTTPass());
  mqttClient.begin(&wifiClient,cfgManager.getMQTTServer(),cfgManager.getMQTTPort(),cfgManager.getClientID(),cfgManager.getMQTTUser(),cfgManager.getMQTTPass());
  // 7. Инициализация RS485 (подключаем RX/TX/DE)
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);
  //rs485.begin(RS485_RX_PIN, RS485_TX_PIN, cfgManager.getRS485Baud());
  rs485.begin(RS485_RX_PIN, RS485_TX_PIN,cfgManager.getRS485Baud(),RS485_DE_PIN);
  rs485.setTimeout(100);

  // 8. Инициализация REST (для обновления настроек при ONLINE)
  restClient.begin(cfgManager.getRESTURL());

  // 9. Инициализация архива (SPIFFS) для сервера
  archiveMgr.begin();


  otaUpdater = new RS485OTAUpdater(rs485);
  otaUpdater->begin("/firmware.bin"); // загружаем прошивку из SPIFFS

  // Задача для рассылки чанков
  xTaskCreatePinnedToCore(
    [](void*) {
      while (otaUpdater->sendNextChunk()) {
        vTaskDelay(pdMS_TO_TICKS(100)); // пауза между чанками
      }
      vTaskDelete(nullptr);
    },
    "ServerOTATask", 4096, nullptr, 2, nullptr, 1
  );
  // ─────────────────────────────────────────────────────────────────────────────
  // 10. Создаём FreeRTOS-задачи для Server Mode
  // Задача для сервера: Mongoose (веб-сервер)
  xTaskCreatePinnedToCore(
    serverMongooseTask,     // функция-задача
    "ServerMongooseTask",   // имя задачи (для отладки)
    4096,                   // размер стека в байтах
    NULL,                   // параметр (не требуется)
    2,                      // приоритет (можно скорректировать)
    NULL,                   // хэндл задачи (не надо хранить)
    0                       // ядро (0 или 1)
  );

  // Задача для RS485 приёма и архивации
  xTaskCreatePinnedToCore(
    serverRS485Task,
    "ServerRS485Task",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Задача для MQTT-отправки архивных записей
  xTaskCreatePinnedToCore(
    serverMQTTTask,
    "ServerMQTTTask",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Задача для дисплея (LVGL-обновление)
  xTaskCreatePinnedToCore(
    serverDisplayTask,
    "ServerDisplayTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

// -----------------------------------------------------------------------------
// === Задача: веб-сервер Mongoose для Server Mode ===
void serverMongooseTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
   // mg_mgr_poll(&mongoose_mgr, 50); // 50 ms тайм-аут
    mongoose_poll();
  vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// -----------------------------------------------------------------------------
// === Задача: приём данных по RS485 и архивирование (Server) ===
void serverRS485Task(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // Если есть данные в буфере RS485 → читаем пакет
    if (rs485.available()) {
    
      RS485Packet pkt;
      if (!rs485.readPacket(pkt)) continue;
    // … обрабатываем pkt

      // Сохраняем «сырую» информацию в архив
      //archiveMgr.addServerRecord(pkt.client_id, pkt.cow_id, pkt.liters, pkt.timestamp, "pending");
      ArchiveRecord r{pkt.cow_id, pkt.timestamp, pkt.liters, pkt.ec, 0};
      archiveMgr.add(r);
      Serial.printf("[ServerRS485] client=%u, cow=%lu, vol=%.2f L, ec=%.2f\n", pkt.client_id,pkt.cow_id, pkt.liters,pkt.ec);
      // Обновляем счётчик уникальных клиентов и последние данные
      displayMgr.showMessage("C" + String(pkt.client_id) + " V=" + String(pkt.liters,2) + " EC=" + String(pkt.ec,2) );
    }
 
    vTaskDelay(pdMS_TO_TICKS(100)); // задержка 100 ms
  }
}

// -----------------------------------------------------------------------------
// === Задача: MQTT-отправка записей из архива (Server) ===
void serverMQTTTask(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    if (serverState == SERVER_ONLINE && wifiConnected) {
      unsigned long now = millis();
      if (now - lastMQTTSend > MQTT_SEND_INTERVAL) {
        lastMQTTSend = now;

        ArchiveRecord rec;
        uint16_t      idx;
        // 1) Ищем первую pending-запись
        if (archiveMgr.getNextPending(idx, rec)) {
          // 2) Формируем MQTT-топик и JSON с клиентом и ec
          String topic = "milk/pum/" + String(rec.client_id) + "/record";
          String payload = String("{")
            + "\"pum_id\":"   + String(rec.client_id)    + ","
            + "\"cow_id\":"   + String(rec.cow_id)       + ","
            + "\"timestamp\":" + String(rec.timestamp)   + ","
            + "\"volume\":"    + String(rec.volume,2)    + ","
            + "\"ec\":"        + String(rec.ec,2)
            + "}";

          // 3) Проверяем соединение
          if (!mqttClient.isConnected()) {
            mqttClient.connect();
          }

          // 4) Публикуем и помечаем
          if (mqttClient.publish(topic, payload)) {
            archiveMgr.updateStatus(idx, /*1=*/1);
            Serial.printf("[ServerMQTT] sent idx=%u pum=%u\n",
                          idx, rec.client_id);
          } else {
            Serial.printf("[ServerMQTT] fail idx=%u\n", idx);
          }
        }
      }
      mqttClient.loop();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// -----------------------------------------------------------------------------
// === Задача: LVGL-обновление экрана (Server) ===
void serverDisplayTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    displayMgr.update();  // lv_timer_handler() + обновление экрана
    vTaskDelay(pdMS_TO_TICKS(10)); // ~100 FPS max (или меньше)
  }
}

// -----------------------------------------------------------------------------
// === РЕАЛИЗАЦИЯ Client Mode ===

void startClientMode() {
  // ─────────────────────────────────────────────────────────────────────────────
  // 1. Инициализация настроек (Preferences)
  cfgManager.begin("milk_cfg");
  if (!cfgManager.hasSavedClientID()) {
    Serial.println("[Client] RS485 Client ID не найден. Нужно настроить в веб-конфиге или вручную.");
  }

  // 2. Инициализация дисплея
  displayMgr.begin();
 // displayMgr.showSplash("Client Mode");
 displayMgr.showStartupScreen("Client Mode");
  // 3. Инициализация RFID
  rfid.begin();

  // 4. Инициализация датчика молока
 // milkSensor.begin();
 milkSensor.begin(/*pulsePin=*/4, /*litersPerPulse=*/0.0025f);
  // 5. Инициализация RS485
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);
 // rs485.begin(RS485_RX_PIN, RS485_TX_PIN, cfgManager.getRS485Baud());
 rs485.begin(RS485_RX_PIN,RS485_TX_PIN,cfgManager.getRS485Baud(),RS485_DE_PIN);
  otaReceiver = new OTAReceiver(rs485);
  rs485.setTimeout(100);

  // 6. Инициализация архива (SPIFFS) для клиента
  archiveMgr.begin();

  // 7. Стартовый экран в клиенте
 // displayMgr.showClientStatus("Idle");
 displayMgr.showClientStatus(0, "—", 0.0f);
  // ─────────────────────────────────────────────────────────────────────────────
  // 8. Создаём FreeRTOS-задачи для Client Mode

  // Задача для сканирования RFID
  xTaskCreatePinnedToCore(
    clientRFIDTask,
    "ClientRFIDTask",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Задача для измерения молока (переходит в работу, когда RFID получен)
  xTaskCreatePinnedToCore(
    clientMilkTask,
    "ClientMilkTask",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Задача для RS485-передачи записей
  xTaskCreatePinnedToCore(
    clientRS485Task,
    "ClientRS485Task",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Задача для дисплея (LVGL)
  xTaskCreatePinnedToCore(
    clientDisplayTask,
    "ClientDisplayTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

// -----------------------------------------------------------------------------
// === Задача: сканирование RFID (Client) ===
void clientRFIDTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // 1) rfid.available() остаётся как есть
    if (rfid.available()) {
      // 2) вместо readCowID() используем readRFID()
      String cow_id = rfid.readRFID();
      Serial.println("[ClientRFID] Scanned cow: " + cow_id);

      // 3) сохраняем локально, чтобы clientMilkTask мог к нему обратиться
      lastCowID = cow_id;

      // 4) отображаем на экране — у DisplayManager есть showMessage()
      displayMgr.showMessage("RFID: " + cow_id);

      // 5) переключаем состояние
      clientState = CLIENT_SCANNING;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// -----------------------------------------------------------------------------
// === Задача: измерение молока (Client) ===
/*void clientMilkTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (clientState == CLIENT_SCANNING) {
      String cow_id = rfid.getLastCowID();
      displayMgr.showMilkProgress(0);

      // Блокирует, пока не будет завершено измерение
      float volume = milkSensor.measureMilk(); 
       // 3) Сохраняем объём в глобальную переменную
       lastVolume = volume;

      Serial.printf("[ClientMilk] Измерили %.2f L для cow_id=%s\n", volume, cow_id.c_str());

      // Сохраняем запись в локальный архив
      String timestamp = archiveMgr.getCurrentTimestamp();
      archiveMgr.addClientRecord(cow_id, timestamp, volume, "pending");

      displayMgr.showMilkProgress(100);

      // Готовимся к передаче
      clientState = CLIENT_MEASURING;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}*/
void clientMilkTask(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    if (clientState == CLIENT_SCANNING) {
      // 1) Берём последний отсканированный ID
      String cow_id = lastCowID;

      // 2) Сбрасываем предыдущие данные и начинаем измерение заново
      milkSensor.reset();
      displayMgr.showMessage("Measuring milk...");

      // 3) Делаем пару циклов обновления сенсора
      //    (реальную логику «блокировки» замените на вашу)
      for (int i = 0; i < 50; ++i) {
        milkSensor.update();
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      // 4) Получаем итоговый объём
      float volume = milkSensor.getVolumeLiters();
      lastVolume = volume;  // сохраняем для дисплея

      Serial.printf("[ClientMilk] Measured %.2f L for cow_id=%s\n",
                    volume, cow_id.c_str());

      // 5) Сохраняем запись в локальный архив
      //    Пример структуры ArchiveRecord: client_id, cow_id, timestamp, volume, ec, status
      ArchiveRecord rec {
        /*client_id=*/ (uint8_t)cfgManager.getClientID().toInt(),
        /*cow_id   =*/ cow_id.toInt(),
        /*timestamp */ (uint32_t)(millis() / 1000),
        /*volume   =*/ volume,
        /*ec       =*/ 0.0f,
        /*status   =*/ 0
      };
      archiveMgr.add(rec);

      // 6) Выводим результат
      displayMgr.showMessage("Done: " + String(volume,2) + " L");

      // 7) Переходим к отправке
      clientState = CLIENT_MEASURING;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// -----------------------------------------------------------------------------
// === Задача: RS485-передача записей (Client) ===
/*
void clientRS485Task(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
      // 1) Сначала обрабатываем OTA через RS485 (если есть)
      otaReceiver->handle();

      // 2) Если мы в состоянии измерили или отправляем
      if (clientState == CLIENT_MEASURING || clientState == CLIENT_SENDING) {
          // 2.1) Проверяем, восстановилась ли связь
          if (rs485.isConnected()) {
              // 2.2) Перебираем все записи в EEPROM-архиве
              ArchiveRecord rec;
              for (uint16_t idx = 0; idx < ArchiveManager::MAX_RECORDS; ++idx) {
                  // читаем по индексу :contentReference[oaicite:0]{index=0}
                  if (archiveMgr.readRecord(idx, rec) && rec.status == 0) {
                      // формируем пакет для отправки :contentReference[oaicite:1]{index=1}
                      RS485Packet pkt;
                      pkt.client_id = (uint8_t)cfgManager.getClientID().toInt();
                      pkt.cow_id    = rec.cow_id;
                      pkt.liters    = rec.volume;
                      pkt.timestamp = rec.timestamp;
                      pkt.ec        = rec.ec;  // если вы расширили ArchiveRecord под ec

                      // отправляем
                      if (rs485.sendPacket(pkt)) {
                          // помечаем как sent (1) :contentReference[oaicite:2]{index=2}
                          archiveMgr.updateStatus(idx, 1);
                          Serial.printf("[ClientRS485] Sent cow_id=%lu\n", rec.cow_id);
                      }
                  }
              }
              // после отправки возвращаемся в idle
              clientState = CLIENT_IDLE;
          } else {
              // Если RS485 всё ещё недоступен — показываем статус
              // showClientStatus(кол-во клиентов, последний cowID, последний объем)
              displayMgr.showClientStatus(0, "-", 0.0f);  :contentReference[oaicite:3]{index=3}
              clientState = CLIENT_SENDING;
          }
      }

      // 3) Задержка, чтобы не грузить ЦП
      vTaskDelay(pdMS_TO_TICKS(500));
  }
}
  */

  void clientRS485Task(void *pvParameters) {
    (void)pvParameters;
  
    for (;;) {
      // 1) Сначала обработка OTA (если придут пакеты прошивки)
      otaReceiver->handle();
  
      // 2) Если нужно отправлять
      if (clientState == CLIENT_MEASURING || clientState == CLIENT_SENDING) {
        if (rs485.isConnected()) {
          // 2.1) Ищем первую pending-запись
          uint16_t idx;
          ArchiveRecord rec;
          if (archiveMgr.getNextPending(idx, rec)) {
            // 2.2) Формируем пакет
            RS485Packet pkt;
            pkt.client_id = (uint32_t)cfgManager.getClientID().toInt(); 
            pkt.cow_id    = rec.cow_id;
            pkt.liters    = rec.volume;
            pkt.timestamp = rec.timestamp;
            pkt.ec        = rec.ec;
  
            // 2.3) Отправляем
            if (rs485.sendPacket(pkt)) {
              // 2.4) Помечаем как sent
              archiveMgr.updateStatus(idx, /*1=*/1);
              Serial.printf("[ClientRS485] Sent idx=%u cow=%lu\n",
                            idx, rec.cow_id);
            } else {
              Serial.printf("[ClientRS485] Fail to send idx=%u\n", idx);
            }
          }
          // вернёмся в idle
          clientState = CLIENT_IDLE;
        } else {
          // 2.5) RS485 всё ещё недоступен
          displayMgr.showMessage("RS485 disconnected");
          clientState = CLIENT_SENDING;
        }
      }
  
      // 3) Небольшая пауза
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
// -----------------------------------------------------------------------------
// === Задача: LVGL-обновление экрана (Client) ===
/*void clientDisplayTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // 1) Считаем количество pending-записей в архиве
    uint16_t pendingCount = 0;
    ArchiveRecord rec;
    // getNextPending не удаляет запись, просто возвращает следующую
    while (archiveMgr.getNextPending(rec)) {
      pendingCount++;
    }

    // 2) Показываем на экране: кол-во клиентов, последний ID и объём
    displayMgr.showClientStatus(
       pendingCount,
       lastCowID,
        lastVolume
    );

    // 3) Оновление дисплея вместо loop()
    displayMgr.update();

    vTaskDelay(pdMS_TO_TICKS(500)); // обновляем раз в полсекунды
  }
}
*/
void clientDisplayTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // 1) Считаем количество pending-записей в архиве
    uint16_t pendingCount = 0;
    ArchiveRecord rec;
    uint16_t    idx;

    // правильно: передаём два аргумента — индекс и запись
    while (archiveMgr.getNextPending(idx, rec)) {
      pendingCount++;
    }

    // 2) Показываем на экране: кол-во записей в ожидании, последний ID и объём
    // Клиентский счётчик — uint8_t, поэтому можно привести:
    displayMgr.showClientStatus(
      /*clientCount=*/ (uint8_t)pendingCount,
      /*lastCowId=*/   lastCowID,
      /*lastVolume=*/  lastVolume
    );

    // 3) Обновление дисплея
    displayMgr.update();

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
// -----------------------------------------------------------------------------
// === Обработчик HTTP-запросов Mongoose (для обоих режимов) ===
void mongooseEventHandler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
 /* if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    if (mg_http_match_uri(hm, "/")) {
      // Главная страница (Dashboard)
      mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                    "<html><body><h1>Milk Server Dashboard</h1>"
                    "<p>Clients, Status, etc.</p></body></html>");
    }
    else if (mg_http_match_uri(hm, "/api/getConfig")) {
      String json = cfgManager.getConfigJSON();
      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json.c_str());
    }
    else if (mg_http_match_uri(hm, "/api/setConfig")) {
      // Обрабатываем POST-запрос с JSON в теле
      char buf[1024] = {0};
      int len = (int)hm->body.len;
      if (len > 0 && len < sizeof(buf)) {
        memcpy(buf, hm->body.ptr, len);
        cfgManager.saveConfigFromJSON(String(buf));
        mg_http_reply(c, 200, "", "OK");
      } else {
        mg_http_reply(c, 400, "", "Bad Request");
      }
    }
    else if (mg_http_match_uri(hm, "/api/exportArchive")) {
      // получаем JSON всего архива
      String archiveJson = archiveMgr.getArchiveJson();
      mg_http_reply(c,
                    200,
                    "Content-Type: application/json\r\n",
                    "%s",
                    archiveJson.c_str());
    }
    else {
      mg_http_reply(c, 404, "", "Not Found");
    }
  }*/
}
