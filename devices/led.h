#ifndef H8_LED_H
#define H8_LED_H

#include "../device.h"

typedef enum
{
  H8_LED_STATE_INVALID = 0,

  H8_LED_STATE_OFF,
  H8_LED_STATE_RED,
  H8_LED_STATE_GREEN,

  H8_LED_STATE_SIZE
} h8_led_state;

typedef struct
{
  h8_bool on;
  h8_bool green;
  h8_led_state state;
} h8_led_t;

void h8_led_init(h8_device_t *device);

void h8_led_on_out(h8_device_t *device, const h8_bool on);

void h8_led_color_out(h8_device_t *device, const h8_bool on);

#endif
