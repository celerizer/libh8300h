#include "sci3.h"
#include "system.h"

/** SMR3 bits (14.3.5) */
#define H8_SMR_CHR 0x40
#define H8_SMR_PE 0x20
#define H8_SMR_STOP 0x08
#define H8_SMR_CKS 0x03

/** SSR3 bits (14.3.7) */
#define H8_SSR_TDRE 0x80
#define H8_SSR_RDRF 0x40
#define H8_SSR_TEND 0x04
#define H8_SSR_CLEARABLE 0xF8
#define H8_SSR_MPBT 0x01

/** SEMR (14.3.11) is at H'FFA6; the basic clock is 8x instead of 16x */
#define H8_REG_SEMR 0xFFA6
#define H8_SEMR_ABCS 0x08

h8_u32 h8_sci3_frame_states(const h8_system_t *system)
{
  const h8_aec_sci3_t *sci3 = &system->vmem.parts.io2.aec_sci3;
  h8_u8 smr = sci3->smr3.u;
  h8_u32 bits, cycles;

  /* Start bit, data bits, optional parity bit, and stop bits */
  bits = 1 + ((smr & H8_SMR_CHR) ? 7 : 8) + ((smr & H8_SMR_PE) ? 1 : 0) +
         ((smr & H8_SMR_STOP) ? 2 : 1);

  /* 14.3.8: B = clock / (32 * 2^(2n) * (N + 1)), or 16 instead of 32 */
  cycles = ((system->vmem.raw[H8_REG_SEMR].u & H8_SEMR_ABCS) ? 16 : 32) *
           ((h8_u32)sci3->brr3.u + 1) * bits;

  switch (smr & H8_SMR_CKS)
  {
  case 1:
    /* Counted on subclock, n = 0: convert watch clock cycles to CPU states */
    cycles = (h8_u32)((double)cycles * system->clock / H8_CLOCK_SUB);
    break;
  case 2:
    /* clock/16, n = 2 */
    cycles *= 16;
    break;
  case 3:
    /* clock/64, n = 3 */
    cycles *= 64;
    break;
  default:
    /* clock, n = 0 */
    break;
  }

  return cycles ? cycles : 1;
}

/** Moves the character in TDR into TSR and starts sending it */
static void h8_sci3_load(h8_system_t *system)
{
  h8_aec_sci3_t *sci3 = &system->vmem.parts.io2.aec_sci3;
  h8_sci3_state_t *state = &system->sci3;

  state->tsr = sci3->tdr3.u;
  state->tx_busy = TRUE;
  state->tx_remaining = h8_sci3_frame_states(system);

  /* 14.3.7: TDRE is set when data is transferred from TDR to TSR */
  sci3->ssr3.raw.u |= H8_SSR_TDRE;
}

void h8_sci3_run(h8_system_t *system, unsigned states)
{
  h8_aec_sci3_t *sci3 = &system->vmem.parts.io2.aec_sci3;
  h8_sci3_state_t *state = &system->sci3;

  while (state->tx_busy)
  {
    if (states < state->tx_remaining)
    {
      state->tx_remaining -= states;
      return;
    }
    states -= state->tx_remaining;

    /* The last bit of the character in TSR has been sent */
    h8_ir_line_out(system, state->tsr);

    if (!(sci3->ssr3.raw.u & H8_SSR_TDRE))
      /* TDR holds the next character, so transmission continues */
      h8_sci3_load(system);
    else
    {
      /* 14.3.7: TEND is set when TDRE = 1 as the last bit is sent */
      state->tx_busy = FALSE;
      sci3->ssr3.raw.u |= H8_SSR_TEND;
    }
  }
}

void h8_sci3_receive(h8_system_t *system, h8_u8 value)
{
  h8_aec_sci3_t *sci3 = &system->vmem.parts.io2.aec_sci3;

  if (!sci3->scr3.flags.re)
    return;

  /* 14.4: Reception does not continue while an error flag is set */
  if (sci3->ssr3.flags.oer || sci3->ssr3.flags.fer || sci3->ssr3.flags.per)
    return;

  /* 14.4: A character received while RDRF is still set is an overrun */
  if (sci3->ssr3.flags.rdrf)
  {
    sci3->ssr3.flags.oer = 1;
    return;
  }

  sci3->rdr3.u = value;
  sci3->ssr3.flags.rdrf = 1;
}

void h8_sci3_init(h8_system_t *system)
{
  system->sci3.tx_busy = FALSE;
  system->sci3.tsr = 0;
  system->sci3.tx_remaining = 0;
  system->sci3.ssr3_read = 0;
}

/**
 * Register handlers
 */

void h8_sci3_scr3_out(h8_system_t *system, h8_byte_t *byte,
                      const h8_byte_t value)
{
  h8_aec_sci3_t *sci3 = &system->vmem.parts.io2.aec_sci3;

  *byte = value;

  /* 14.3.7: With TE cleared, TDRE and TEND are set and transmission stops */
  if (!sci3->scr3.flags.te)
  {
    sci3->ssr3.raw.u |= H8_SSR_TDRE | H8_SSR_TEND;
    system->sci3.tx_busy = FALSE;
  }
}

void h8_sci3_tdr3_out(h8_system_t *system, h8_byte_t *byte,
                      const h8_byte_t value)
{
  h8_aec_sci3_t *sci3 = &system->vmem.parts.io2.aec_sci3;

  *byte = value;
  if (!sci3->scr3.flags.te)
    return;

  /* 14.3.7: Writing TDR clears TDRE and TEND */
  sci3->ssr3.raw.u &= (h8_u8)~(H8_SSR_TDRE | H8_SSR_TEND);
  if (!system->sci3.tx_busy)
    h8_sci3_load(system);
}

void h8_sci3_ssr3_in(h8_system_t *system, h8_byte_t *byte)
{
  system->sci3.ssr3_read = byte->u;
}

void h8_sci3_ssr3_out(h8_system_t *system, h8_byte_t *byte,
                      const h8_byte_t value)
{
  h8_u8 old = byte->u;
  h8_u8 cleared, now;

  /**
   * 14.3.7: TDRE, RDRF, OER, FER and PER are cleared by writing 0 after
   * reading 1. TEND and MPBR are read-only, and MPBT is writable.
   */
  cleared = (h8_u8)(old & system->sci3.ssr3_read & H8_SSR_CLEARABLE & ~value.u);
  now = (h8_u8)(((old & ~cleared) & ~H8_SSR_MPBT) | (value.u & H8_SSR_MPBT));
  system->sci3.ssr3_read = 0;

  /* Clearing TDRE also clears TEND, and marks TDR as holding data */
  if (cleared & H8_SSR_TDRE)
  {
    now &= (h8_u8)~H8_SSR_TEND;
    byte->u = now;
    if (system->vmem.parts.io2.aec_sci3.scr3.flags.te && !system->sci3.tx_busy)
      h8_sci3_load(system);
  }
  else
    byte->u = now;
}

void h8_sci3_rdr3_in(h8_system_t *system, h8_byte_t *byte)
{
  /* 14.3.7: Reading RDR clears RDRF */
  H8_UNUSED(byte);
  system->vmem.parts.io2.aec_sci3.ssr3.flags.rdrf = 0;
}
