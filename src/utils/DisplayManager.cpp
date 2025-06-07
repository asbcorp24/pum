#include "DisplayManager.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();  // глобальный объект дисплея

DisplayManager::DisplayManager() {}

void DisplayManager::begin() {
    tft.init();
    tft.setRotation(1); // Повернуть при необходимости
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Display Init...", 10, 10);
    delay(500);
}

void DisplayManager::showStartupScreen(const String& mode) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Milk Tracker", 10, 10);
    tft.drawString("Mode:", 10, 40);
    tft.drawString(mode, 80, 40);
}

void DisplayManager::showWiFiStatus(const String& ssid, const String& ip, bool connected) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Wi-Fi Status", 10, 10);
    tft.drawString("SSID: " + ssid, 10, 40);
    tft.drawString("IP: " + ip, 10, 70);
    tft.drawString("State: " + String(connected ? "Connected" : "Disconnected"), 10, 100);
}

void DisplayManager::showClientStatus(uint8_t clientCount, const String& lastCowId, float lastVolume) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Clients: " + String(clientCount), 10, 10);
    tft.drawString("Last Cow ID: " + lastCowId, 10, 40);
    tft.drawString("Volume: " + String(lastVolume, 2) + " L", 10, 70);
}

void DisplayManager::showSensorData(float volume, float flowRate, float ec) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Milk Volume: " + String(volume, 2) + " L", 10, 10);
    tft.drawString("Flow Rate: " + String(flowRate, 2) + " L/s", 10, 40);
    tft.drawString("EC: " + String(ec, 2), 10, 70);
}

void DisplayManager::showMessage(const String& msg, uint16_t color) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(msg, 10, 10);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // вернуть обратно
}

void DisplayManager::update() {
    // Здесь можно вызывать lv_timer_handler() при использовании LVGL
    // lv_timer_handler(); delay(5);
}
