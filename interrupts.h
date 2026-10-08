#ifndef H8_INTERRUPTS_H
#define H8_INTERRUPTS_H

#include "types.h"

/**
 * Section 3: Exception Handling
 *
 * The interrupt controller decides which pending, enabled interrupt request
 * is accepted next. The CPU side (saving PC and CCR, vectoring, RTE) is in
 * emu.c.
 */

struct h8_system_t;

/**
 * Exception vector numbers, from table 3.1. The vector address is twice the
 * vector number. Lower numbers have higher priority.
 */
typedef enum
{
  H8_VECTOR_RESET = 0,
  H8_VECTOR_NMI = 7,
  H8_VECTOR_TRAPA0 = 8,
  H8_VECTOR_TRAPA1 = 9,
  H8_VECTOR_TRAPA2 = 10,
  H8_VECTOR_TRAPA3 = 11,
  H8_VECTOR_SLEEP = 13,
  H8_VECTOR_IRQ0 = 16,
  H8_VECTOR_IRQ1 = 17,
  H8_VECTOR_IRQAEC = 18,
  H8_VECTOR_COMP0 = 21,
  H8_VECTOR_COMP1 = 22,
  H8_VECTOR_RTC_QUARTER_SECOND = 23,
  H8_VECTOR_RTC_HALF_SECOND = 24,
  H8_VECTOR_RTC_SECOND = 25,
  H8_VECTOR_RTC_MINUTE = 26,
  H8_VECTOR_RTC_HOUR = 27,
  H8_VECTOR_RTC_DAY = 28,
  H8_VECTOR_RTC_WEEK = 29,
  H8_VECTOR_RTC_FREE_RUNNING = 30,
  H8_VECTOR_WDT = 31,
  H8_VECTOR_AEC = 32,
  H8_VECTOR_TIMER_B1 = 33,
  H8_VECTOR_SSU = 34,
  H8_VECTOR_TIMER_W = 35,
  H8_VECTOR_SCI3 = 37,
  H8_VECTOR_ADC = 38,

  H8_VECTOR_SIZE = 40
} h8_vector;

/** The last sampled level of the IRQ0 and IRQ1 input pins */
typedef struct
{
  h8_bool pin_level[2];
} h8_irq_state_t;

/**
 * Samples the pins assigned to IRQ0 and IRQ1, and sets their request flags in
 * IRR1 on the edge selected in IEGR. External interrupts work in every mode.
 */
void h8_interrupt_pins(struct h8_system_t *system);

/**
 * Returns the vector number of the highest-priority maskable interrupt that is
 * both requested and enabled, or 0 if there is none. Does not check the I bit
 * in CCR.
 */
unsigned h8_interrupt_pending(const struct h8_system_t *system);

/**
 * Write handler for IRR1, IRR2 and RTCFLG, whose flags can only be cleared by
 * writing 0.
 */
void h8_interrupt_flag_out(struct h8_system_t *system, h8_byte_t *byte,
                           const h8_byte_t value);

#endif
