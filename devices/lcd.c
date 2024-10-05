#include "../dma.h"
#include "lcd.h"

static const char *name = "128x64 LCD device";
static const h8_device_id type = H8_DEVICE_LCD;

void h8_lcd_init(h8_device_t *device)
{
  if (device)
  {
    device->name = name;
    device->type = type;
  }
}
