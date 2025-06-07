#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <mongoose.h>
#include <lvgl.h>
#include "utils/ConfigManager.h"
#include "utils/RS485Manager.h"
#include "utils/MQTTManager.h"
#include "utils/RESTManager.h"
#include "utils/RFIDManager.h"
#include "utils/MilkSensor.h"
#include "utils/ArchiveManager.h"
#include "utils/DisplayManager.h"

// -----------------------------------------------------------------------------
// === ПИНЫ ===
#define GPIO_TOGGLE_PIN     2    // Переключатель Server/Client
#define LED_STATUS_PIN      13   // Индикатор статуса
#define RS485_RX_PIN        16   // пример пин для RS485 RX
#define RS485_TX_PIN        17   // пример пин для RS485 TX
#define RS485_DE_PIN        18   // пин DE на трансивере RS485

#define AP_SSID             "ESP32_MILK_SERVER"
#define AP_PASSWORD         "12345678"

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
  if (!LittleFS.begin()) {
    Serial.println("Ошибка: не удалось смонтировать LittleFS");
  }

  // 3) Считываем переключатель, чтобы определить режим
  bool toggle = digitalRead(GPIO_TOGGLE_PIN);

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

  // 2. Инициализация дисплея (LVGL)
  displayMgr.begin();                      // Конфигурируем LVGL, дисплей
  displayMgr.showSplash("Server Mode");    // Заставка

  // 3. Подключение к Wi-Fi (если есть сохранённые креды)
  bool gotSSID = cfgManager.getWiFiCredentials();
  if (gotSSID) {
    Serial.println("[Server] Попытка подключения к Wi-Fi: " + cfgManager.savedSSID);
    displayMgr.showStatus("Wi-Fi: Connecting...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfgManager.savedSSID.c_str(), cfgManager.savedPassword.c_str());
    unsigned long start = millis();
    while (millis() - start < 15000) {
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("[Server] Подключились к Wi-Fi: " + WiFi.localIP().toString());
        displayMgr.showStatus("Wi-Fi: Connected");
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
    displayMgr.showAPInfo(AP_SSID, AP_PASSWORD, ip.toString());
  }

  // 5. Инициализация Mongoose (для обоих состояний: AP и ONLINE)
  mg_mgr_init(&mongoose_mgr);
  s_http_server = mg_http_listen(&mongoose_mgr, "http://0.0.0.0:80", mongooseEventHandler, NULL);
  if (s_http_server == NULL) {
    Serial.println("[Server][Mongoose] Ошибка запуска веб-сервера.");
  } else {
    Serial.println("[Server][Mongoose] Веб-сервер запущен на порт 80.");
  }

  // 6. Инициализация MQTT (кем бы мы ни были: ONLINE или AP)
  mqttClient.begin(cfgManager.getMQTTServer(), cfgManager.getMQTTPort(), wifiClient);
  mqttClient.setCredentials(cfgManager.getMQTTUser(), cfgManager.getMQTTPass());

  // 7. Инициализация RS485 (подключаем RX/TX/DE)
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);
  rs485.begin(RS485_RX_PIN, RS485_TX_PIN, cfgManager.getRS485Baud());
  rs485.setTimeout(100);

  // 8. Инициализация REST (для обновления настроек при ONLINE)
  restClient.begin(cfgManager.getRESTURL());

  // 9. Инициализация архива (SPIFFS) для сервера
  archiveMgr.begin("/archive_server");

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
    mg_mgr_poll(&mongoose_mgr, 50); // 50 ms тайм-аут
  }
}

// -----------------------------------------------------------------------------
// === Задача: приём данных по RS485 и архивирование (Server) ===
void serverRS485Task(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // Если есть данные в буфере RS485 → читаем пакет
    if (rs485.available()) {
      RS485Packet pkt = rs485.readPacket();
      // Сохраняем «сырую» информацию в архив
      archiveMgr.addServerRecord(pkt.client_id, pkt.cow_id, pkt.liters, pkt.timestamp, "pending");
      Serial.printf("[ServerRS485] Пришли данные: client=%s, cow=%s, %.2f L\n", 
                    pkt.client_id.c_str(), pkt.cow_id.c_str(), pkt.liters);
      // Обновляем счётчик уникальных клиентов и последние данные
      displayMgr.updateClientCount(archiveMgr.getUniqueClientCount());
      displayMgr.updateLastData(pkt.cow_id, pkt.liters);
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // задержка 100 ms
  }
}

