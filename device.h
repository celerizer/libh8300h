#ifndef H8_DEVICE_H
#define H8_DEVICE_H

#include "types.h"

/**
 * An enumeration of unique identifiers for any plugged-in devices that can be
 * emulated
 */
typedef enum
{
  H8_DEVICE_INVALID = 0,

  /* 8KB generic EEPROM device */
  H8_DEVICE_EEPROM_8K,

  /* 64KB generic EEPROM device */
  H8_DEVICE_EEPROM_64K,

  /* Bosch Sensortec - Digital, triaxial acceleration sensor */
  H8_DEVICE_BMA150,

  /* A 4-color 128x64 LCD */
  H8_DEVICE_LCD,

  /* An LED that can light either red or green */
  H8_DEVICE_LED,

  /* The SPI bus connector on the NTR-031 cartridge */
  H8_DEVICE_SPI_BUS,

  /* A simple sound output device */
  H8_DEVICE_BUZZER,

  /* A single-button input device */
  H8_DEVICE_1BUTTON,

  /* A three-button input device */
  H8_DEVICE_3BUTTON,

  /* A factory test controller that can tell the device to check itself */
  H8_DEVICE_FACTORY_CONTROL,

  /* A controller to setup IrDA */
  H8_DEVICE_IRDA_CONTROL,

  H8_DEVICE_SIZE
} h8_device_id;

/**
 * An enumeration of unique identifiers for preset systems that can be
 * configured to contain all necessary devices
 */
typedef enum
{
  H8_SYSTEM_INVALID = 0,

  H8_SYSTEM_NTR_027,
  H8_SYSTEM_NTR_031,
  H8_SYSTEM_NTR_032,

  H8_SYSTEM_SIZE
} h8_system_id;

/**
 * An enumeration of IO ports devices can be hooked up to
 */
typedef enum
{
  H8_HOOKUP_PORT_INVALID = 0,

  H8_HOOKUP_PORT_1,
  H8_HOOKUP_PORT_3,
  H8_HOOKUP_PORT_8,
  H8_HOOKUP_PORT_9,
  H8_HOOKUP_PORT_B,

  H8_HOOKUP_PORT_SIZE
} h8_hookup_port;

/**
 * An enumeration of pins that can be pulled down to perform a chip select on a
 * specific port
 */
typedef enum
{
  H8_HOOKUP_SELECT_INVALID = 0,

  H8_HOOKUP_SELECT_0 = 1,
  H8_HOOKUP_SELECT_1 = 2,
  H8_HOOKUP_SELECT_2 = 4,
  H8_HOOKUP_SELECT_3 = 8,

  H8_HOOKUP_SELECT_SIZE
} h8_hookup_select;

struct h8_device_t;

typedef void H8D_OP_IN_T(struct h8_device_t*, h8_byte_t*);
typedef void H8D_OP_OUT_T(struct h8_device_t*, h8_byte_t*, h8_byte_t);
typedef void H8D_OP_INIT_T(struct h8_device_t*);
typedef void H8D_OP_FREE_T(struct h8_device_t*);
typedef h8_bool H8D_OP_SAVE_T(const struct h8_device_t*, h8_u8**, unsigned*);
typedef h8_bool H8D_OP_LOAD_T(struct h8_device_t*, const h8_u8**, unsigned*);

typedef struct h8_device_t
{
  void *device;

  h8_device_id type;
  const char *name;
  h8_hookup_port port;
  h8_hookup_select select;

  /** A pointer to general device data without any additional state */
  void *data;

  /** The size, in bytes, of what `data` points to */
  unsigned size;

  H8D_OP_INIT_T *init;

  /**
   * A function to be caled when the CPU reads from either the connected port
   * directly, or the SSU when this device is selected
   */
  H8D_OP_IN_T *read;

  /**
   * A function to be caled when the CPU reads from either the connected port
   * directly, or the SSU when this device is selected
   */
  H8D_OP_OUT_T *write;

  /**
   * A function to be called to serialize the device state
   */
  H8D_OP_SAVE_T *save;

  /**
   * A function to be called to load the device state from serialized data
   */
  H8D_OP_LOAD_T *load;
} h8_device_t;

/** An arbitrary maximum for how many ROMs are associated with a system */
#define H8_CRC32_MAX 4

/** An arbitrary maximum for how many port associations can be created */
#define H8_HOOKUP_MAX 32

typedef struct h8_software_hookup_t
{
  /* An identifier differentiating hooked up devices from each other, so
   * multiple hookups can be made for one allocated device. */
  unsigned id;

  /* An identifier specifying which of the implemented devices this is */
  h8_device_id type;

  /* The port this hookup takes place in */
  h8_hookup_port port;

  /* The select pin pulled when this device is being accessed on this port */
  h8_hookup_select select;

  /* A pointer to the function called when data is loaded from the port.
   * If NULL, a default implementation will be used. */
  H8D_OP_IN_T *func_load;

  /* A pointer to the function called when data is stored to the port.
   * If NULL, a default implementation will be used. */
  H8D_OP_OUT_T *func_store;
} software_hookup_t;

typedef struct h8_system_preset_t
{
  const char *title;
  h8_system_id system;
  unsigned crc32[H8_CRC32_MAX];
  software_hookup_t hookups[H8_HOOKUP_MAX];
} h8_system_preset_t;

#endif
