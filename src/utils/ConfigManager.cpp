#include "ConfigManager.h"

void ConfigManager::begin(const char* ns) {
    _prefs.begin(ns, false);
}

// Проверяет, есть ли SSID и пароль
bool ConfigManager::hasSavedConfig()  {
    return _prefs.isKey(KEY_SSID) && _prefs.isKey(KEY_PASSWORD);
}

// Проверяет, есть ли RS485 ID
bool ConfigManager::hasSavedClientID() {
    return _prefs.isKey(KEY_RS485_ID);
}

// Считывает SSID и пароль из Preferences
bool ConfigManager::getWiFiCredentials() {
    savedSSID = _getString(KEY_SSID, "");
    savedPassword = _getString(KEY_PASSWORD, "");
    return (savedSSID.length() > 0 && savedPassword.length() > 0);
}

// Возвращает Client ID (RS485)
String ConfigManager::getClientID()  {
    return _getString(KEY_RS485_ID, "");
}

// Возвращает скорость RS485
uint32_t ConfigManager::getRS485Baud()  {
    return _getUInt32(KEY_RS485_BAUD, 9600);
}

// Возвращает MQTT сервер
String ConfigManager::getMQTTServer()  {
    return _getString(KEY_MQTT_SERVER, "");
}

// Возвращает порт MQTT
uint16_t ConfigManager::getMQTTPort()  {
    return static_cast<uint16_t>(_getUInt32(KEY_MQTT_PORT, 1883));
}

// Возвращает MQTT пользователь
String ConfigManager::getMQTTUser()  {
    return _getString(KEY_MQTT_USER, "");
}

// Возвращает MQTT пароль
String ConfigManager::getMQTTPass()  {
    return _getString(KEY_MQTT_PASS, "");
}

// Возвращает REST URL
String ConfigManager::getRESTURL()  {
    return _getString(KEY_REST_URL, "");
}

// Формирует JSON с текущими настройками
String ConfigManager::getConfigJSON()  {
    // Оценим размер документа: 
    // SSID (~32), password (~64), rs485_id (~10), mqtt strings (~64), rest_url (~128)
    DynamicJsonDocument doc(512);

    doc["ssid"] = _getString(KEY_SSID, "");
    doc["password"] = _getString(KEY_PASSWORD, "");
    doc["rs485_id"] = _getString(KEY_RS485_ID, "");
    doc["rs485_baud"] = static_cast<uint32_t>(_getUInt32(KEY_RS485_BAUD, 9600));
    doc["mqtt_server"] = _getString(KEY_MQTT_SERVER, "");
    doc["mqtt_port"] = static_cast<uint32_t>(_getUInt32(KEY_MQTT_PORT, 1883));
    doc["mqtt_user"] = _getString(KEY_MQTT_USER, "");
    doc["mqtt_pass"] = _getString(KEY_MQTT_PASS, "");
    doc["rest_url"] = _getString(KEY_REST_URL, "");

    String output;
    serializeJson(doc, output);
    return output;
}

// Парсит JSON и сохраняет параметры в Preferences
void ConfigManager::saveConfigFromJSON(const String& jsonStr) {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        // Ошибка при парсинге JSON — ничего не сохраняем
        return;
    }

    if (doc.containsKey("ssid")) {
        String ssid = doc["ssid"].as<const char*>();
        _saveString(KEY_SSID, ssid);
    }
    if (doc.containsKey("password")) {
        String pass = doc["password"].as<const char*>();
        _saveString(KEY_PASSWORD, pass);
    }
    if (doc.containsKey("rs485_id")) {
        String rsid = doc["rs485_id"].as<const char*>();
        _saveString(KEY_RS485_ID, rsid);
    }
    if (doc.containsKey("rs485_baud")) {
        uint32_t baud = doc["rs485_baud"].as<uint32_t>();
        _saveUInt32(KEY_RS485_BAUD, baud);
    }
    if (doc.containsKey("mqtt_server")) {
        String mserv = doc["mqtt_server"].as<const char*>();
        _saveString(KEY_MQTT_SERVER, mserv);
    }
    if (doc.containsKey("mqtt_port")) {
        uint32_t mport = doc["mqtt_port"].as<uint32_t>();
        _saveUInt32(KEY_MQTT_PORT, mport);
    }
    if (doc.containsKey("mqtt_user")) {
        String musr = doc["mqtt_user"].as<const char*>();
        _saveString(KEY_MQTT_USER, musr);
    }
    if (doc.containsKey("mqtt_pass")) {
        String mpwd = doc["mqtt_pass"].as<const char*>();
        _saveString(KEY_MQTT_PASS, mpwd);
    }
    if (doc.containsKey("rest_url")) {
        String rur = doc["rest_url"].as<const char*>();
        _saveString(KEY_REST_URL, rur);
    }
}
void ConfigManager::saveWiFiCredentials(const String& ssid, const String& password) {
    // Сохраняем SSID
    if (ssid.length() > 0) {
        _prefs.putString(KEY_SSID, ssid);
    } else {
        _prefs.remove(KEY_SSID);
    }
    // Сохраняем пароль
    if (password.length() > 0) {
        _prefs.putString(KEY_PASSWORD, password);
    } else {
        _prefs.remove(KEY_PASSWORD);
    }
}
void ConfigManager::commit() {
    _prefs.end();
    _prefs.begin("milk_cfg", false);  // замените "milk_cfg", если другой ns
}
// Сохраняет строку в Preferences
void ConfigManager::_saveString(const char* key, const String& value) {
    if (value.length() > 0) {
        _prefs.putString(key, value);
    } else {
        _prefs.remove(key);
    }
}

// Читает строку из Preferences, если нет — возвращает defaultValue
String ConfigManager::_getString(const char* key, const String& defaultValue)  {
    return _prefs.getString(key, defaultValue);
}

// Сохраняет uint32_t в Preferences
void ConfigManager::_saveUInt32(const char* key, uint32_t value) {
    _prefs.putUInt(key, value);
}

// Читает uint32_t из Preferences, если нет — возвращает defaultValue
uint32_t ConfigManager::_getUInt32(const char* key, uint32_t defaultValue)  {
    return _prefs.getUInt(key, defaultValue);
}
void ConfigManager::saveRS485ID(const String& id) {
    _saveString(KEY_RS485_ID, id);
}

void ConfigManager::saveRS485Baud(uint32_t baud) {
    _saveUInt32(KEY_RS485_BAUD, baud);
}

void ConfigManager::saveMQTTServer(const String& addr) {
    _saveString(KEY_MQTT_SERVER, addr);
}

void ConfigManager::saveMQTTUser(const String& user) {
    _saveString(KEY_MQTT_USER, user);
}

void ConfigManager::saveMQTTPass(const String& pass) {
    _saveString(KEY_MQTT_PASS, pass);
}

void ConfigManager::saveRESTURL(const String& url) {
    _saveString(KEY_REST_URL, url);
}