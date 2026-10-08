#ifndef H8_IR_H
#define H8_IR_H

#include "types.h"

/**
 * IR link for communicating with another emulated system, ie. another
 * instance or something else entirely
 *
 * Every record on the connection is 8 bytes:
 *   [0]    'B' for an IR byte, 'T' for the end of a burst (packet)
 *   [1]    the IR byte, for 'B' records
 *   [2..3] 'I', 'R', so a receiver can find the record boundaries again
 *   [4..7] the sender's clock in CPU states, big-endian
 */

struct h8_system_t;

#define H8_IR_RECORD_SIZE 8
#define H8_IR_QUEUE_LEN 1024

typedef struct
{
  /** The sender's clock when the event happened */
  h8_u32 time;

  /** 'B' or 'T', as in the record type */
  h8_u8 type;

  h8_u8 value;
} h8_ir_event_t;

typedef struct
{
  /** Local clock, in CPU states */
  h8_u32 time;

  /** States until the network is next checked for incoming records */
  h8_u32 poll_countdown;

  /** When the last character was sent, and whether its burst is still open */
  h8_u32 tx_time;
  h8_bool tx_open;

  /** A record being reassembled from the incoming stream */
  h8_u8 record[H8_IR_RECORD_SIZE];
  unsigned record_len;

  /** Incoming characters and markers waiting to be delivered */
  h8_ir_event_t queue[H8_IR_QUEUE_LEN];
  unsigned head;
  unsigned tail;

  /** The number of end-of-burst markers in the queue */
  unsigned bursts;

  /** Whether a burst is being delivered */
  h8_bool delivering;

  /** Added to the sender's clock to get the local delivery time */
  h8_u32 offset;

  /**
   * The last character delivered, by the sender's clock and by the local
   * clock, so a burst that closely follows it keeps the sender's gap
   */
  h8_u32 last_sent;
  h8_u32 last_due;
  h8_bool have_last;
} h8_ir_t;

void h8_ir_init(h8_ir_t *ir);

/**
 * Advances the link by an amount of CPU time: closes finished bursts, checks
 * for incoming records, and delivers characters that are due.
 */
void h8_ir_run(struct h8_system_t *system, unsigned states);

/**
 * Sends a character that has just finished transmitting from the SCI3.
 */
void h8_ir_line_out(struct h8_system_t *system, h8_u8 value);

#endif
