#include "../dma.h"
#include "bma150.h"

#include <stdio.h>

static const char *name = "Bosch BMA150 Triaxial digital acceleration sensor";
static const h8_device_id type = H8_DEVICE_BMA150;

typedef struct
{
  h8_byte_t data[0x80];

  union
  {
    H8_BITFIELD_2
    (
      h8_u8 addr : 7,
      h8_u8 mode : 1
    ) parts;
    h8_byte_t raw;
  } state;

  h8_u8 count;

  h8_bool selected;
} h8_bma150_t;

#define H8_BMA150_WRITING 0
#define H8_BMA150_READING 1

void h8_bma150_select_out(h8_device_t *device, h8_bool on)
{
  h8_bma150_t *bma = device->device;

  bma->selected = !on;
  if (!bma->selected)
    bma->count = 0;
}

/** 4.1.1 Four-wire SPI interface - Figure 7 */
void h8_bma150_read(h8_device_t *device, h8_byte_t *dst)
{
  h8_bma150_t *bma = device->device;

  if (!bma->selected)
    return;
  else if (bma->state.parts.mode == H8_BMA150_READING && bma->count > 1)
  {
    unsigned address = bma->state.parts.addr + bma->count - 2;

    *dst = bma->data[address];
    printf("[BMA150] read 0x%02X -> %02X\n",
           address, dst->u);
  }
}

/** 4.1.1 Four-wire SPI interface - Figure 6 */
void h8_bma150_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_bma150_t *bma = device->device;

  /* Not selected. Ignore SSU command. */
  if (!bma->selected)
    return;

  /* RW pulled down. This is a new command. */
  if (bma->state.parts.mode == H8_BMA150_READING && !(value.u & B10000000))
    bma->count = 0;

  if (bma->count == 0)
    bma->state.raw = value;
  else
  {
    if (bma->state.parts.mode == H8_BMA150_WRITING)
    {
      if (bma->state.parts.addr >= 0x0A)
      {
        printf("[BMA150] write 0x%02X -> %02X\n",
               bma->state.parts.addr, value.u);
        bma->data[bma->state.parts.addr] = value;
      }
      else
        printf("[BMA150] ERROR attempted write to read-only address %02X \n",
               bma->state.parts.addr);
      bma->count = 0;
    }
  }
  bma->count++;

  *dst = value;
}

void h8_bma150_init(h8_device_t *device)
{
  if (device)
  {
    h8_bma150_t *bma = h8_dma_alloc(sizeof(h8_bma150_t), TRUE);

    device->name = name;
    device->type = type;
    device->device = bma;

    device->ssu_in = h8_bma150_read;
    device->ssu_out = h8_bma150_write;
    device->save = NULL; /** @todo */
    device->load = NULL; /** @todo */

    /** 3. Global Memory Map - Figure 1 */
    bma->data[0x00].u = B00000010;
    /** @todo al_version and ml_version?
    bma->data[0x01].u = ??? */
    bma->data[0x0B].u = B00000011;
    bma->data[0x0C].u = 20;
    bma->data[0x0D].u = 150;
    bma->data[0x0E].u = 160;
    bma->data[0x0F].u = 150;
    bma->data[0x12].u = 162;
    bma->data[0x13].u = 13;
    bma->data[0x14].u = B00001110;
    bma->data[0x15].u = B10000000;
    bma->data[0x2B].u = B00000011;
    bma->data[0x2C].u = 20;
    bma->data[0x2D].u = 150;
    bma->data[0x2E].u = 160;
    bma->data[0x2F].u = 150;
    bma->data[0x32].u = 162;
    bma->data[0x33].u = 13;
    bma->data[0x34].u = B00001110;
    bma->data[0x35].u = B10000000;

    bma->count = 0;
  }
}
