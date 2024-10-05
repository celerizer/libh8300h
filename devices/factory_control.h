#ifndef H8_FACTORY_CONTROL_H
#define H8_FACTORY_CONTROL_H

#include "../device.h"

void h8_factory_control_init(h8_device_t *device);

void h8_factory_control_read(h8_device_t *device, h8_byte_t *dst);

void h8_factory_control_write(h8_device_t *device, h8_byte_t *dst,
                              h8_byte_t value);

#endif
