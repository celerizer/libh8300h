#include "../dma.h"
#include "lcd.h"
#include "../types.h"

static const char *name = "128x64 LCD device";
static const h8_device_id type = H8_DEVICE_LCD;

typedef union
{
  H8_BITFIELD_4
  (
    /** Hardcoded chip ID */
    h8_u8 id : 5,

    /** Whether the device is busy with a reset. Unimplemented */
    h8_u8 res : 1,

    /** Whether the device is on */
    h8_u8 on : 1,

    /** Whether the device is busy with a command. Unimplemented */
    h8_u8 bsy : 1
  ) flags;
  h8_u8 raw;
} h8_lcd_sr_t;

#define H8_LCD_PALETTE_WHITE 0
#define H8_LCD_PALETTE_LIGHT_GRAY 1
#define H8_LCD_PALETTE_DARK_GRAY 2
#define H8_LCD_PALETTE_BLACK 3
#define H8_LCD_PALETTE_SIZE 4

typedef struct
{
  h8_u8 vram[128 * 64];
  h8_bool selected;
  h8_bool mode;
  h8_u8 command;
  h8_bool second_byte;

  h8_u8 x, y;

  /** @todo What is this? */
  h8_bool icon_enable;

  /** If true, displays all pixels on regardless of VRAM contents */
  h8_bool all_on;

  h8_bool inverse_display;

  h8_bool power_save;

  /** If true, the display will sleep when power save is enabled.
   *  If false, the display will standby. */
  h8_bool power_save_mode_sleep;

  h8_bool x_flip, y_flip;

  /** Various unimplemented parameters */
  h8_u8 start_line, display_offset, multiplex_ratio, contrast, nline_inversion;

  h8_u8 palette_modes[H8_LCD_PALETTE_SIZE];

  h8_lcd_sr_t status;
} h8_lcd_t;

#define H8_LCD_MODE_CMD 0
#define H8_LCD_MODE_DATA 1

void h8_lcd_select_pin(h8_device_t *device, const h8_bool on)
{
  h8_lcd_t *m_lcd = device->device;

  m_lcd->selected = !on;
}

void h8_lcd_mode_pin(h8_device_t *device, const h8_bool on)
{
  h8_lcd_t *m_lcd = device->device;

  m_lcd->mode = on;
}

void h8_lcd_read(h8_device_t *device, h8_byte_t *dst)
{
  h8_lcd_t *m_lcd = device->device;

  if (m_lcd->mode)
    dst->u = m_lcd->status.raw;
  else
    /** @todo */
    dst->u = 0;
}

void h8_lcd_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_lcd_t *m_lcd = device->device;

  if (!m_lcd->second_byte)
  {
    m_lcd->command = value.u;

    /**
     * Await second byte for 4X or 8X commands
     * @todo A little more nuanced than this but not for current purposes
     */
    if ((value.u >= 0x40 && value.u <= 0x4F) ||
        (value.u >= 0x80 && value.u <= 0x8F))
      m_lcd->second_byte = TRUE;
    else switch (m_lcd->command)
    {
    /** Set Column Address bit0-3 */
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F:
      m_lcd->x = (m_lcd->x & B01110000) | value.u;
      break;
    /** Set Column Address bit4-6 */
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
      m_lcd->x = (m_lcd->x & B00001111) | (h8_u8)((value.u & B00000111) << 4);
      break;
    case 0xA2:
      m_lcd->icon_enable = FALSE;
      break;
    case 0xA3:
      m_lcd->icon_enable = TRUE;
      break;
    case 0xA4:
      m_lcd->status.flags.on = FALSE;
      break;
    case 0xA5:
      m_lcd->status.flags.on = TRUE;
      break;
    case 0xA6:
      m_lcd->inverse_display = FALSE;
      break;
    case 0xA7:
      m_lcd->inverse_display = TRUE;
      break;
    case 0xA8:
      m_lcd->power_save_mode_sleep = FALSE;
      break;
    case 0xA9:
      m_lcd->power_save_mode_sleep = TRUE;
      break;
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
    case 0xB8:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
      m_lcd->y = (value.u & B00001111) * 8;
      break;
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC3:
    case 0xC4:
    case 0xC5:
    case 0xC6:
    case 0xC7:
      m_lcd->y_flip = FALSE;
      break;
    case 0xC8:
    case 0xC9:
    case 0xCA:
    case 0xCB:
    case 0xCC:
    case 0xCD:
    case 0xCE:
    case 0xCF:
      m_lcd->y_flip = TRUE;
      break;
    case 0xE1:
      m_lcd->power_save = FALSE;
      break;
    /** Software Reset */
    case 0xE2:
      /** @todo set defaults */
      break;
    default:
      break;
    }
  }
  else
  {
    switch (m_lcd->command)
    {
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
      m_lcd->start_line = value.u & B01111111;
      break;
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
      m_lcd->display_offset = value.u & B00111111;
      break;
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
      m_lcd->multiplex_ratio = value.u;
      break;
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
      m_lcd->nline_inversion = value.u;
      break;
    case 0x81:
      m_lcd->contrast = value.u & B00111111;
      break;
    case 0x88:
    case 0x89:
      m_lcd->palette_modes[H8_LCD_PALETTE_WHITE] = value.u & B00001111;
      break;
    case 0x8A:
    case 0x8B:
      m_lcd->palette_modes[H8_LCD_PALETTE_LIGHT_GRAY] = value.u & B00001111;
      break;
    case 0x8C:
    case 0x8D:
      m_lcd->palette_modes[H8_LCD_PALETTE_DARK_GRAY] = value.u & B00001111;
      break;
    case 0x8E:
    case 0x8F:
      m_lcd->palette_modes[H8_LCD_PALETTE_BLACK] = value.u & B00001111;
      break;
    default:
      break;
    }
    m_lcd->second_byte = FALSE;
  }

  *dst = value;
}

void h8_lcd_init(h8_device_t *device)
{
  if (device)
  {
    h8_lcd_t *m_lcd = h8_dma_alloc(sizeof(h8_lcd_t), TRUE);

    m_lcd->status.flags.on = TRUE;
    m_lcd->status.flags.id = 0x08;

    device->device = m_lcd;
    device->read = h8_lcd_read;
    device->write = h8_lcd_write;
    device->name = name;
    device->type = type;
  }
}
