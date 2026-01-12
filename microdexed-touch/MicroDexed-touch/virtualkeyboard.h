#ifndef _VIRTUALKEYBOARD_H
#define _VIRTUALKEYBOARD_H

#include <stdint.h>

enum VirtualKeyboardFlags : uint8_t {
    VK_DRAW_INSTRUMENT_BUTTONS = 1 << 1,
    VK_DRAW_OCTAVE_BUTTONS = 1 << 2,
    VK_DRAW_KEYS = 1 << 3,
    VK_DRAW_OCTAVE = 1 << 4,
    VK_DRAW_INSTRUMENT_NAME = 1 << 5,
    VK_DRAW_ALL = 0xFF
};

void virtual_keyboard_print_velocity_bar();
void drawVirtualKeyboard(uint8_t drawFlags = VK_DRAW_ALL);
void handleTouchVirtualKeyboard();
void virtual_keyboard_smart_preselect_mode();

#endif //_VIRTUALKEYBOARD_H