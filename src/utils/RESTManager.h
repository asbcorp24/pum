#ifndef REST_MANAGER_H
#define REST_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class RESTManager {
public:
    /**
     * @brief Инициализация
     * @param baseUrl — базовый URL (например, https://api.example.com)
     * @param token — Bearer-токен (если требуется)
     */
    void begin(const String& baseUrl, const String& token = "");

    /**
     * @brief Получить JSON-настройки по GET-запросу
     * @param endpoint — путь, например "/api/device/config"
     * @param json — объект для парсинга
     * @return true, если успешно
     */
    bool fetchConfig(const String& endpoint, JsonDocument& json);

    /**
     * @brief Проверка и выполнение OTA-обновления прошивки
     * @param endpoint — путь для запроса версии и бинарника
     * @param currentVersion — текущая версия прошивки
     * @return true, если начато обновление
     */
    bool checkForFirmwareUpdate(const String& endpoint, const String& currentVersion);

private:
    String _baseUrl;
    String _token;

    void _applyCommonHeaders(HTTPClient& http);
};

#endif // REST_MANAGER_H
