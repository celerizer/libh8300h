#ifndef H8_BUZZER_H
#define H8_BUZZER_H

#include "../device.h"

#ifndef H8_BUZZER_DEFAULT_RATE
/** The default output sample rate in Hz */
#define H8_BUZZER_DEFAULT_RATE 48000
#endif

/** The number of samples the ring buffer holds; must be a power of 2 */
#define H8_BUZZER_BUFFER_SIZE 4096

/** Maximum volume, as signed 16bit */
#define H8_BUZZER_AMPLITUDE 2048

typedef struct
{
  /** The levels of the two pins connected to the piezo */
  h8_bool pins[2];

  /** The output sample rate in Hz, or 0 to stop generating samples */
  unsigned rate;

  /** Sum of (drive level * states) over the sample being built */
  h8_s32 sum;

  /** Number of states accumulated into the sample being built */
  h8_u32 sum_states;

  /** Progress toward the next sample, in units of (states * rate) */
  h8_u32 phase;

  /** Previous input and output of the DC blocking filter */
  h8_s32 dc_in;
  h8_s32 dc_out;

  /** Ring buffer of finished samples */
  h8_s16 buffer[H8_BUZZER_BUFFER_SIZE];
  unsigned read_pos;
  unsigned write_pos;
} h8_buzzer_t;

void h8_buzzer_init(h8_device_t *device);

/** The pin on the piezo's positive terminal */
void h8_buzzer_pin_0_out(h8_device_t *device, const h8_bool on);

/** The pin on the piezo's negative terminal */
void h8_buzzer_pin_1_out(h8_device_t *device, const h8_bool on);

void h8_buzzer_step(h8_device_t *device, unsigned states, h8_u32 clock);

/**
 * Sets the output sample rate in Hz. Samples already buffered are discarded.
 */
void h8_buzzer_set_rate(h8_device_t *device, unsigned rate);

/**
 * Returns the number of samples waiting in the ring buffer.
 */
unsigned h8_buzzer_available(const h8_device_t *device);

/**
 * Moves samples out of the ring buffer.
 * @param buffer Where to write the samples
 * @param count The maximum number of samples to read
 * @return The number of samples actually read
 */
unsigned h8_buzzer_read(h8_device_t *device, h8_s16 *buffer, unsigned count);

#endif