// -----------------------------------------------------------------------------
// === Задача: MQTT-отправка записей из архива (Server) ===
void serverMQTTTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (serverState == SERVER_ONLINE && wifiConnected) {
      unsigned long now = millis();
      if (now - lastMQTTSend > MQTT_SEND_INTERVAL) {
        lastMQTTSend = now;
        // Читаем все pending-записи из архива
        std::vector<ArchiveRecord> pending = archiveMgr.getPendingServerRecords();
        for (auto &rec : pending) {
          bool ok = mqttClient.publish(rec.topic.c_str(), rec.payload.c_str());
          if (ok) {
            archiveMgr.markServerRecordAsSent(rec.id);
            Serial.printf("[ServerMQTT] Отправили запись (ID=%u)\n", rec.id);
          }
        }
      }
      // Для поддержания MQTT-потока
      if (! mqttClient.connected()) {
        mqttClient.connect("ESP32_MILK_SERVER"); // clientID можно генерировать
      }
      mqttClient.loop();
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // проверяем каждые 500 ms
  }
}

// -----------------------------------------------------------------------------
// === Задача: LVGL-обновление экрана (Server) ===
void serverDisplayTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    displayMgr.loop();  // lv_timer_handler() + обновление экрана
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
  displayMgr.showSplash("Client Mode");

  // 3. Инициализация RFID
  rfid.begin();

  // 4. Инициализация датчика молока
  milkSensor.begin();

  // 5. Инициализация RS485
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);
  rs485.begin(RS485_RX_PIN, RS485_TX_PIN, cfgManager.getRS485Baud());
  rs485.setTimeout(100);

  // 6. Инициализация архива (SPIFFS) для клиента
  archiveMgr.begin("/archive_client");

  // 7. Стартовый экран в клиенте
  displayMgr.showClientStatus("Idle");

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
    if (rfid.available()) {
      String cow_id = rfid.readCowID();
      Serial.println("[ClientRFID] Сканировали корову: " + cow_id);

      // Сохраним ID для следующей задачи измерения молока
      rfid.setLastCowID(cow_id);

      // Обновляем состояние на дисплее
      displayMgr.showRFIDStatus(cow_id);

      // Переходим в состояние «сканирование завершено»
      clientState = CLIENT_SCANNING;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// -----------------------------------------------------------------------------
// === Задача: измерение молока (Client) ===
void clientMilkTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (clientState == CLIENT_SCANNING) {
      String cow_id = rfid.getLastCowID();
      displayMgr.showMilkProgress(0);

      // Блокирует, пока не будет завершено измерение
      float volume = milkSensor.measureMilk(); 
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
}

// -----------------------------------------------------------------------------
// === Задача: RS485-передача записей (Client) ===
void clientRS485Task(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (clientState == CLIENT_MEASURING || clientState == CLIENT_SENDING) {
      // Если RS485 доступен (можно опросить isConnected() или просто попробовать отправить)
      if (rs485.isConnected()) {
        // Читаем все pending-записи
        std::vector<ClientRecord> pending = archiveMgr.getPendingClientRecords();
        for (auto &rec : pending) {
          RS485Packet pkt;
          pkt.client_id = cfgManager.getClientID();
          pkt.cow_id    = rec.cow_id;
          pkt.liters    = rec.volume;
          pkt.timestamp = rec.datetime;

          bool sent = rs485.sendPacket(pkt);
          if (sent) {
            archiveMgr.markClientRecordAsSent(rec.id);
            Serial.printf("[ClientRS485] Отправили запись (cow=%s)\n", rec.cow_id.c_str());
          }
        }
        clientState = CLIENT_IDLE;
      } else {
        displayMgr.showClientStatus("RS485 disconnected");
        clientState = CLIENT_SENDING;
      }
    }
    // Следим, если слишком много записей → обрезаем старые
    archiveMgr.maintainClientArchiveSize();

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// -----------------------------------------------------------------------------
// === Задача: LVGL-обновление экрана (Client) ===
void clientDisplayTask(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // Обновляем статус на экране (статистика за день, статус RFID/RS485)
    String stats = archiveMgr.getTodayStats(); 
    displayMgr.showClientStats(stats);

    displayMgr.loop();  // lv_timer_handler()
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// -----------------------------------------------------------------------------
// === Обработчик HTTP-запросов Mongoose (для обоих режимов) ===
void mongooseEventHandler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
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
      String archiveJson = archiveMgr.getServerArchiveJSON();
      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", archiveJson.c_str());
    }
    else {
      mg_http_reply(c, 404, "", "Not Found");
    }
  }
}
