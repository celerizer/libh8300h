#include "buttons.h"
#include "dma.h"

static const char *name_1 = "1-button input device";
static const char *name_3 = "3-button input device";

static void h8_buttons_init(h8_device_t *device, h8_device_id type)
{
  if (device)
  {
    h8_buttons_t *buttons = h8_dma_alloc(sizeof(h8_buttons_t), TRUE);

    buttons->button_count = type == H8_DEVICE_1BUTTON ? 1 : 3;

    device->name = type == H8_DEVICE_1BUTTON ? name_1 : name_3;
    device->type = type;
    device->device = buttons;
  }
}

void h8_buttons_init_1b(h8_device_t *device)
{
  h8_buttons_init(device, H8_DEVICE_1BUTTON);
}

void h8_buttons_init_3b(h8_device_t *device)
{
  h8_buttons_init(device, H8_DEVICE_3BUTTON);
}

void h8_buttons_read(h8_device_t *device, h8_byte_t *byte)
{
  if (device)
  {
    h8_buttons_t *m_buttons = device->device;
    unsigned i;

    byte->u &= ((1 << m_buttons->button_count) - 1);
    for (i = 0; i < m_buttons->button_count; i++)
      byte->u |= (m_buttons->buttons[i] << i);
  }
}
