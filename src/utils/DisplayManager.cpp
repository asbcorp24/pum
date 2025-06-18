// DisplayManager.cpp
#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t csPin, uint8_t dcPin, int8_t rstPin)
  : tft(Adafruit_ILI9341(csPin, dcPin, rstPin)) {}

void DisplayManager::begin() {
  // Инициируем SPI (по умолчанию SPI на ESP32 pins: SCK=18, MOSI=23, MISO=19)
  SPI.begin();
  tft.begin();                   // Инициализация контроллера ILI9341
  tft.setRotation(1);            // Ориентация экрана (0–3)
  tft.fillScreen(bgColor);
  tft.setTextWrap(true);         // Автоброк
  tft.setTextColor(fgColor, bgColor);
  tft.setTextSize(2);
}

void DisplayManager::showStartupScreen(const char* text) {
  tft.fillScreen(bgColor);
  tft.setCursor(10, tft.height()/2 - 10);
  tft.setTextSize(3);
  tft.print(text);
  tft.setTextSize(2);
}

void DisplayManager::showClientStatus(uint16_t clientCount, const String& lastCowId, float lastVolume) {
  tft.fillScreen(bgColor);
  tft.setCursor(0, 0);
  tft.print("Pending: ");
  tft.println(clientCount);
  tft.print("Last ID: ");
  tft.println(lastCowId);
  tft.print("Volume: ");
  tft.print(lastVolume, 2);
  tft.println(" L");
}

void DisplayManager::showMessage(const String& msg) {
  tft.fillScreen(bgColor);
  tft.setCursor(0, 0);
  tft.println(msg);
}

void DisplayManager::update() {
  // У Adafruit_ILI9341 нет двойного буфера — ничего не нужно делать,
  // но оставляем для совместимости с LVGL-циклом
}

 void DisplayManager::showWiFiStatus(const String& ssid, const String& ip, bool connected) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.print("WiFi: ");
  tft.print(ssid);
  tft.setCursor(0, 20);
  tft.print("IP: ");
  tft.print(ip);
  tft.setCursor(0, 40);
  tft.print("Mode: ");
  tft.print(connected ? "Station" : "AP");
}
