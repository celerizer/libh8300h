#include "device.h"
#include "devices/bma150.h"
#include "devices/buttons.h"
#include "devices/eeprom.h"
#include "devices/factory_control.h"
#include "devices/lcd.h"
#include "devices/led.h"
#include "system.h"
#include "types.h"

#define DEVICES_END H8_DEVICE_INVALID, 0, { NULL }, { NULL }

static const h8_system_preset_t h8_systems[] =
{
  {
    "NTR-027",
    H8_SYSTEM_NTR_027,
    { 0x82341b9f, 0 },
    {
      {
        H8_DEVICE_FACTORY_CONTROL,
        H8_HOOKUP_PORT_1,
        { h8_factory_control_test_in, NULL, NULL, NULL },
        { NULL, NULL, h8_factory_control_unknown_out, NULL }
      },

      {
        H8_DEVICE_LED,
        H8_HOOKUP_PORT_8,
        { NULL },
        { h8_led_on_out, h8_led_color_out, NULL }
      },

      {
        H8_DEVICE_EEPROM_8K,
        H8_HOOKUP_PORT_9,
        { NULL, NULL },
        { h8_eeprom_select_out, NULL }
      },

      {
        H8_DEVICE_1BUTTON,
        H8_HOOKUP_PORT_B,
        { h8_buttons_in_0, NULL },
        { NULL }
      },

      { DEVICES_END }
    }
  },

  {
    "NTR-031",
    H8_SYSTEM_NTR_031,
    { 0x64b40d8d /* earlier */, 0x9321792f /* later */, 0 },
    {
      {
        H8_DEVICE_SPI_BUS,
        H8_HOOKUP_PORT_8,
        { NULL, /** @todo Savedata chip select */ NULL, NULL },
        { NULL }
      },

      /* Not actually used in normal operation */
      {
        H8_DEVICE_1BUTTON,
        H8_HOOKUP_PORT_B,
        { h8_buttons_in_0, NULL },
        { NULL }
      },

      /**
       * There is also code in the earlier ROM for interfacing with an LED on
       * port 8, bits 2/3, which is unused and conflicts with SPI chip select.
       */

      { DEVICES_END }
    }
  },

  {
    "NTR-032",
    H8_SYSTEM_NTR_032,
    { 0xd4a05446, 0 },
    {
      {
        H8_DEVICE_LCD,
        H8_HOOKUP_PORT_1,
        { NULL },
        { h8_lcd_select_out, h8_lcd_mode_out, NULL }
      },

      {
        H8_DEVICE_EEPROM_64K,
        H8_HOOKUP_PORT_1,
        { NULL },
        { NULL, NULL, h8_eeprom_select_out }
      },

      {
        H8_DEVICE_BMA150,
        H8_HOOKUP_PORT_9,
        { NULL },
        { h8_bma150_select_out, NULL }
      },

      {
        H8_DEVICE_3BUTTON,
        H8_HOOKUP_PORT_B,
        { h8_buttons_in_0, h8_buttons_in_1, h8_buttons_in_2 },
        { NULL }
      },

      { DEVICES_END }
    }
  },
  { NULL, H8_SYSTEM_INVALID, { 0 }, { NULL } }
};

h8_bool h8_device_init(h8_device_t *device, const h8_device_id type)
{
  if (device)
  {
    switch (type)
    {
    case H8_DEVICE_1BUTTON:
      device->init = h8_buttons_init_1b;
      break;
    case H8_DEVICE_3BUTTON:
      device->init = h8_buttons_init_3b;
      break;
    case H8_DEVICE_BMA150:
      device->init = h8_bma150_init;
      break;
    case H8_DEVICE_BUZZER:
      device->init = NULL; /** @todo */
      break;
    case H8_DEVICE_EEPROM_64K:
      device->init = h8_eeprom_init_64k;
      break;
    case H8_DEVICE_EEPROM_8K:
      device->init = h8_eeprom_init_8k;
      break;
    case H8_DEVICE_FACTORY_CONTROL:
      device->init = h8_factory_control_init;
      break;
    case H8_DEVICE_LCD:
      device->init = h8_lcd_init;
      break;
    case H8_DEVICE_LED:
      device->init = h8_led_init;
      break;
    default:
      return 0;
    }
    if (device->init)
      device->init(device);
    else
      device->type = type;

    return 1;
  }

  return 0;
}

h8_bool h8_system_init(h8_system_t *system, const h8_system_id id)
{
  if (system)
  {
    const h8_system_preset_t *preset;
    unsigned i, j = 0;

    switch (id)
    {
    case H8_SYSTEM_NTR_027:
      preset = &h8_systems[0];
      break;
    case H8_SYSTEM_NTR_031:
      preset = &h8_systems[1];
      break;
    case H8_SYSTEM_NTR_032:
      preset = &h8_systems[2];
      break;
    default:
      return FALSE;
    }

    for (i = 0; i < H8_HOOKUP_MAX; i++)
    {
      const software_hookup_t *hookup = &preset->hookups[i];
      h8_device_t *device = &system->devices[j];

      if (preset->hookups[i].type == H8_DEVICE_INVALID)
        break;
      else
      {
        /* Create the device if it does not already exist */
        if (device->type == H8_DEVICE_INVALID)
        {
          h8_device_init(device, hookup->type);
          j++;
        }

        /* Setup IO functions */
        device->port = hookup->port;
      }
    }

    system->device_count = j;
  }

  return FALSE;
}
