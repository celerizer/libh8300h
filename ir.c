#include "ir.h"

#include "frontend.h"
#include "logger.h"

/** @todo remove */
#include <stdio.h>

h8_bool h8_ir_out(h8_ir_t *ir, h8_byte_t out)
{
  if (ir->tx_len < H8_IR_BUFFER_LEN)
  {
    ir->tx[ir->tx_len] = out;
    ir->tx_len++;

    return TRUE;
  }
  else
    return FALSE;
}

h8_bool h8_ir_in(h8_ir_t *ir, h8_byte_t *in)
{
  if (ir->rx_len > 0)
  {
    unsigned i;

    *in = ir->rx[0];
    ir->rx_len--;
    for (i = 0; i < ir->rx_len; i++)
      ir->rx[i] = ir->rx[i + 1];

    return TRUE;
  }
  else
    return FALSE;
}

void h8_ir_receive(h8_ir_t *ir)
{
  char log[32];
  unsigned i;

  log[0] = '\0';
  for (i = 0; i < ir->rx_len; i++)
    snprintf(log, sizeof(log), "%s%02X", log, ir->rx[i].u);

  h8_log(H8_LOG_WARN, H8_LOG_IR, "Unimplemented receive: %u <- %s",
                                 ir->rx_len, log);
  h8_fe_network_receive(ir->rx, ir->rx_len);
  ir->rx_len = 0;
}

void h8_ir_transmit(h8_ir_t *ir)
{
  char log[32];
  unsigned i;

  log[0] = '\0';
  for (i = 0; i < ir->tx_len; i++)
    snprintf(log, sizeof(log), "%s%02X", log, ir->tx[i].u);

  h8_log(H8_LOG_WARN, H8_LOG_IR, "Unimplemented transmit: %u -> %s",
                                 ir->tx_len, log);
  h8_fe_network_transmit(ir->tx, ir->tx_len);
  ir->tx_len = 0;
}
