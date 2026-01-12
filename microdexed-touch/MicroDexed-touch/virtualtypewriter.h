// virtualtypewriter.h - Simplified init declaration
#ifndef VIRTUALTYPEWRITER_H
#define VIRTUALTYPEWRITER_H

#include <Arduino.h>

#define VK_RIGHT 0x02
#define VK_ENTER '\n'
#define VK_LEFT  0x01

void draw_keyboard();
bool virtual_typewriter_touchloop(char* out_key);
void virtual_typewriter_init( int8_t pos);  
void virtual_typewriter_update_display();
int8_t virtual_typewriter_get_cursor_pos();
bool virtual_typewriter_take_ok();

#endif