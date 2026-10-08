#include "buzzer.h"
#include "../dma.h"

static const char *name = "Piezo buzzer";
static const h8_device_id type = H8_DEVICE_BUZZER;

#define H8_BUZZER_DC_POLE 32604

static h8_s32 h8_buzzer_div(const h8_s32 n, const h8_s32 d)
{
  return n >= 0 ? n / d : -((-n) / d);
}

void h8_buzzer_init(h8_device_t *device)
{
  if (device)
  {
    h8_buzzer_t *buzzer = h8_dma_alloc(sizeof(h8_buzzer_t), TRUE);

    buzzer->rate = H8_BUZZER_DEFAULT_RATE;

    device->name = name;
    device->type = type;
    device->device = buzzer;
    device->step = h8_buzzer_step;
  }
}

void h8_buzzer_pin_0_out(h8_device_t *device, const h8_bool on)
{
  ((h8_buzzer_t*)device->device)->pins[0] = on;
}

void h8_buzzer_pin_1_out(h8_device_t *device, const h8_bool on)
{
  ((h8_buzzer_t*)device->device)->pins[1] = on;
}

static void h8_buzzer_push(h8_buzzer_t *buzzer, const h8_s32 input)
{
  unsigned next = (buzzer->write_pos + 1) & (H8_BUZZER_BUFFER_SIZE - 1);
  h8_s32 output;

  output = input - buzzer->dc_in +
           h8_buzzer_div(buzzer->dc_out * H8_BUZZER_DC_POLE, 32768);
  buzzer->dc_in = input;
  buzzer->dc_out = output;

  if (output > 32767)
    output = 32767;
  else if (output < -32768)
    output = -32768;

  /* When the frontend falls behind, drop the oldest sample to bound latency */
  if (next == buzzer->read_pos)
    buzzer->read_pos = (buzzer->read_pos + 1) & (H8_BUZZER_BUFFER_SIZE - 1);
  buzzer->buffer[buzzer->write_pos] = (h8_s16)output;
  buzzer->write_pos = next;
}

void h8_buzzer_step(h8_device_t *device, unsigned states, h8_u32 clock)
{
  h8_buzzer_t *buzzer = device->device;
  h8_s32 level;

  if (!buzzer->rate || !states)
    return;

  /* The voltage across the piezo: -1, 0, or 1 times the supply */
  level = (h8_s32)buzzer->pins[0] - (h8_s32)buzzer->pins[1];

  /* Box filter */
  buzzer->sum += level * (h8_s32)states;
  buzzer->sum_states += states;
  buzzer->phase += states * buzzer->rate;

  while (buzzer->phase >= clock)
  {
    /* The part of this step past the sample boundary belongs to the next */
    h8_u32 leftover;
    h8_s32 sum;
    h8_u32 sum_states;

    buzzer->phase -= clock;
    leftover = buzzer->phase / buzzer->rate;
    if (leftover > buzzer->sum_states)
      leftover = buzzer->sum_states;
    sum = buzzer->sum - level * (h8_s32)leftover;
    sum_states = buzzer->sum_states - leftover;

    h8_buzzer_push(buzzer, sum_states ?
                   h8_buzzer_div(sum * H8_BUZZER_AMPLITUDE, (h8_s32)sum_states) :
                   level * H8_BUZZER_AMPLITUDE);

    buzzer->sum = level * (h8_s32)leftover;
    buzzer->sum_states = leftover;
  }
}

void h8_buzzer_set_rate(h8_device_t *device, unsigned rate)
{
  h8_buzzer_t *buzzer = device->device;

  buzzer->rate = rate;
  buzzer->sum = 0;
  buzzer->sum_states = 0;
  buzzer->phase = 0;
  buzzer->read_pos = buzzer->write_pos;
}

unsigned h8_buzzer_available(const h8_device_t *device)
{
  const h8_buzzer_t *buzzer = device->device;

  return (buzzer->write_pos - buzzer->read_pos) & (H8_BUZZER_BUFFER_SIZE - 1);
}

unsigned h8_buzzer_read(h8_device_t *device, h8_s16 *buffer, unsigned count)
{
  h8_buzzer_t *buzzer = device->device;
  unsigned i;

  for (i = 0; i < count && buzzer->read_pos != buzzer->write_pos; i++)
  {
    buffer[i] = buzzer->buffer[buzzer->read_pos];
    buzzer->read_pos = (buzzer->read_pos + 1) & (H8_BUZZER_BUFFER_SIZE - 1);
  }

  return i;
}
