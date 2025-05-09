#include "accelerometer.h"

#include "generic_adc.h"

static const char *name_x = "Analog accelerometer X";
static const char *name_y = "Analog accelerometer Y";

void h8_accelerometer_init_x(h8_device_t *device)
{
  if (device)
  {
    h8_generic_adc_init(device);
    device->name = name_x;
    device->type = H8_DEVICE_ACCELEROMETER_X;
  }
}

void h8_accelerometer_init_y(h8_device_t *device)
{
  if (device)
  {
    h8_generic_adc_init(device);
    device->name = name_y;
    device->type = H8_DEVICE_ACCELEROMETER_Y;
  }
}
