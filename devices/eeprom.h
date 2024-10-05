#ifndef H8_EEPROM_H
#define H8_EEPROM_H

#include "../device.h"

void h8_eeprom_init_8k(h8_device_t *device);

void h8_eeprom_init_64k(h8_device_t *device);

void h8_eeprom_read(h8_device_t *device, h8_byte_t *dst);

void h8_eeprom_write(h8_device_t *device, h8_byte_t *dst,
                     const h8_byte_t value);

#endif
