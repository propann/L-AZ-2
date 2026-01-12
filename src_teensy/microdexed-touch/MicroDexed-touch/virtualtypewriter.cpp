#include "virtualtypewriter.h"
#include "ILI9341_t3n.h"
#include "touch.h"

extern ts_t ts;
extern ILI9341_t3n display;
extern int numTouchPoints;

// Keyboard layout
static const char *keyboard_rows[] = {
    "1234567890",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM",     // 7 letters
    "^_O"          // Special keys: Shift, Space, OK
};

// Optimized key size constants
static constexpr uint8_t VK_KEY_W = 28;
static constexpr uint8_t VK_KEY_H = 28;
static constexpr uint8_t KEY_SPACING = 2;
static constexpr uint8_t KEY_START_X = CHAR_width;
static constexpr uint8_t KEY_START_Y = 65;

// Special key sizes
static constexpr uint8_t VK_SHIFT_W = 60;
static constexpr uint8_t VK_SPACE_W = 76;
static constexpr uint8_t VK_OK_W = 60;
static constexpr uint8_t VK_DEL_W = 60;  // Reduced width for better proportions

extern char edit_string_global[FILENAME_LEN];
extern uint8_t edit_len_global;
extern uint8_t edit_pos_global;
extern bool edit_mode_global;

static bool shift_active = false;

uint8_t vtw_x;

// Case conversion helper
FLASHMEM char get_display_char(char c) {
    if (isalpha(c)) {
        return shift_active ? c : tolower(c);
    }
    return c;
}

FLASHMEM void virtual_typewriter_init(int8_t pos) {
    uint8_t current_len = strlen(edit_string_global);
    shift_active = false;
    
    // Pad with spaces only if necessary
    for (uint8_t i = current_len; i < edit_len_global; i++) {
        edit_string_global[i] = ' ';
    }
    edit_string_global[edit_len_global] = '\0';
    
    // Set cursor position
    edit_pos_global = (pos >= 0 && pos < edit_len_global) ? pos : 0;
    
    // Draw keyboard and update display
    draw_keyboard();
    virtual_typewriter_update_display();
}

FLASHMEM void draw_key(uint16_t px, uint8_t py, const char *label, bool pressed, uint8_t key_w = VK_KEY_W) {
      display.console = true;
    display.fillRect(px, py, key_w, VK_KEY_H, pressed ? RED : DX_DARKCYAN);
    uint16_t textX = px + (key_w - strlen(label)*CHAR_width)/2;
    display.setCursor(textX > px ? textX : px+2, py + 8);
    display.setTextColor(COLOR_SYSTEXT,DX_DARKCYAN);
    display.print(label);
}

// Optimized keyboard drawing with proper case handling
FLASHMEM void draw_keyboard() {
    uint8_t current_y = KEY_START_Y;
    
    // First 3 rows
    for (uint8_t r = 0; r < 3; r++) {
        uint16_t x = KEY_START_X + (r == 2 ? VK_KEY_W/2 : 0);
        const char* row = keyboard_rows[r];
        while (*row) {
            char c = get_display_char(*row);
            char label[2] = {c, 0};
            draw_key(x, current_y, label, false);
            x += VK_KEY_W + KEY_SPACING;
            row++;
        }
        current_y += VK_KEY_H + KEY_SPACING;
    }
    
    // Row 3 with arrow keys
    const char* row3 = keyboard_rows[3];
    uint16_t row3_width = 9 * (VK_KEY_W + KEY_SPACING); // 7 letters + 2 arrows
    uint16_t x3 = (320 - row3_width) / 2;
    current_y = KEY_START_Y + 3 * (VK_KEY_H + KEY_SPACING);
    
    // Draw letters in row 3
    for (uint8_t c = 0; c < 7; c++) {
        char ch = get_display_char(*row3);
        char label[2] = {ch, 0};
        draw_key(x3, current_y, label, false);
        x3 += VK_KEY_W + KEY_SPACING;
        row3++;
    }
    
    // Draw LEFT arrow key
    draw_key(x3, current_y, "<", false);
    x3 += VK_KEY_W + KEY_SPACING;
    
    // Draw RIGHT arrow key
    draw_key(x3, current_y, ">", false);
    current_y += VK_KEY_H + KEY_SPACING;
    
    // Bottom row
    const uint16_t bottom_row_width = VK_SHIFT_W + KEY_SPACING + VK_SPACE_W + KEY_SPACING + VK_OK_W;
    uint16_t x4 = (320 - bottom_row_width) / 2;
    
    draw_key(x4, current_y, shift_active ? "SHIFT" : "shift", false, VK_SHIFT_W);
    x4 += VK_SHIFT_W + KEY_SPACING;
    draw_key(x4, current_y, "SPACE", false, VK_SPACE_W);
    x4 += VK_SPACE_W + KEY_SPACING;
    draw_key(x4, current_y, "OK", false, VK_OK_W);
}

