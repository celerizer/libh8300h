#include "dma.h"
#include "led.h"

static const char *name = "LED device";
static h8_device_id type = H8_DEVICE_LED;

static void h8_led_update_state(h8_led_t *led)
{
  led->state = led->on ?
               (led->green ? H8_LED_STATE_GREEN : H8_LED_STATE_RED) :
               H8_LED_STATE_OFF;
}

void h8_led_init(h8_device_t *device)
{
  if (device)
  {
    device->name = name;
    device->type = type;
    device->device = h8_dma_alloc(sizeof(h8_led_t), FALSE);
  }
}

void h8_led_write_0(h8_device_t *device, h8_byte_t *dst, h8_byte_t value)
{
#if H8_SAFETY
  if (device)
  {
#endif
    h8_led_t *m_led = (h8_led_t*)device->device;

#if H8_SAFETY
    if (m_led)
    {
#endif
      m_led->on = value.u;
      h8_led_update_state(m_led);
#if H8_SAFETY
    }
  }
#endif
  *dst = value;
}

void h8_led_write_1(h8_device_t *device, h8_byte_t *dst, h8_byte_t value)
{
#if H8_SAFETY
  if (device)
  {
#endif
    h8_led_t *m_led = (h8_led_t*)device->device;

#if H8_SAFETY
    if (m_led)
    {
#endif
      m_led->green = value.u;
      h8_led_update_state(m_led);
#if H8_SAFETY
    }
  }
#endif
  *dst = value;
}
