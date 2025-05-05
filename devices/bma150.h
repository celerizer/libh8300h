#ifndef H8_BMA150_H
#define H8_BMA150_H

#include "../device.h"

void h8_bma150_init(h8_device_t *device);

void h8_bma150_select_out(h8_device_t *device, h8_bool on);

void h8_bma150_set_axis(h8_device_t *device, h8_u16 x, h8_u16 y, h8_u16 z);

#endif
