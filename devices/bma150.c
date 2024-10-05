#include "../dma.h"
#include "bma150.h"

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
} h8_bma150_t;

#define H8_BMA150_WRITING 0
#define H8_BMA150_READING 1

/** 4.1.1 Four-wire SPI interface - Figure 7 */
void h8_bma150_read(h8_device_t *device, h8_byte_t *dst)
{
  h8_bma150_t *bma = device->device;

  if (bma->state.parts.mode == H8_BMA150_READING)
    *dst = bma->data[bma->state.parts.addr];
}

/** 4.1.1 Four-wire SPI interface - Figure 6 */
void h8_bma150_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_bma150_t *bma = device->device;

  if (bma->state.parts.mode == H8_BMA150_WRITING && bma->state.parts.addr)
  {
    bma->data[bma->state.parts.addr] = value;
    bma->state.parts.addr = 0;
  }
  else if (bma->state.parts.mode == H8_BMA150_READING)
    bma->state.parts.addr++;
  else
    bma->state.raw = value;

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
  }
}
