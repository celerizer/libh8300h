#ifndef H8_SCI3_H
#define H8_SCI3_H

#include "types.h"

/**
 * Section 14: Serial Communication Interface 3 (SCI3, IrDA)
 *
 * Models the timing of asynchronous transfers: each character takes a full
 * frame (start bit, data, parity, stop bits) at the configured bit rate. The
 * memory-mapped registers live in h8_aec_sci3_t; this is the internal state.
 * Characters leave through h8_ir_line_out and arrive through
 * h8_sci3_receive.
 */

struct h8_system_t;

typedef struct
{
  /** Whether the transmit shift register (TSR) is sending a character */
  h8_bool tx_busy;

  /** The character in TSR */
  h8_u8 tsr;

  /** States left until the character in TSR has been sent */
  h8_u32 tx_remaining;

  /**
   * The SSR3 flags the CPU last read as 1. Writing 0 only clears these, so
   * a flag set between the read and the write (such as RDRF for a character
   * that just arrived) survives a read-modify-write of SSR3.
   */
  h8_u8 ssr3_read;
} h8_sci3_state_t;

void h8_sci3_init(struct h8_system_t *system);

/**
 * Advances the transmitter by an amount of CPU time.
 */
void h8_sci3_run(struct h8_system_t *system, unsigned states);

/**
 * Returns the number of CPU states one character frame takes with the current
 * SMR3, BRR3 and SEMR settings.
 */
h8_u32 h8_sci3_frame_states(const struct h8_system_t *system);

/**
 * Called when a whole character has arrived on the receive line.
 */
void h8_sci3_receive(struct h8_system_t *system, h8_u8 value);

/** Register handlers */
void h8_sci3_scr3_out(struct h8_system_t *system, h8_byte_t *byte,
                      const h8_byte_t value);
void h8_sci3_tdr3_out(struct h8_system_t *system, h8_byte_t *byte,
                      const h8_byte_t value);
void h8_sci3_ssr3_in(struct h8_system_t *system, h8_byte_t *byte);
void h8_sci3_ssr3_out(struct h8_system_t *system, h8_byte_t *byte,
                      const h8_byte_t value);
void h8_sci3_rdr3_in(struct h8_system_t *system, h8_byte_t *byte);

#endif
