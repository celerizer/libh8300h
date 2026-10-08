#ifndef H8_BATTERY_H
#define H8_BATTERY_H

#include "../device.h"

/** The highest reading of the 10-bit A/D converter */
#define H8_BATTERY_LEVEL_MAX 1023

void h8_battery_init(h8_device_t *device);

/** Sets the battery level to a value between 0 and H8_BATTERY_LEVEL_MAX */
void h8_battery_set_level(h8_device_t *device, unsigned level);

/** Returns the battery reading, from 0 to H8_BATTERY_LEVEL_MAX */
unsigned h8_battery_get_level(const h8_device_t *device);

/** A/D converter hookup: the reading, left-aligned as ADRR stores it */
h8_word_t h8_battery_adrr(h8_device_t *device);

#endif
