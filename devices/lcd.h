#ifndef H8_LCD_H
#define H8_LCD_H

#include "../device.h"

void h8_lcd_init(h8_device_t *device);

void h8_lcd_select_out(h8_device_t *device, const h8_bool on);

void h8_lcd_mode_out(h8_device_t *device, const h8_bool on);

#endif
