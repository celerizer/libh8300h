#include "buttons.h"
#include "../dma.h"

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

static h8_bool h8_buttons_in(h8_device_t *device, unsigned index)
{
  if (device)
  {
    h8_buttons_t *m_buttons = device->device;
    return m_buttons->buttons[index];
  }
  else
    return 0;
}

h8_bool h8_buttons_in_0(h8_device_t *device)
{
  return h8_buttons_in(device, 0);
}

h8_bool h8_buttons_in_1(h8_device_t *device)
{
  return h8_buttons_in(device, 1);
}

h8_bool h8_buttons_in_2(h8_device_t *device)
{
  return h8_buttons_in(device, 2);
}
