#ifndef TOUCHBUTTON_H
#define TOUCHBUTTON_H

#include <stdio.h>
#include <string>
#include <functional>
#include "config.h"

struct ColorCombo {
  uint16_t text;
  uint16_t bg;
};

class TouchButton {
public:
  enum PressedState {
    NOT_PRESSED = 0,
    PRESSED,
    WAIT_RELEASED
  };

  enum Color {
    BUTTON_NORMAL = 0,
    BUTTON_ACTIVE = 1,
    BUTTON_RED = 2,
    BUTTON_HIGHLIGHTED = 3,
    BUTTON_LABEL = 4,
    BUTTON_BLUE = 5,
    BUTTON_PINK = 6,
    BUTTON_INACTIVE = 7,
    BUTTONCOLOR_NUM = 8
  };

  template<int nx, int ny>
  struct Grid {
    constexpr Grid() : X(), Y() {
      for (auto i = 0; i < nx; ++i) {
          X[i] = i * (TouchButton::BUTTON_SIZE_X + TouchButton::BUTTON_SPACING);
      }
      for (auto i = 0; i < ny; ++i) {
          const uint16_t offset = i > 1 ? 10 : 0;
          Y[i] = offset + i * (TouchButton::BUTTON_SIZE_Y + TouchButton::BUTTON_SPACING);
      }
    }
    int X[nx];
    int Y[ny];
  };

  static constexpr uint16_t BUTTON_SIZE_X = 50;
  static constexpr uint16_t BUTTON_SIZE_Y = 35;
  static constexpr uint8_t BUTTON_SPACING = 4;  // center in screen

  TouchButton(uint16_t x_coord, uint16_t y_coord, std::function<void(TouchButton*)> draw, std::function<void(TouchButton *b)> clicked = [](TouchButton *b){}, std::function<void(TouchButton *b)> longPressed = 0);
  void processPressed();
  void drawNow();
  void draw(const std::string label, const std::string sub, Color Color);
  void setSelected(bool selected);
  void clear(uint16_t color);

  static void drawVirtualKeyboardButton(uint16_t x, uint16_t y);
  static void drawButton(uint16_t x, uint16_t y, const std::string label, const std::string sub, Color color);
  static bool isPressed(uint16_t x, uint16_t y);
  static bool isAreaPressed(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  static bool isInArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  static ColorCombo getColors(Color color);
  static void clearButton(uint16_t x, uint16_t y, uint16_t color);

private:
  bool isSelected = false;
  uint16_t x;
  uint16_t y;
  std::function<void(TouchButton*)> drawHandler{};
  std::function<void(TouchButton*)> clickedHandler{};
  std::function<void(TouchButton*)> longPressedHandler{};
  uint16_t pressedMs;
  PressedState pressedState;

  static constexpr uint16_t LONGPRESS_TIME_MS = 600;

};

constexpr TouchButton::Grid<6, 6> GRID;

#endif //TOUCHBUTTON_H