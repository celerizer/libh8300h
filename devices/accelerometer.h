#ifndef H8_ACCELEROMETER_H
#define H8_ACCELEROMETER_H

#include "../device.h"

void h8_accelerometer_init(h8_device_t *device);

h8_word_t h8_accelerometer_adrr_x(h8_device_t *device);

h8_word_t h8_accelerometer_adrr_y(h8_device_t *device);

#endif
