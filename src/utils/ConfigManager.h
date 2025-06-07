#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

/**
 * @brief Класс для управления конфигурацией в Preferences на ESP32.
 * 
 * Хранит и читает следующие параметры:
 *  - SSID и пароль Wi-Fi
 *  - RS485 Client ID
 *  - Скорость RS485 (baud rate)
 *  - MQTT сервер, порт, логин, пароль
 *  - REST URL
 * 
 * Также умеет сериализовать/десериализовать настройки в/из JSON,
 * что удобно для веб-конфигурирования через Mongoose.
 */
class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() = default;

    /**
     * @brief Инициализирует Preferences с указанным namespace.
     * 
     * @param ns Название namespace, например "milk_cfg".
     */
    void begin(const char* ns);

    /**
     * @brief Проверяет, сохранены ли SSID и пароль Wi-Fi.
     * 
     * @return true, если и ключ "ssid", и ключ "password" присутствуют.
     * @return false, если один из ключей отсутствует.
     */
    bool hasSavedConfig() const;

    /**
     * @brief Проверяет, сохранён ли RS485 Client ID (для Client Mode).
     * 
     * @return true, если ключ "rs485_id" присутствует.
     * @return false, иначе.
     */
    bool hasSavedClientID() const;

    /**
     * @brief Считывает SSID и пароль из Preferences в поля savedSSID и savedPassword.
     * 
     * @return true, если обе строки (SSID и пароль) не пустые.
     * @return false, если хотя бы одна строка пустая.
     */
    bool getWiFiCredentials();

    /**
     * @brief Возвращает сохранённый RS485 Client ID.
     * 
     * @return String — client ID (например, "A1").
     */
    String getClientID() const;

    /**
     * @brief Возвращает сохранённый baud rate для RS485.
     * 
     * @return uint32_t — скорость, например 9600.
     */
    uint32_t getRS485Baud() const;

    /**
     * @brief Возвращает адрес MQTT-брокера (IP или hostname).
     * 
     * @return String — адрес брокера.
     */
    String getMQTTServer() const;

    /**
     * @brief Возвращает порт MQTT-брокера.
     * 
     * @return uint16_t — порт, например 1883.
     */
    uint16_t getMQTTPort() const;

    /**
     * @brief Возвращает логин для подключения к MQTT-брокеру.
     * 
     * @return String — имя пользователя.
     */
    String getMQTTUser() const;

    /**
     * @brief Возвращает пароль для подключения к MQTT-брокеру.
     * 
     * @return String — пароль.
     */
    String getMQTTPass() const;

    /**
     * @brief Возвращает базовый URL для REST API.
     * 
     * @return String — например "https://api.example.com".
     */
    String getRESTURL() const;

    /**
     * @brief Генерирует JSON-строку с текущими настройками.
     * 
     * Формат:
     * {
     *   "ssid": "...",
     *   "password": "...",
     *   "rs485_id": "...",
     *   "rs485_baud": 9600,
     *   "mqtt_server": "...",
     *   "mqtt_port": 1883,
     *   "mqtt_user": "...",
     *   "mqtt_pass": "...",
     *   "rest_url": "..."
     * }
     * 
     * @return String — JSON с параметрами.
     */
    String getConfigJSON() const;

    /**
     * @brief Парсит JSON-строку и сохраняет все поля в Preferences.
     * 
     * Ожидаемый формат:
     * {
     *   "ssid": "MySSID",
     *   "password": "MyPass",
     *   "rs485_id": "A1",
     *   "rs485_baud": 9600,
     *   "mqtt_server": "broker.example.com",
     *   "mqtt_port": 1883,
     *   "mqtt_user": "user",
     *   "mqtt_pass": "pass",
     *   "rest_url": "https://api.example.com"
     * }
     * 
     * @param jsonStr JSON с обновлёнными параметрами.
     */
    void saveConfigFromJSON(const String& jsonStr);

    // Публичные поля для прямого доступа (если нужно)
    String savedSSID;
    String savedPassword;

private:
    Preferences _prefs;  ///< Объект Preferences

    // Вспомогательные методы для чтения/записи отдельных ключей:
    void _saveString(const char* key, const String& value);
    String _getString(const char* key, const String& defaultValue = "") const;
    void _saveUInt32(const char* key, uint32_t value);
    uint32_t _getUInt32(const char* key, uint32_t defaultValue = 0) const;

    // Ключи в Preferences:
    static constexpr const char* KEY_SSID        = "ssid";
    static constexpr const char* KEY_PASSWORD    = "password";
    static constexpr const char* KEY_RS485_ID    = "rs485_id";
    static constexpr const char* KEY_RS485_BAUD  = "rs485_baud";
    static constexpr const char* KEY_MQTT_SERVER = "mqtt_srv";
    static constexpr const char* KEY_MQTT_PORT   = "mqtt_prt";
    static constexpr const char* KEY_MQTT_USER   = "mqtt_usr";
    static constexpr const char* KEY_MQTT_PASS   = "mqtt_pwd";
    static constexpr const char* KEY_REST_URL    = "rest_url";
};

#endif // CONFIG_MANAGER_H
