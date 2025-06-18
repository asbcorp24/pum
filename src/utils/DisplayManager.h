// DisplayManager.h
#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

class DisplayManager {
public:
  /** 
   * @param csPin   — Chip Select (TFT_CS)
   * @param dcPin   — Data/Command   (TFT_DC)
   * @param rstPin  — Reset          (TFT_RST), можно передать –1, если не используется
   */
  DisplayManager(uint8_t csPin, uint8_t dcPin, int8_t rstPin);

  /// Инициализация SPI и дисплея
  void begin();

  /// Вывод стартового экрана с текстом
  void showStartupScreen(const char* text);


  /// Показать простое сообщение (например, приёмы RS485)
  void showMessage(const String& msg);
// Для Server Mode
  void showWiFiStatus(const String& ssid, const String& ip, bool connected);
  // Для Client Mode
  void showClientStatus(uint16_t pendingCount, const String& lastCowId, float lastVolume);
  // Просто сообщение
  /// Вызывать в таске для отрисовки (если используется DMA или буфер)
  void update();

private:
  Adafruit_ILI9341 tft;
  uint16_t fgColor = ILI9341_WHITE;
  uint16_t bgColor = ILI9341_BLACK;
};

#endif // DISPLAYMANAGER_H