// return pressed key via out_key
FLASHMEM bool virtual_typewriter_touchloop(char* out_key) {
    if (numTouchPoints == 0) return false;

    uint8_t y = KEY_START_Y;
    
    // First 3 rows
    for (uint8_t r = 0; r < 3; r++) {
        uint16_t x = KEY_START_X + (r == 2 ? VK_KEY_W/2 : 0);
        const char* row = keyboard_rows[r];
        while (*row) {
            if (ts.p.x >= x && ts.p.x < x + VK_KEY_W && 
                ts.p.y >= y && ts.p.y < y + VK_KEY_H) {
                
                char c = *row;
                char displayChar = get_display_char(c);
                *out_key = displayChar;
                
                // Visual feedback - use displayChar for correct case
                char label[2] = {displayChar, 0};
                draw_key(x, y, label, true);
                delay(100);
                draw_key(x, y, label, false);
                
                // Turn off shift after any keypress
                if (shift_active) {
                    shift_active = false;
                    draw_keyboard();
                    delay(50); // Ensure keyboard redraw completes before next input
                }
                
                return true;
            }
            x += VK_KEY_W + KEY_SPACING;
            row++;
        }
        y += VK_KEY_H + KEY_SPACING;
    }
    
    // Row 3 with arrow keys
    const char* row3 = keyboard_rows[3];
    uint16_t row3_width = 9 * (VK_KEY_W + KEY_SPACING); // 7 letters + 2 arrows
    uint16_t x3 = (320 - row3_width) / 2;
    y = KEY_START_Y + 3 * (VK_KEY_H + KEY_SPACING);
    
    // Handle letters in row 3
    for (uint8_t c = 0; c < 7; c++) {
        if (ts.p.x >= x3 && ts.p.x < x3 + VK_KEY_W && 
            ts.p.y >= y && ts.p.y < y + VK_KEY_H) {
            
            char ch = *row3;
            char displayChar = get_display_char(ch);
            *out_key = displayChar;
            
            // Visual feedback - use displayChar for correct case
            char label[2] = {displayChar, 0};
            draw_key(x3, y, label, true);
            delay(100);
            draw_key(x3, y, label, false);
            
            // Turn off shift after any keypress
            if (shift_active) {
                shift_active = false;
                draw_keyboard();
                delay(50); // Ensure keyboard redraw completes before next input
            }
            
            return true;
        }
        x3 += VK_KEY_W + KEY_SPACING;
        row3++;
    }
    
    // LEFT arrow key
    if (ts.p.x >= x3 && ts.p.x < x3 + VK_KEY_W && 
        ts.p.y >= y && ts.p.y < y + VK_KEY_H) {
        *out_key = VK_LEFT;
        draw_key(x3, y, "<", true);
        delay(100);
        draw_key(x3, y, "<", false);
        return true;
    }
    x3 += VK_KEY_W + KEY_SPACING;
    
    // RIGHT arrow key
    if (ts.p.x >= x3 && ts.p.x < x3 + VK_KEY_W && 
        ts.p.y >= y && ts.p.y < y + VK_KEY_H) {
        *out_key = VK_RIGHT;
        draw_key(x3, y, ">", true);
        delay(100);
        draw_key(x3, y, ">", false);
        return true;
    }
    
    // Bottom row
    y = KEY_START_Y + 4 * (VK_KEY_H + KEY_SPACING);
    const uint16_t bottom_row_width = VK_SHIFT_W + KEY_SPACING + VK_SPACE_W + KEY_SPACING + VK_OK_W;
    uint16_t x4 = (320 - bottom_row_width) / 2;
    
    // Shift key
    if (ts.p.x >= x4 && ts.p.x < x4 + VK_SHIFT_W && 
        ts.p.y >= y && ts.p.y < y + VK_KEY_H) {
        shift_active = !shift_active;
        draw_keyboard();
        delay(200);
        return false;
    }
    
    // Space key
    x4 += VK_SHIFT_W + KEY_SPACING;
    if (ts.p.x >= x4 && ts.p.x < x4 + VK_SPACE_W && 
        ts.p.y >= y && ts.p.y < y + VK_KEY_H) {
        *out_key = ' ';
        draw_key(x4, y, "SPACE", true, VK_SPACE_W);
        delay(100);
        draw_key(x4, y, "SPACE", false, VK_SPACE_W);
        
        // Turn off shift after any keypress
        if (shift_active) {
            shift_active = false;
            draw_keyboard();
            delay(50); // Ensure keyboard redraw completes before next input
        }
        
        return true;
    }
    
    // OK key
    x4 += VK_SPACE_W + KEY_SPACING;
    if (ts.p.x >= x4 && ts.p.x < x4 + VK_OK_W && 
        ts.p.y >= y && ts.p.y < y + VK_KEY_H) {
        *out_key = VK_ENTER;
        draw_key(x4, y, "OK", true, VK_OK_W);
        delay(100);
        draw_key(x4, y, "OK", false, VK_OK_W);
        
        // Turn off shift after any keypress
        if (shift_active) {
            shift_active = false;
            draw_keyboard();
            delay(50); // Ensure keyboard redraw completes before next input
        }
        
        return true;
    }

    return false;
}

FLASHMEM void virtual_typewriter_update_display() {
    // Draw the entire string at fixed position
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(vtw_x * CHAR_width, 2 * CHAR_height);
    
    // NEW: Print exactly edit_len_global characters
    for (uint8_t i = 0; i < edit_len_global; i++) {
        display.print(edit_string_global[i]);
    }
    
    // Draw black cursors at -1 and +1 positions
    for (int offset = -1; offset <= 1; offset += 2) {
        int8_t pos = edit_pos_global + offset;
        
        if (pos >= 0 && pos < edit_len_global) {
             display.console = true;
            display.fillRect(
                vtw_x * CHAR_width + (pos * CHAR_width),
                3 * CHAR_height + 1,
                CHAR_width, 2, COLOR_BACKGROUND
            );
        }
    }

    // Draw cursor at current position
    if (edit_pos_global >= 0 && edit_pos_global < edit_len_global) {
        uint16_t cursor_color = edit_mode_global ? RED : COLOR_PITCHSMP;
         display.console = true;
        display.fillRect(
            vtw_x * CHAR_width + (edit_pos_global * CHAR_width),
            3 * CHAR_height + 1,
            CHAR_width, 2, cursor_color
        );
    }
}