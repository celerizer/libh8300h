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
h8_bool h8_factory_control_test_in(h8_device_t *device)
{
  H8_UNUSED(device);
  return H8_FACTORY_CONTROL_NO_TEST;
}

/** @todo "Set for sum of eight A/D conversions" */
void h8_factory_control_unknown_out(h8_device_t *device, const h8_bool on)
{
  H8_UNUSED(device);
  H8_UNUSED(on);
  return;
}
