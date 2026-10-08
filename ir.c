#include "ir.h"

#include "frontend.h"
#include "logger.h"
#include "system.h"

#define H8_IR_TYPE_CHAR 'B'
#define H8_IR_TYPE_END 'T'

/**
 * How long the sender's line must be quiet before the burst is closed, and
 * how often the network is checked, as fractions of a second. Closing a burst
 * only decides when the receiver may start delivering it; spacing within a
 * burst is always preserved.
 */
#define H8_IR_BURST_GAP_DIV 1000
#define H8_IR_POLL_DIV 2000

/**
 * Gaps between bursts shorter than this are kept, as a fraction of a second.
 * Longer ones are not, so a sender whose clock runs ahead of the receiver's
 * cannot push deliveries further and further into the future.
 */
#define H8_IR_KEEP_GAP_DIV 100

static void h8_ir_send(const h8_u8 type, const h8_u8 value, const h8_u32 time)
{
  h8_u8 record[H8_IR_RECORD_SIZE];

  record[0] = type;
  record[1] = value;
  record[2] = 'I';
  record[3] = 'R';
  record[4] = (h8_u8)(time >> 24);
  record[5] = (h8_u8)(time >> 16);
  record[6] = (h8_u8)(time >> 8);
  record[7] = (h8_u8)time;

  /* Fails harmlessly when nothing is connected, like IR with no partner */
  h8_fe_network_transmit(record, sizeof(record));
}

static void h8_ir_queue(h8_ir_t *ir, const h8_u8 *record)
{
  unsigned next = (ir->tail + 1) % H8_IR_QUEUE_LEN;
  h8_ir_event_t *event;

  if (next == ir->head)
  {
    h8_log(H8_LOG_WARN, H8_LOG_IR, "Receive queue full, dropping record");
    return;
  }

  event = &ir->queue[ir->tail];
  event->type = record[0];
  event->value = record[1];
  event->time = ((h8_u32)record[4] << 24) | ((h8_u32)record[5] << 16) |
                ((h8_u32)record[6] << 8) | (h8_u32)record[7];
  ir->tail = next;

  if (event->type == H8_IR_TYPE_END)
    ir->bursts++;
}

/** Reads any pending network data and reassembles it into records */
static void h8_ir_poll(h8_ir_t *ir)
{
  h8_u8 data[256];
  unsigned size;

  while ((size = h8_fe_network_receive(data, sizeof(data))) > 0)
  {
    unsigned i;

    for (i = 0; i < size; i++)
    {
      ir->record[ir->record_len++] = data[i];
      if (ir->record_len < H8_IR_RECORD_SIZE)
        continue;

      if ((ir->record[0] == H8_IR_TYPE_CHAR ||
           ir->record[0] == H8_IR_TYPE_END) &&
          ir->record[2] == 'I' && ir->record[3] == 'R')
      {
        h8_ir_queue(ir, ir->record);
        ir->record_len = 0;
      }
      else
      {
        /* Out of step with the stream: slide forward a byte and try again */
        unsigned j;

        for (j = 1; j < H8_IR_RECORD_SIZE; j++)
          ir->record[j - 1] = ir->record[j];
        ir->record_len--;
      }
    }
  }
}

/** Feeds characters from complete bursts to the SCI3 once they are due */
static void h8_ir_deliver(h8_system_t *system)
{
  h8_ir_t *ir = &system->ir;

  while (ir->head != ir->tail)
  {
    h8_ir_event_t *event = &ir->queue[ir->head];

    if (!ir->delivering)
    {
      /* Wait until the whole burst has arrived */
      if (!ir->bursts)
        return;

      /* The first character of the burst is due now; the rest keep their
       * spacing relative to it */
      ir->delivering = TRUE;
      ir->offset = ir->time - event->time;

      /* If the sender started this burst soon after the last one, keep that
       * gap: back-to-back packets would otherwise run together */
      if (ir->have_last)
      {
        h8_u32 gap = event->time - ir->last_sent;

        if (gap < system->clock / H8_IR_KEEP_GAP_DIV &&
            (h8_s32)(ir->last_due + gap - ir->time) > 0)
          ir->offset = ir->last_due + gap - event->time;
      }
    }

    if (event->type == H8_IR_TYPE_END)
    {
      ir->bursts--;
      ir->delivering = FALSE;
    }
    else
    {
      /* Wrap-safe check of whether the character is due yet */
      if ((h8_s32)(ir->time - (event->time + ir->offset)) < 0)
        return;
      if (system->vmem.parts.io2.aec_sci3.ircr.flags.enable)
        h8_sci3_receive(system, event->value);
      ir->last_sent = event->time;
      ir->last_due = event->time + ir->offset;
      ir->have_last = TRUE;
    }
    ir->head = (ir->head + 1) % H8_IR_QUEUE_LEN;
  }
}

void h8_ir_run(h8_system_t *system, unsigned states)
{
  h8_ir_t *ir = &system->ir;

  ir->time += states;

  /* Close the burst once the line has been quiet long enough */
  if (ir->tx_open &&
      ir->time - ir->tx_time >= system->clock / H8_IR_BURST_GAP_DIV)
  {
    h8_ir_send(H8_IR_TYPE_END, 0, ir->time);
    ir->tx_open = FALSE;
  }

  if (ir->poll_countdown > states)
    ir->poll_countdown -= states;
  else
  {
    ir->poll_countdown = system->clock / H8_IR_POLL_DIV;
    h8_ir_poll(ir);
  }

  h8_ir_deliver(system);
}

void h8_ir_line_out(h8_system_t *system, h8_u8 value)
{
  h8_ir_t *ir = &system->ir;

  /* Only the IrDA encoder drives the infrared transceiver */
  if (!system->vmem.parts.io2.aec_sci3.ircr.flags.enable)
    return;

  h8_ir_send(H8_IR_TYPE_CHAR, value, ir->time);
  ir->tx_time = ir->time;
  ir->tx_open = TRUE;
}

void h8_ir_init(h8_ir_t *ir)
{
  ir->time = 0;
  ir->poll_countdown = 0;
  ir->tx_time = 0;
  ir->tx_open = FALSE;
  ir->record_len = 0;
  ir->head = 0;
  ir->tail = 0;
  ir->bursts = 0;
  ir->delivering = FALSE;
  ir->offset = 0;
  ir->last_sent = 0;
  ir->last_due = 0;
  ir->have_last = FALSE;
}
