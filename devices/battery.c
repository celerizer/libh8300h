#include "battery.h"

static const char *name = "CR2032 battery";
static const h8_device_id type = H8_DEVICE_BATTERY;

h8_word_t h8_battery_adrr(h8_device_t *device)
{
  h8_word_t result;

  H8_UNUSED(device);
  result.u = 256; /** @todo Configurable battery level */

  return result;
}

void h8_battery_init(h8_device_t *device)
{
  if (device)
  {
    device->name = name;
    device->type = type;
  }
}
