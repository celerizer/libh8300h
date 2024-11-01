#include "generic.h"

static const char *name = "Generic device";
static const unsigned type = H8_DEVICE_GENERIC;

void h8_generic_init(h8_device_t *device)
{
  if (device)
  {
    device->name = name;
    device->type = type;
  }
}

h8_bool h8_generic_in_0(h8_device_t *device)
{
  H8_UNUSED(device);
  return FALSE;
}

h8_bool h8_generic_in_1(h8_device_t *device)
{
  H8_UNUSED(device);
  return TRUE;
}
