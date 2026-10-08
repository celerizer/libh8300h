#ifndef H8_COMPARATOR_H
#define H8_COMPARATOR_H

#include "types.h"

/**
 * Section 18: Comparators
 *
 * Two comparators check the voltage on COMP0 and COMP1 against a reference.
 * COMP0 and COMP1 share pins with AN4 and AN5, so they see whatever analog
 * device is hooked up to those A/D channels. The result is computed whenever
 * CMDR is read.
 */

struct h8_system_t;

typedef struct
{
  /** The last result of each comparator */
  h8_bool output[2];
} h8_comparator_state_t;

/**
 * Puts the comparator registers and internal state into their reset values.
 */
void h8_comparator_init(struct h8_system_t *system);

/**
 * CMDR read handler: updates CDR0 and CDR1 from the input pins.
 * @todo CMF0 and CMF1 are never set, so comparator interrupts never occur.
 */
void h8_comparator_cmdr_in(struct h8_system_t *system, h8_byte_t *byte);

/** CMDR write handler: CMF0 and CMF1 can only be cleared by writing 0 */
void h8_comparator_cmdr_out(struct h8_system_t *system, h8_byte_t *byte,
                            const h8_byte_t value);

#endif
