#include "MQTTManager.h"

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager() {
    _instance = this;
}

void MQTTManager::begin(WiFiClient* wifiClient, const String& broker, uint16_t port,
                        const String& clientId, const String& login, const String& password) {
    _wifiClient = wifiClient;
    _broker = broker;
    _port = port;
    _clientId = clientId;
    _login = login;
    _password = password;

    _mqttClient.setClient(*_wifiClient);
    _mqttClient.setServer(_broker.c_str(), _port);
    _mqttClient.setCallback(_internalCallback);
}

bool MQTTManager::connect() {
    if (_mqttClient.connected()) return true;

    Serial.print("[MQTT] Connecting to ");
    Serial.print(_broker);
    Serial.print(" as ");
    Serial.println(_clientId);

    bool success;
    if (_login.length() > 0) {
        success = _mqttClient.connect(_clientId.c_str(), _login.c_str(), _password.c_str());
    } else {
        success = _mqttClient.connect(_clientId.c_str());
    }

    if (success) {
        Serial.println("[MQTT] Connected ✅");
    } else {
        Serial.print("[MQTT] Failed ❌ rc=");
        Serial.println(_mqttClient.state());
    }

    return success;
}

void MQTTManager::loop() {
    if (!_mqttClient.connected()) {
        connect();
    }
    _mqttClient.loop();
}

bool MQTTManager::isConnected() const {
    return _mqttClient.connected();
}

bool MQTTManager::publish(const String& topic, const String& payload) {
    if (!_mqttClient.connected()) return false;

    Serial.printf("[MQTT] Publishing to %s: %s\n", topic.c_str(), payload.c_str());
    return _mqttClient.publish(topic.c_str(), payload.c_str());
}

bool MQTTManager::subscribe(const String& topic) {
    if (!_mqttClient.connected()) return false;

    Serial.printf("[MQTT] Subscribing to %s\n", topic.c_str());
    return _mqttClient.subscribe(topic.c_str());
}

void MQTTManager::onMessage(std::function<void(String topic, String payload)> cb) {
    _messageCallback = cb;
}

void MQTTManager::_internalCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance && _instance->_messageCallback) {
        String topicStr = String(topic);
        String payloadStr;

        for (unsigned int i = 0; i < length; ++i) {
            payloadStr += (char)payload[i];
        }

        Serial.printf("[MQTT] Incoming message on %s: %s\n", topic, payloadStr.c_str());
        _instance->_messageCallback(topicStr, payloadStr);
    }
}
