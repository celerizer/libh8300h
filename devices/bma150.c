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

  /* ROM tries to read immediately after sending control byte, when it should
   * send a don't care data byte first. This flag is a hack to keep track.
   * Not sure if this is correct behavior, but it seems like it should only
   * read bytes 0 and 1 on boot, while without this check also reads 2 */
  h8_bool read_next;

  h8_bool selected;
} h8_bma150_t;

#define H8_BMA150_WRITING 0
#define H8_BMA150_READING 1

void h8_bma150_select_pin(h8_device_t *device, h8_bool on)
{
  h8_bma150_t *bma = device->device;

  bma->selected = !on;
}

/** 4.1.1 Four-wire SPI interface - Figure 7 */
void h8_bma150_read(h8_device_t *device, h8_byte_t *dst)
{
  h8_bma150_t *bma = device->device;

  if (!bma->selected)
    return;
  else if (bma->read_next)
    bma->read_next = FALSE;
  else if (bma->state.parts.mode == H8_BMA150_READING)
    *dst = bma->data[bma->state.parts.addr];
}

/** 4.1.1 Four-wire SPI interface - Figure 6 */
void h8_bma150_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_bma150_t *bma = device->device;

  if (!bma->selected)
    return;

  bma->read_next = FALSE;

  if (bma->state.parts.mode == H8_BMA150_WRITING && bma->state.parts.addr)
  {
    if (bma->state.parts.addr > 0x0A)
      bma->data[bma->state.parts.addr] = value;
    else
      printf("ERROR: BMA150 write to read-only address 0x%02X\n",
             bma->state.parts.addr);
    bma->state.parts.addr = 0;
  }
  else if (bma->state.parts.mode == H8_BMA150_READING)
    bma->state.parts.addr++;
  else
  {
    bma->state.raw = value;
    if (bma->state.parts.mode == H8_BMA150_READING)
    {
      bma->state.parts.addr--;
      bma->read_next = TRUE;
    }
  }

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

    device->read = h8_bma150_read;
    device->write = h8_bma150_write;
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
  }
}
