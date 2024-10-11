#include "factory_control.h"

static const char *name = "Factory control";
static const unsigned type = H8_DEVICE_FACTORY_CONTROL;

#define H8_FACTORY_CONTROL_TEST 0
#define H8_FACTORY_CONTROL_NO_TEST 1

void h8_factory_control_init(h8_device_t *device)
{
  if (device)
  {
    device->name = name;
    device->type = type;
  }
}

/** @todo Make the test option configurable */
void h8_factory_control_read(h8_device_t *device, h8_byte_t *dst)
{
  H8_UNUSED(device);
  dst->u = H8_FACTORY_CONTROL_NO_TEST;
}

/** @todo "Set for sum of eight A/D conversions" */
void h8_factory_control_write(h8_device_t *device, h8_byte_t *dst,
                              h8_byte_t value)
{
  H8_UNUSED(device);
  H8_UNUSED(dst);
  H8_UNUSED(value);
  return;
}
