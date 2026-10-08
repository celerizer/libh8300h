#include "../dma.h"
#include "led.h"

static const char *name = "LED device";
static h8_device_id type = H8_DEVICE_LED;

static void h8_led_update_state(h8_led_t *led)
{
  if (led->red && led->green)
    led->state = H8_LED_STATE_BOTH;
  else if (led->red)
    led->state = H8_LED_STATE_RED;
  else if (led->green)
    led->state = H8_LED_STATE_GREEN;
  else
    led->state = H8_LED_STATE_OFF;
}

void h8_led_init(h8_device_t *device)
{
  if (device)
  {
    device->name = name;
    device->type = type;
    device->device = h8_dma_alloc(sizeof(h8_led_t), TRUE);
    if (device->device)
      h8_led_update_state((h8_led_t*)device->device);
  }
}

void h8_led_red_out(h8_device_t *device, const h8_bool on)
{
  h8_led_t *led;

  if (!device || !device->device)
    return;
  led = (h8_led_t*)device->device;
  led->red = on;
  h8_led_update_state(led);
}

void h8_led_green_out(h8_device_t *device, const h8_bool on)
{
  h8_led_t *led;

  if (!device || !device->device)
    return;
  led = (h8_led_t*)device->device;
  led->green = on;
  h8_led_update_state(led);
}
