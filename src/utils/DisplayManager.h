#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

// forward declaration (если ты используешь LVGL или другой дисплейный стек)
class TFT_eSPI;

class DisplayManager {
public:
    DisplayManager();

    void begin(); // инициализация дисплея и LVGL

    void showStartupScreen(const String& mode);
    void showWiFiStatus(const String& ssid, const String& ip, bool connected);
    void showClientStatus(uint8_t clientCount, const String& lastCowId, float lastVolume);
    void showSensorData(float volume, float flowRate, float ec);
    void showMessage(const String& msg, uint16_t color = 0xFFFF); // произвольное сообщение

    void update(); // для вызова lv_timer_handler() или дисплейной отрисовки
};

#endif // DISPLAY_MANAGER_H
