#ifndef H8_BUTTONS_H
#define H8_BUTTONS_H

#include "../device.h"

typedef enum
{
  /** The main input button, used on both NTR-027 and NTR-032 */
  H8_BUTTON_MAIN = 0,

  /** A button to navigate left, only on NTR-032 */
  H8_BUTTON_LEFT,

  /** A button to navigate right, only on NTR-032 */
  H8_BUTTON_RIGHT,

  H8_BUTTON_SIZE
} h8_button_id;

typedef struct
{
  h8_bool buttons[H8_BUTTON_SIZE];
  h8_u8 button_count;
} h8_buttons_t;

void h8_buttons_init_1b(h8_device_t *device);

void h8_buttons_init_3b(h8_device_t *device);

void h8_buttons_read(h8_device_t *device, h8_byte_t *byte);

#endif
