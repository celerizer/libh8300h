#include "bma150.h"

#include "../dma.h"
#include "../logger.h"

static const char *name = "Bosch BMA150 Triaxial digital acceleration sensor";
static const h8_device_id type = H8_DEVICE_BMA150;

typedef union
{
  H8_BITFIELD_3
  (
    h8_u16 data : 10,
    h8_u16 unused : 5,
    h8_u16 new : 1
  ) flags;
  h8_word_t raw;
} h8_bma150_acc_t;

typedef struct
{
  union
  {
    struct
    {
      h8_u8 chip_id;
      h8_u8 version;
      h8_bma150_acc_t x;
      h8_bma150_acc_t y;
      h8_bma150_acc_t z;
      h8_u8 unsorted[0x78];
    } parts;
    h8_byte_t raw[0x80];
  } data;

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

  h8_u16 selected;
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

    *dst = bma->data.raw[address];

    /* Clear "new_data" flags for accel axis */
    if (address == 0x02 || address == 0x03)
      bma->data.parts.x.flags.new = 0;
    else if (address == 0x04 || address == 0x05)
      bma->data.parts.y.flags.new = 0;
    else if (address == 0x06 || address == 0x07)
      bma->data.parts.z.flags.new = 0;

    h8_log(H8_LOG_INFO, H8_LOG_SSU, "BMA150 read 0x%02X -> %02X",
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
        h8_log(H8_LOG_INFO, H8_LOG_SSU, "BMA150 write 0x%02X -> %02X",
               bma->state.parts.addr, value.u);
        bma->data.raw[bma->state.parts.addr] = value;
      }
      else
        h8_log(H8_LOG_INFO, H8_LOG_SSU, "BMA150 ERROR attempted write to "
                                        "read-only address %02X",
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
    bma->data.raw[0x00].u = B00000010;
    /** @todo al_version and ml_version?
    bma->data.raw[0x01].u = ??? */
    bma->data.raw[0x0B].u = B00000011;
    bma->data.raw[0x0C].u = 20;
    bma->data.raw[0x0D].u = 150;
    bma->data.raw[0x0E].u = 160;
    bma->data.raw[0x0F].u = 150;
    bma->data.raw[0x12].u = 162;
    bma->data.raw[0x13].u = 13;
    bma->data.raw[0x14].u = B00001110;
    bma->data.raw[0x15].u = B10000000;
    bma->data.raw[0x2B].u = B00000011;
    bma->data.raw[0x2C].u = 20;
    bma->data.raw[0x2D].u = 150;
    bma->data.raw[0x2E].u = 160;
    bma->data.raw[0x2F].u = 150;
    bma->data.raw[0x32].u = 162;
    bma->data.raw[0x33].u = 13;
    bma->data.raw[0x34].u = B00001110;
    bma->data.raw[0x35].u = B10000000;

    bma->count = 0;
  }
}

void h8_bma150_set_axis(h8_device_t *device, h8_u16 x, h8_u16 y, h8_u16 z)
{
  if (device && device->type == H8_DEVICE_BMA150 && device->device)
  {
    h8_bma150_t *bma = device->device;

    bma->data.parts.x.flags.data = x;
    bma->data.parts.x.flags.new = 1;
    bma->data.parts.y.flags.data = y;
    bma->data.parts.y.flags.new = 1;
    bma->data.parts.z.flags.data = z;
    bma->data.parts.z.flags.new = 1;
  }
}
