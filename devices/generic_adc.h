#ifndef H8_GENERIC_ADC_H
#define H8_GENERIC_ADC_H

#include "../device.h"

void h8_generic_adc_init(h8_device_t *device);

void h8_generic_adrr_set(h8_device_t *device, h8_word_t value);

h8_word_t h8_generic_adrr_fuzz(h8_device_t *device);

h8_word_t h8_generic_adrr_get(h8_device_t *device);

h8_word_t h8_generic_adrr_half(h8_device_t *device);

h8_word_t h8_generic_adrr_max(h8_device_t *device);

h8_word_t h8_generic_adrr_zero(h8_device_t *device);

#endif
