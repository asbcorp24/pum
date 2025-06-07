#include "RESTManager.h"
#include <Update.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>

void RESTManager::begin(const String& baseUrl, const String& token) {
    _baseUrl = baseUrl;
    _token = token;
}

void RESTManager::_applyCommonHeaders(HTTPClient& http) {
    http.setUserAgent("ESP32-MilkCounter");
    http.addHeader("Content-Type", "application/json");

    if (_token.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _token);
    }
}

bool RESTManager::fetchConfig(const String& endpoint, JsonDocument& json) {
    String url = _baseUrl + endpoint;
    Serial.println("[REST] Fetching config: " + url);

    HTTPClient http;
    http.begin(url);
    _applyCommonHeaders(http);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[REST] Failed to get config. Code: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    DeserializationError error = deserializeJson(json, payload);
    if (error) {
        Serial.println("[REST] Failed to parse JSON: " + String(error.c_str()));
        http.end();
        return false;
    }

    Serial.println("[REST] Config received and parsed successfully.");
    http.end();
    return true;
}

bool RESTManager::checkForFirmwareUpdate(const String& endpoint, const String& currentVersion) {
    String url = _baseUrl + endpoint;
    Serial.println("[REST] Checking for firmware update: " + url);

    HTTPClient http;
    http.begin(url);
    _applyCommonHeaders(http);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[REST] Failed to check update. Code: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.println("[REST] Failed to parse update info JSON.");
        http.end();
        return false;
    }

    String latestVersion = doc["version"] | "";
    String binaryUrl = doc["binary_url"] | "";

    if (latestVersion == "" || binaryUrl == "") {
        Serial.println("[REST] Incomplete update info.");
        http.end();
        return false;
    }

    if (latestVersion == currentVersion) {
        Serial.println("[REST] Firmware up to date.");
        http.end();
        return false;
    }

    Serial.printf("[REST] New firmware available: %s → %s\n", currentVersion.c_str(), latestVersion.c_str());
    http.end();

    // Start OTA
    WiFiClient client;
    HTTPUpdateResult result = httpUpdate.update(client, binaryUrl);

    switch (result) {
        case HTTP_UPDATE_OK:
            Serial.println("[REST] OTA Update Success. Rebooting...");
            return true;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[REST] No update available.");
            return false;
        case HTTP_UPDATE_FAILED:
            Serial.printf("[REST] OTA Update failed. Error (%d): %s\n",
                          httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            return false;
    }

    return false;
}
