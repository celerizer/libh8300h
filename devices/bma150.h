#ifndef H8_BMA150_H
#define H8_BMA150_H

#include "../device.h"

void h8_bma150_init(h8_device_t *device);

void h8_bma150_select_out(h8_device_t *device, h8_bool on);

#endif
