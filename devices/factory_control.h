#ifndef H8_FACTORY_CONTROL_H
#define H8_FACTORY_CONTROL_H

#include "../device.h"

void h8_factory_control_init(h8_device_t *device);

h8_bool h8_factory_control_test_in(h8_device_t *device);

void h8_factory_control_unknown_out(h8_device_t *device, const h8_bool on);

#endif
