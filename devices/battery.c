#include "battery.h"

#include "../dma.h"

static const char *name = "CR2032 battery";
static const h8_device_id type = H8_DEVICE_BATTERY;

typedef struct
{
  unsigned level;
} h8_battery_t;

void h8_battery_init(h8_device_t *device)
{
  if (device)
  {
    device->device = h8_dma_alloc(sizeof(h8_battery_t), TRUE);
    device->name = name;
    device->type = type;
    if (device->device)
      ((h8_battery_t*)device->device)->level = H8_BATTERY_LEVEL_MAX;
  }
}

void h8_battery_set_level(h8_device_t *device, unsigned level)
{
  if (!device || !device->device)
    return;
  ((h8_battery_t*)device->device)->level =
    level > H8_BATTERY_LEVEL_MAX ? H8_BATTERY_LEVEL_MAX : level;
}

unsigned h8_battery_get_level(const h8_device_t *device)
{
  if (!device || !device->device)
    return 0;
  return ((const h8_battery_t*)device->device)->level;
}

h8_word_t h8_battery_adrr(h8_device_t *device)
{
  h8_word_t value;

  /* 17.3.1: The 10-bit result is stored in the upper bits of ADRR */
  value.u = (h8_u16)(h8_battery_get_level(device) << 6);

  return value;
}
