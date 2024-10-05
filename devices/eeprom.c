#include "../dma.h"
#include "eeprom.h"

#include <string.h>

static const char *name_8k = "8KB EEPROM device";
static const char *name_64k = "64KB EEPROM device";

enum
{
  H8_EEPROM_INVALID = 0,

  H8_EEPROM_UNKNOWN_1,
  H8_EEPROM_WRITE,
  H8_EEPROM_READ,
  H8_EEPROM_UNKNOWN_2,
  H8_EEPROM_UNKNOWN_3,
  H8_EEPROM_UNKNOWN_4,

  H8_EEPROM_SIZE
};

typedef union
{
  struct
  {
    h8_byte_t command;
    h8_word_be_t address;
    h8_byte_t value;
  } parts;
  h8_byte_t raw[4];
} h8_eeprom_cmd_t;

typedef struct
{
  h8_byte_t *data;
  unsigned length;
  h8_eeprom_cmd_t rx_buf;
  h8_byte_t rx_pos;
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

    memcpy(*data, &m_eeprom->rx_buf, sizeof(m_eeprom->rx_buf));
    *data += sizeof(m_eeprom->rx_buf);
    *size += sizeof(m_eeprom->rx_buf);

    memcpy(*data, &m_eeprom->rx_pos, sizeof(m_eeprom->rx_pos));
    *data += sizeof(m_eeprom->rx_pos);
    *size += sizeof(m_eeprom->rx_pos);

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

    if (*size < m_eeprom->length +
        sizeof(m_eeprom->rx_buf) +
        sizeof(m_eeprom->rx_pos))
      return FALSE;

    memcpy(m_eeprom->data, *data, m_eeprom->length);
    *data += m_eeprom->length;
    *size += m_eeprom->length;

    memcpy(&m_eeprom->rx_buf, *data, sizeof(m_eeprom->rx_buf));
    *data += sizeof(m_eeprom->rx_buf);
    *size += sizeof(m_eeprom->rx_buf);

    memcpy(&m_eeprom->rx_pos, *data, sizeof(m_eeprom->rx_pos));
    *data += sizeof(m_eeprom->rx_pos);
    *size += sizeof(m_eeprom->rx_pos);

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

  if (eeprom->rx_buf.parts.command.u == H8_EEPROM_READ)
  {
    h8_u16 address = (h8_u16)((eeprom->rx_buf.parts.address.h.u << 8) |
                      eeprom->rx_buf.parts.address.l.u);

    *dst = eeprom->data[address & (eeprom->length - 1)];
  }
  else
    dst->u = 0;
}

void h8_eeprom_write(h8_device_t *device, h8_byte_t *dst, const h8_byte_t value)
{
  h8_eeprom_t *eeprom = (h8_eeprom_t*)device->device;

  eeprom->rx_buf.raw[eeprom->rx_pos.u] = value;
  eeprom->rx_pos.u++;

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

    device->read = h8_eeprom_read;
    device->write = h8_eeprom_write;
    device->save = h8_eeprom_serialize;
    device->load = h8_eeprom_deserialize;
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
