#ifndef H8_TIMER_W_H
#define H8_TIMER_W_H

#include "types.h"

/**
 * Section 10: Timer W
 *
 * A 16-bit counter with four general registers that can generate compare match
 * outputs, PWM waveforms and interrupts. The memory-mapped registers live in
 * h8_tw_t; this file holds the internal state that is not visible to software.
 */

struct h8_system_t;

/** The four timer output pins */
typedef enum
{
  /** FTIOA, shared with P10 */
  H8_TW_PIN_A = 0,

  /** FTIOB, shared with P82 */
  H8_TW_PIN_B,

  /** FTIOC, shared with P83 */
  H8_TW_PIN_C,

  /** FTIOD, shared with P84 */
  H8_TW_PIN_D,

  H8_TW_PIN_SIZE
} h8_tw_pin;

typedef struct
{
  /** Progress toward the next count for clock-based sources, in CPU states */
  h8_u32 prescaler;

  /**
   * Progress toward the next count for subclock-based sources, in units of
   * (CPU states * subclock), so no precision is lost converting between clocks
   */
  h8_u32 phase;

  /** Current level of each timer output, whether or not it drives its pin */
  h8_bool output[H8_TW_PIN_SIZE];

  /** Whether the timer drove each pin when devices were last notified */
  h8_bool pin_driven[H8_TW_PIN_SIZE];

  /** The level devices were last notified of for each timer-driven pin */
  h8_bool pin_level[H8_TW_PIN_SIZE];
} h8_tw_state_t;

/**
 * Puts Timer W registers and internal state into their reset values.
 */
void h8_tw_init(struct h8_system_t *system);

/**
 * Advances Timer W by an amount of time.
 * @param states The time that elapsed, in system clock states
 * @param phi_running Whether the system clock runs, for clock-based sources
 * @param sub_running Whether subclock-based sources may count in this mode
 */
void h8_tw_run(struct h8_system_t *system, unsigned states,
               h8_bool phi_running, h8_bool sub_running);

/**
 * Returns how long until the counter next reaches a compare match or
 * overflows, in system clock states, or max if that is later or the
 * counter is stopped. Lets a halted CPU skip ahead without delaying output
 * edges or interrupt flags.
 */
h8_u32 h8_tw_states_until_event(const struct h8_system_t *system, h8_u32 max,
                                h8_bool phi_running, h8_bool sub_running);

/**
 * Returns whether Timer W currently controls a pin instead of its port data
 * register. This is the case when the pin is in PWM mode, or when compare
 * match output is selected for it in TIOR.
 */
h8_bool h8_tw_drives_pin(const struct h8_system_t *system, unsigned pin);

/**
 * Notifies devices of any timer output pin whose driver or level changed.
 * Called whenever an output or a pin function setting changes.
 */
void h8_tw_update_pins(struct h8_system_t *system);

/** Register write handlers */
void h8_tw_tmrw_out(struct h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value);
void h8_tw_tcrw_out(struct h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value);
void h8_tw_tierw_out(struct h8_system_t *system, h8_byte_t *byte,
                     const h8_byte_t value);
void h8_tw_tsrw_out(struct h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value);
void h8_tw_tior_out(struct h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value);

#endif
