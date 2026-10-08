#ifndef H8_TIMER_B1_H
#define H8_TIMER_B1_H

#include "types.h"

/**
 * Section 9: Timer B1
 *
 * An 8-bit up-counter that requests an interrupt when it overflows, either
 * restarting from 0 (interval timer) or from TLB1 (auto-reload). TCB1 and
 * TLB1 share H'F0D1: reads return the counter, and writes set the reload
 * value.
 */

struct h8_system_t;

typedef struct
{
  /** Timer counter B1 */
  h8_u8 tcb1;

  /** Timer load register B1 */
  h8_u8 tlb1;

  /** Progress toward the next count for clock-based sources, in CPU states */
  h8_u32 prescaler;

  /** Progress toward the next count for subclock-based sources, in (states * subclock) */
  h8_u32 phase;
} h8_tb1_state_t;

void h8_tb1_init(struct h8_system_t *system);

/**
 * Advances Timer B1 by an amount of time.
 * @param states The time that elapsed, in system clock states
 * @param phi_running Whether the system clock runs; clock-based sources halt in
 * subactive, watch, and standby modes
 * @param sub_running Whether the watch clock runs; it halts in standby mode
 */
void h8_tb1_run(struct h8_system_t *system, unsigned states,
                h8_bool phi_running, h8_bool sub_running);

/** Register handlers */
void h8_tb1_tmb1_out(struct h8_system_t *system, h8_byte_t *byte,
                     const h8_byte_t value);
void h8_tb1_tcb1_in(struct h8_system_t *system, h8_byte_t *byte);
void h8_tb1_tlb1_out(struct h8_system_t *system, h8_byte_t *byte,
                     const h8_byte_t value);

#endif
