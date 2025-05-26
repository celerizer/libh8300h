#ifndef H8_LCD_H
#define H8_LCD_H

#include "../device.h"

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

  /**
   * 6-bit contrast value. NTR-032 program refers to this as "shade" with
   * selectable range of 22-31. Startup initializes it to 26.
   */
  h8_u8 contrast;

  /** Various unimplemented parameters */
  h8_u8 start_line, display_offset, multiplex_ratio, nline_inversion,
        dcdc_factor, irr_ratio, lcd_bias, pwm_frc, power_control;

  h8_u8 palette_modes[H8_LCD_PALETTE_SIZE];

  h8_lcd_sr_t status;
} h8_lcd_t;

void h8_lcd_init(h8_device_t *device);

void h8_lcd_select_out(h8_device_t *device, const h8_bool on);

void h8_lcd_mode_out(h8_device_t *device, const h8_bool on);

#endif
