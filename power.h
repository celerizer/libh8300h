#ifndef H8_POWER_H
#define H8_POWER_H

#include "types.h"

/**
 * Section 5: Power-Down Modes
 *
 * The SLEEP instruction halts the CPU in one of several modes, chosen by
 * SYSCR1 and SYSCR2, until an enabled interrupt is requested while the I bit
 * in CCR is clear. Some modes stop the system clock, halting the peripherals
 * that use it. The CPU can also run from the watch clock (subactive mode) or
 * a divided system clock (active medium-speed mode).
 */

struct h8_system_t;

typedef enum
{
  /** Active (high-speed) mode: the CPU runs on the system clock */
  H8_POWER_ACTIVE = 0,

  /** Active (medium-speed) mode: the CPU runs on the oscillator clock / 8 to 64 */
  H8_POWER_ACTIVE_MEDIUM,

  /** Subactive mode: the CPU runs on the subclock / 8 to 1 */
  H8_POWER_SUBACTIVE,

  /** Sleep (high-speed) mode: the CPU halts; peripherals keep running */
  H8_POWER_SLEEP,

  /** Sleep (medium-speed) mode */
  H8_POWER_SLEEP_MEDIUM,

  /** Subsleep mode: the CPU halts; the system clock is stopped */
  H8_POWER_SUBSLEEP,

  /** Watch mode: the CPU and system clock stop; subclock peripherals keep running */
  H8_POWER_WATCH,

  /** Standby mode: the CPU, system clock, and most peripherals stop */
  H8_POWER_STANDBY
} h8_power_mode;

typedef struct
{
  /** The current operating mode */
  h8_power_mode mode;

  /** The mode an interrupt returns to from a halted mode */
  h8_power_mode wake_mode;

  /** The leftover fraction of a system clock state from converting slower clocks */
  h8_u32 remainder;

  /** Waiting time to charge to the current step, in system clock states */
  h8_u32 wait_states;
} h8_power_t;

void h8_power_init(struct h8_system_t *system);

/**
 * Returns whether the CPU is halted in a sleep, subsleep, watch, or standby
 * mode.
 */
h8_bool h8_power_halted(const struct h8_system_t *system);

/**
 * Returns whether the system clock is running, which drives clock-based
 * peripherals.
 */
h8_bool h8_power_phi_running(const struct h8_system_t *system);

/**
 * Returns whether peripherals clocked from the watch clock keep counting.
 * They stop in standby mode, and Timer W also stops in watch mode.
 */
h8_bool h8_power_sub_running(const struct h8_system_t *system);

/**
 * Carries out the SLEEP instruction, per table 5.2.
 */
void h8_power_sleep(struct h8_system_t *system);

/**
 * Returns from a halted mode when an interrupt is accepted.
 */
void h8_power_wake(struct h8_system_t *system);

/**
 * Converts states of the CPU clock in a mode into system clock states,
 * which every peripheral counts in.
 */
h8_u32 h8_power_to_phi(struct h8_system_t *system, h8_power_mode mode,
                       unsigned states);

/**
 * The time that passes per step while the CPU is halted, in system clock states.
 */
h8_u32 h8_power_idle_states(const struct h8_system_t *system);

#endif
