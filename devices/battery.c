#include "battery.h"

#include "generic_adc.h"

static const char *name = "CR2032 battery";
static const h8_device_id type = H8_DEVICE_BATTERY;

void h8_battery_init(h8_device_t *device)
{
  if (device)
  {
    h8_generic_adc_init(device);
    device->name = name;
    device->type = type;
  }
}
