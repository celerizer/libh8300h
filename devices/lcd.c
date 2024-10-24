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
  /** Max X by max Y */
  h8_u8 vram[128 * 16 * 2];

  h8_bool selected;
  h8_bool data_mode;
  h8_u8 command;

  h8_bool second_write_cmd, second_write_data, second_read;

  h8_u8 x, y;

  /** @todo What is this? */
  h8_bool icon_enable;

  /** If true, displays all pixels on regardless of VRAM contents */
  h8_bool all_on;

  /** If true, flips the poles of white and black */
  h8_bool inverse_display;

  /** If true, enters the mode determined by power_save_mode_sleep */
  h8_bool power_save;

  /** If true, the display will sleep when power save is enabled.
   *  If false, the display will standby. */
  h8_bool power_save_mode_sleep;

  h8_bool x_flip, y_flip;

  h8_bool segment_remap, internal_oscillator, display_on;

  /** Various unimplemented parameters */
  h8_u8 start_line, display_offset, multiplex_ratio, contrast, nline_inversion,
        dcdc_factor, irr_ratio, lcd_bias, pwm_frc, power_control;

  h8_u8 palette_modes[H8_LCD_PALETTE_SIZE];

  h8_lcd_sr_t status;
} h8_lcd_t;

/**
 * VRAM layout as array of bytes looks summat like this:
 *           x0     x1     x2     x3     ... x127
 *         |------|------|------|------|
 * y0-3    | 0000 | 0002 | 0004 | 0006 | ... $00FE / 254
 *         |------|------|------|------|
 * y4-7    | 0001 | 0003 | 0005 | 0007 | ... $00FF / 255
 *         |------|------|------|------|
 * y8-11   | 0100 | 0102 | 0104 | 0106 | ... $01FE / 510
 *         |------|------|------|------|
 * y12-15  | 0101 | 0103 | 0105 | 0107 | ... $01FF / 511
 *         |------|------|------|------|
 */

void h8_lcd_read(h8_device_t *device, h8_byte_t *dst)
{
  h8_lcd_t *m_lcd = device->device;

  if (!m_lcd->selected)
    return;
  else if (m_lcd->data_mode)
  {
    /* Data mode read -- retreive raw VRAM bytes */
    dst->u = m_lcd->vram[m_lcd->y * 0x0100 +
                         m_lcd->x * 2 +
                         m_lcd->second_read ? 1 : 0];

    /* If we are retreiving the second byte, increment only the X address */
    if (m_lcd->second_read)
    {
      m_lcd->x = (m_lcd->x + 1) & B01111111;
      m_lcd->second_read = FALSE;
    }
    else
      m_lcd->second_read = TRUE;
  }
  else
    /* Command mode read -- retreive the status register */
    dst->u = m_lcd->status.raw;
}

void h8_lcd_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_lcd_t *m_lcd = device->device;

  if (!m_lcd->selected)
    return;
  else if (m_lcd->data_mode)
  {
    /* Data mode write -- plot pixels */
    m_lcd->vram[m_lcd->y * 0x0100 +
                m_lcd->x * 2 +
                m_lcd->second_write_data ? 1 : 0] = value.u;

    /* If we are writing the second byte, increment only the X address */
    if (m_lcd->second_write_data)
    {
      m_lcd->x = (m_lcd->x + 1) & B01111111;
      m_lcd->second_write_data = FALSE;
    }
    else
      m_lcd->second_write_data = TRUE;
  }
  else
  {
    /* Command mode write -- execute commands */
    if (!m_lcd->second_write_cmd)
    {
      m_lcd->command = value.u;

      /**
       * Await second byte for 4X, 8X, or FX commands
       * @todo A little more nuanced than this but not for current purposes
       */
      if ((value.u >= 0x40 && value.u <= 0x4F) ||
          (value.u >= 0x80 && value.u <= 0x8F) ||
          (value.u >= 0xF0))
        m_lcd->second_write_cmd = TRUE;
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
      case 0x20:
      case 0x21:
      case 0x22:
      case 0x23:
      case 0x24:
      case 0x25:
      case 0x26:
      case 0x27:
        m_lcd->irr_ratio = value.u & B00000111;
        break;
      case 0x28:
      case 0x29:
      case 0x2A:
      case 0x2B:
      case 0x2C:
      case 0x2D:
      case 0x2E:
      case 0x2F:
        m_lcd->power_control = value.u & B00000111;
        break;
      case 0x50:
      case 0x51:
      case 0x52:
      case 0x53:
      case 0x54:
      case 0x55:
      case 0x56:
      case 0x57:
        m_lcd->lcd_bias = value.u & B00000111;
        break;
      case 0x64:
      case 0x65:
      case 0x66:
      case 0x67:
        m_lcd->dcdc_factor = value.u & B00000011;
        break;
      case 0x90:
      case 0x91:
      case 0x92:
      case 0x93:
      case 0x94:
      case 0x95:
      case 0x96:
      case 0x97:
        m_lcd->pwm_frc = value.u & B00000111;
        break;
      case 0xA0:
        m_lcd->segment_remap = FALSE;
        break;
      case 0xA1:
        m_lcd->segment_remap = TRUE;
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
      case 0xAB:
        m_lcd->internal_oscillator = TRUE;
        break;
      case 0xAE:
        m_lcd->display_on = FALSE;
        break;
      case 0xAF:
        m_lcd->display_on = TRUE;
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
        m_lcd->y = value.u & B00001111;
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
      case 0xF0:
      case 0xF1:
      case 0xF2:
      case 0xF3:
      case 0xF4:
      case 0xF5:
      case 0xF6:
      case 0xF7:
      case 0xF8:
      case 0xF9:
      case 0xFA:
      case 0xFB:
      case 0xFC:
      case 0xFD:
      case 0xFE:
      case 0xFF:
        /** Unknown/custom purpose extended command */
        break;
      default:
        break;
      }
      m_lcd->second_write_cmd = FALSE;
    }
  }

  *dst = value;
}

void h8_lcd_select_out(h8_device_t *device, const h8_bool on)
{
  h8_lcd_t *m_lcd = device->device;

  m_lcd->selected = !on;
}

void h8_lcd_mode_out(h8_device_t *device, const h8_bool on)
{
  h8_lcd_t *m_lcd = device->device;

  m_lcd->data_mode = on;
}

void h8_lcd_init(h8_device_t *device)
{
  if (device)
  {
    h8_lcd_t *m_lcd = h8_dma_alloc(sizeof(h8_lcd_t), TRUE);

    m_lcd->status.flags.on = TRUE;
    m_lcd->status.flags.id = 0x08;

    device->device = m_lcd;
    device->ssu_in = h8_lcd_read;
    device->ssu_out = h8_lcd_write;
    device->data = m_lcd->vram;
    device->name = name;
    device->type = type;
  }
}
