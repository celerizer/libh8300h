#include "../dma.h"
#include "lcd.h"

static const char *name = "128x64 LCD device";
static const h8_device_id type = H8_DEVICE_LCD;

typedef struct
{
  h8_u8 vram[128 * 64];
} h8_lcd_t;

void h8_lcd_read(h8_device_t *device, h8_byte_t *dst)
{
  h8_lcd_t *m_lcd = device->device;

  /** @todo */
  dst->u = m_lcd->vram[0];
}

void h8_lcd_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_lcd_t *m_lcd = device->device;

  /** @todo */
  m_lcd->vram[0] = value.u;
}

void h8_lcd_init(h8_device_t *device)
{
  if (device)
  {
    h8_lcd_t *m_lcd = h8_dma_alloc(sizeof(h8_lcd_t), TRUE);

    device->device = m_lcd;
    device->read = h8_lcd_read;
    device->write = h8_lcd_write;
    device->name = name;
    device->type = type;
  }
}
