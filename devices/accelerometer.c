#include "accelerometer.h"

#include "generic_adc.h"

static const char *name_x = "Analog accelerometer X";
static const char *name_y = "Analog accelerometer Y";

static void h8_accelerometer_center(h8_device_t *device)
{
  h8_word_t value;

  /* 512 out of 1023 in 10 bits */
  value.u = 0x8000;
  if (device->device)
    h8_generic_adrr_set(device, value);
}

void h8_accelerometer_init_x(h8_device_t *device)
{
  if (device)
  {
    h8_generic_adc_init(device);
    device->name = name_x;
    device->type = H8_DEVICE_ACCELEROMETER_X;
    h8_accelerometer_center(device);
  }
}

void h8_accelerometer_init_y(h8_device_t *device)
{
  if (device)
  {
    h8_generic_adc_init(device);
    device->name = name_y;
    device->type = H8_DEVICE_ACCELEROMETER_Y;
    h8_accelerometer_center(device);
  }
}
