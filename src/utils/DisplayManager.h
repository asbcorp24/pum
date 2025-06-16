#pragma once

#include <Arduino.h>
#include <bb_spi_lcd.h>

class DisplayManager {
public:
  // Инициализация дисплея
  void begin();

  // Заставка при старте (Client/Server)
  void showStartupScreen(const String &mode);

  // Статус Wi-Fi: SSID, IP, флаг соединения (true = STA, false = AP)
  void showWiFiStatus(const String &ssid, const String &ip, bool connected);

  // Произвольное сообщение (например: RS485-пакет)
  void showMessage(const String &msg);

  // Статус Client Mode: ID, последний cow ID, объём
  void showClientStatus(uint32_t clientId, const String &cow, float volume);

  // Нужен, чтобы в цикле вызывать обновление буфера (если используете backbuffer)
  void update();

private:
  SPILCD lcd;
};
