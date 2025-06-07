#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

class MQTTManager {
public:
    /**
     * @brief Конструктор
     */
    MQTTManager();

    /**
     * @brief Инициализация MQTT-клиента
     * 
     * @param wifiClient — указатель на внешний WiFiClient (обычно &client)
     * @param broker     — адрес MQTT брокера (IP или DNS)
     * @param port       — порт (обычно 1883)
     * @param clientId   — уникальный идентификатор клиента
     * @param login      — логин (может быть пустым)
     * @param password   — пароль (может быть пустым)
     */
    void begin(WiFiClient* wifiClient, const String& broker, uint16_t port,
               const String& clientId, const String& login = "", const String& password = "");

    /**
     * @brief Основной loop MQTT
     */
    void loop();

    /**
     * @brief Подключение к брокеру (если не подключен)
     * @return true если успешно
     */
    bool connect();

    /**
     * @brief Проверка соединения
     */
    bool isConnected() const;

    /**
     * @brief Отправка сообщения
     * @param topic MQTT топик
     * @param payload сообщение (строка)
     * @return true — если отправлено
     */
    bool publish(const String& topic, const String& payload);

    /**
     * @brief Подписка на топик
     */
    bool subscribe(const String& topic);

    /**
     * @brief Колбэк при получении сообщений
     */
    void onMessage(std::function<void(String topic, String payload)> cb);

private:
    WiFiClient* _wifiClient;
    PubSubClient _mqttClient;

    String _broker;
    uint16_t _port;
    String _clientId;
    String _login;
    String _password;

    std::function<void(String, String)> _messageCallback;

    static void _internalCallback(char* topic, byte* payload, unsigned int length);
    static MQTTManager* _instance;
};

#endif // MQTT_MANAGER_H
