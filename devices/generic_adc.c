#include "generic.h"

#include "../dma.h"

typedef struct
{
  h8_word_t value;
} h8_generic_adc_t;

void h8_generic_adc_init(h8_device_t *device)
{
  if (device)
    device->device = h8_dma_alloc(sizeof(h8_generic_adc_t), FALSE);
}

void h8_generic_adrr_set(h8_device_t *device, h8_word_t value)
{
#if H8_SAFETY
  if (!device || !device->device)
    return;
#endif
  ((h8_generic_adc_t*)device->device)->value = value;
}

h8_word_t h8_generic_adrr_fuzz(h8_device_t *device)
{
#if H8_SAFETY
  if (!device || !device->device)
    return h8_generic_adrr_zero(device);
  else
  {
#endif
    h8_generic_adc_t *adc = (h8_generic_adc_t*)device->device;

    adc->value.u = (adc->value.u * 1103515245 + 12345) & 0xFFC0;

    return adc->value;
#if H8_SAFETY
  }
#endif
}

h8_word_t h8_generic_adrr_get(h8_device_t *device)
{
#if H8_SAFETY
  if (!device || !device->device)
    return h8_generic_adrr_zero(device);
#endif
  return ((h8_generic_adc_t*)device->device)->value;
}

h8_word_t h8_generic_adrr_half(h8_device_t *device)
{
  h8_word_t value;

  H8_UNUSED(device);
  value.u = 0x03E0;

  return value;
}

h8_word_t h8_generic_adrr_max(h8_device_t *device)
{
  h8_word_t value;

  H8_UNUSED(device);
  value.u = 0xFFC0;

  return value;
}

h8_word_t h8_generic_adrr_zero(h8_device_t *device)
{
  h8_word_t value;

  H8_UNUSED(device);
  value.u = 0x0000;

  return value;
}
