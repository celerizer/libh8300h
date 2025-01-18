#include "accelerometer.h"

#include <stdlib.h> /** @todo remove */

static const char *name = "Analog accelerometer";
static const h8_device_id type = H8_DEVICE_ACCELEROMETER;

h8_word_t h8_accelerometer_adc_in(h8_device_t *device)
{
  h8_word_t result;

  H8_UNUSED(device);
  result.u = rand() % 32; /** @todo */

  return result;
}

void h8_accelerometer_init(h8_device_t *device)
{
  if (device)
  {
    device->name = name;
    device->type = type;
    device->adc_in = h8_accelerometer_adc_in;
  }
}
