#include "DisplayManager.h"

// Задайте пины в соответствии с вашей платой!
static constexpr int PIN_CS   = 5;
static constexpr int PIN_DC   = 16;
static constexpr int PIN_RST  = 4;
static constexpr int PIN_BL   = 15;
static constexpr int PIN_MOSI = 19;
static constexpr int PIN_CLK  = 18;
static constexpr int PIN_MISO = -1;  // не используем

void DisplayManager::begin() {
  // Инициализируем дисплей (пример для ST7789 240×135):
  //   spilcdInit(lcd, тип, флаги, частота, CS, DC, MISO, RST, BL, MOSI, CLK)
  spilcdInit(&lcd,
              LCD_ST7789_135,
              FLAGS_NONE,
              40000000,
              PIN_CS, PIN_DC, PIN_MISO,
              PIN_RST, PIN_BL,
              PIN_MOSI, PIN_CLK);

  // Поворачиваем на 90°
  spilcdSetOrientation(&lcd, LCD_ORIENTATION_90);

  // Выделяем back-buffer (если позволяет память)
#ifndef __AVR__
  spilcdAllocBackbuffer(&lcd);
#endif

  // Заливаем всё чёрным
  spilcdFill(&lcd, 0, DRAW_TO_LCD);
}

void DisplayManager::showStartupScreen(const String &mode) {
  // Чистим экран
  spilcdFill(&lcd, 0, DRAW_TO_LCD);

  // Выводим текст
  char buf[32];
  mode.toCharArray(buf, sizeof(buf));
  spilcdWriteString(&lcd, 20,  60, "Starting:",   0xFFFF, 0x0000, FONT_8x8, DRAW_TO_LCD);
  spilcdWriteString(&lcd, 20,  80, buf,          0x07E0, 0x0000, FONT_8x8, DRAW_TO_LCD);
}

void DisplayManager::showWiFiStatus(const String &ssid, const String &ip, bool connected) {
  // Рисуем фон вверху
  uint16_t bg = connected ? 0x001F : 0xF800;  // синий / красный
  spilcdRectangle(&lcd, 0, 0, 240, 20, bg, bg, 1, DRAW_TO_LCD);

  // Пишем SSID
  char buf[32];
  ssid.toCharArray(buf, sizeof(buf));
  spilcdWriteString(&lcd, 5, 2, connected ? "WiFi:" : "AP:", 0xFFFF, bg, FONT_8x8, DRAW_TO_LCD);
  spilcdWriteString(&lcd, 40, 2, buf, 0xFFFF, bg, FONT_8x8, DRAW_TO_LCD);

  // Пишем IP ниже
  ip.toCharArray(buf, sizeof(buf));
  spilcdWriteString(&lcd, 5, 12, buf, 0xFFFF, bg, FONT_8x8, DRAW_TO_LCD);
}

void DisplayManager::showMessage(const String &msg) {
  // Очищаем середину экрана
  spilcdFill(&lcd, 0, DRAW_TO_LCD);

  // Показываем строку
  char buf[64];
  msg.toCharArray(buf, sizeof(buf));
  spilcdWriteString(&lcd, 5, 60, buf, 0xFFFF, 0x0000, FONT_8x8, DRAW_TO_LCD);
}

void DisplayManager::showClientStatus(uint32_t clientId, const String &cow, float volume) {
  // Чистим экран
  spilcdFill(&lcd, 0, DRAW_TO_LCD);

  char buf[32];
  snprintf(buf, sizeof(buf), "Client ID: %u", clientId);
  spilcdWriteString(&lcd,  5,  5, buf, 0x07E0, 0x0000, FONT_8x8, DRAW_TO_LCD);

  cow.toCharArray(buf, sizeof(buf));
  snprintf(buf + cow.length(), sizeof(buf) - cow.length(), "Cow: %s", buf);
  spilcdWriteString(&lcd,  5, 20, buf, 0x07E0, 0x0000, FONT_8x8, DRAW_TO_LCD);

  snprintf(buf, sizeof(buf), "Vol: %.2f L", volume);
  spilcdWriteString(&lcd,  5, 35, buf, 0x07E0, 0x0000, FONT_8x8, DRAW_TO_LCD);
}

void DisplayManager::update() {
  // если используете backbuffer, то тут нужно показать буфер:
#ifndef __AVR__
  spilcdShowBuffer(&lcd, 0, 0, 240, 135, DRAW_TO_LCD);
#endif
}
