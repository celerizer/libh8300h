#include "../dma.h"
#include "eeprom.h"

#include <string.h>

static const char *name_8k = "8KB EEPROM device";
static const char *name_64k = "64KB EEPROM device";

enum
{
  H8_EEPROM_INVALID = 0,

  /**
   * Write Status Register
   * If WEL is on (?), write next byte to status register
   * 2 byte payload
   */
  H8_EEPROM_WRSR,

  /**
   * Write
   * If WEL is on, take an address then write values to it for every byte
   * afterwards, incrementing the address
   * 4+ byte payload
   */
  H8_EEPROM_WRITE,

  /**
   * Read
   * Take an address, then read from it for every "don't care" byte afterwards,
   * incrementing the address
   * 4+ byte payload
   */
  H8_EEPROM_READ,

  /**
   * Write Disable
   * Set the WEL to zero, diabling writes
   * 1 byte payload
   */
  H8_EEPROM_WRDI,

  /**
   * Read Status Register
   * Return the status register after the next "don't care" byte?
   * 2 byte payload
   */
  H8_EEPROM_RDSR,

  /**
   * Write Enable
   * Set the WEL to one, enabling writes
   * 1 byte payload
   */
  H8_EEPROM_WREN,

  H8_EEPROM_SIZE
};

typedef struct
{
  h8_byte_t *data;
  unsigned length;
  h8_word_t address;
  h8_u8 command;
  h8_s8 position;
  h8_bool selected;
  union
  {
    H8_BITFIELD_5
    (
      /** Write In Progress (unimplemented) */
      h8_u8 wip : 1,

      /** Write Enable Latch */
      h8_u8 wel : 1,

      /** Block Protect bits (unimplemented) */
      h8_u8 bp : 2,

      h8_u8 reserved : 3,

      /** Status Register Write Disable */
      h8_u8 srwd : 1
    ) flags;
    h8_byte_t raw;
  } status;
} h8_eeprom_t;

h8_bool h8_eeprom_serialize(const h8_device_t *device, h8_u8 **data,
                            unsigned *size)
{
  if (!device || !device->device || !data || !*data || !size || *size == 0)
    return FALSE;
  else
  {
    const h8_eeprom_t *m_eeprom = (h8_eeprom_t*)device->device;

    memcpy(*data, m_eeprom->data, m_eeprom->length);
    *data += m_eeprom->length;
    *size += m_eeprom->length;

    /** @todo */

    return TRUE;
  }
}

h8_bool h8_eeprom_deserialize(h8_device_t *device, const h8_u8 **data,
                              unsigned *size)
{
  if (!device || !device->device || !data || !*data || !size || *size == 0)
    return FALSE;
  else
  {
    h8_eeprom_t *m_eeprom = (h8_eeprom_t*)device->device;

    /*if (*size < m_eeprom->length +
        sizeof(m_eeprom->rx_buf) +
        sizeof(m_eeprom->rx_pos))
      return FALSE;*/

    memcpy(m_eeprom->data, *data, m_eeprom->length);
    *data += m_eeprom->length;
    *size += m_eeprom->length;

    /** @todo */

    return TRUE;
  }
}

void h8_eeprom_free(h8_device_t *device)
{
  if (device && device->device)
    h8_dma_free(((h8_eeprom_t*)device->device)->data);
}

void h8_eeprom_read(h8_device_t *device, h8_byte_t *dst)
{
  h8_eeprom_t *eeprom = (h8_eeprom_t*)device->device;

  if (!eeprom->selected)
    return;
  else switch (eeprom->command)
  {
  case H8_EEPROM_READ:
    if (eeprom->position > 3)
    {
      *dst = eeprom->data[eeprom->address.u];
      printf("[EEPROM] read 0x%04X -> %02X\n", eeprom->address.u, dst->u);
    }
    break;
  case H8_EEPROM_RDSR:
    *dst = eeprom->status.raw;
    break;
  }

  dst->u = 0;
}

void h8_eeprom_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_eeprom_t *eeprom = (h8_eeprom_t*)device->device;

  if (!eeprom->selected)
    goto end;
  else if (eeprom->position == 0)
  {
    eeprom->command = value.u;

    /* Handle 1-byte payloads */
    if (eeprom->command == H8_EEPROM_WREN)
    {
      eeprom->status.flags.wel = 1;
      eeprom->position = -1;
      goto end;
    }
    else if (eeprom->command == H8_EEPROM_WRDI)
    {
      eeprom->status.flags.wel = 0;
      eeprom->position = -1;
      goto end;
    }
  }
  else if (eeprom->position == 1)
  {
    /* Handle first byte of address, or value in two-byte payloads */
    switch (eeprom->command)
    {
    case H8_EEPROM_WRITE:
    case H8_EEPROM_READ:
      eeprom->address.h = value;
      goto end;
    case H8_EEPROM_RDSR:
      eeprom->position = -1;
      goto end;
    case H8_EEPROM_WRSR:
      if (eeprom->status.flags.wel)
        eeprom->status.raw = value;
      eeprom->position = -1;
      goto end;
    }
  }
  else if (eeprom->position == 2)
    eeprom->address.l = value;
  else
  {
    /* Handle 4 byte payload */
    if (eeprom->command == H8_EEPROM_WRITE)
    {
      if (eeprom->status.flags.wel)
      {
        eeprom->data[eeprom->address.u] = value;
        printf("[EEPROM] write 0x%04X -> %02X\n", eeprom->address.u, value.u);
      }
      eeprom->address.u++;
      goto end;
    }
    /* If reading, ensure address only increments after receiving first d/c */
    else if (eeprom->position != 3)
      eeprom->address.u++;
  }

end:
  eeprom->position++;
  *dst = value;
}

void h8_eeprom_init(h8_device_t *device, unsigned type)
{
  if (device)
  {
    h8_eeprom_t *eeprom = h8_dma_alloc(sizeof(h8_eeprom_t), TRUE);

    eeprom->data = h8_dma_alloc(type == H8_DEVICE_EEPROM_8K ? 8 * 1024 :
                                                              64 * 1024, FALSE);
    eeprom->length = type == H8_DEVICE_EEPROM_8K ? 8 * 1024 : 64 * 1024;

    device->name = type == H8_DEVICE_EEPROM_8K ? name_8k : name_64k;
    device->type = type;
    device->device = eeprom;
    device->data = eeprom->data;
    device->size = eeprom->length;

    device->ssu_in = h8_eeprom_read;
    device->ssu_out = h8_eeprom_write;
    device->save = h8_eeprom_serialize;
    device->load = h8_eeprom_deserialize;
  }
}

void h8_eeprom_select_out(h8_device_t *device, const h8_bool on)
{
  h8_eeprom_t *m_eeprom = (h8_eeprom_t*)device->device;

  m_eeprom->selected = !on;
  if (!m_eeprom->selected)
  {
    m_eeprom->position = 0;
    m_eeprom->address.u = 0;
  }
}

void h8_eeprom_init_64k(h8_device_t *device)
{
  h8_eeprom_init(device, H8_DEVICE_EEPROM_64K);
}

void h8_eeprom_init_8k(h8_device_t *device)
{
  h8_eeprom_init(device, H8_DEVICE_EEPROM_8K);
}
