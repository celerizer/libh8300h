#include "timer_b1.h"
#include "system.h"

/** Clocks a counter by one input, handling overflow */
static void h8_tb1_count(h8_system_t *system)
{
  h8_tb1_state_t *state = &system->timer_b1;

  state->tcb1++;
  if (!state->tcb1)
  {
    /* 9.2.2: Overflow requests an interrupt and, in auto-reload, reloads */
    if (system->vmem.raw[H8_REG_TMB1].u & H8_TMB1_RELOAD)
      state->tcb1 = state->tlb1;
    system->vmem.raw[H8_REG_IRR2].u |= H8_IRQTB1;
  }
}

void h8_tb1_run(h8_system_t *system, unsigned states, h8_bool phi_running,
                h8_bool sub_running)
{
  h8_tb1_state_t *state = &system->timer_b1;
  h8_u8 tmb1 = system->vmem.raw[H8_REG_TMB1].u;
  h8_u8 cks = tmb1 & H8_TMB1_CKS;

  if (!(system->vmem.raw[H8_REG_CKSTPR1].u & H8_CKSTPR1_TB1CKSTP) ||
      !(tmb1 & H8_TMB1_RUN))
    return;

  if (cks < 6)
  {
    /* 9.2.1: clock/8192, clock/2048, clock/256, clock/64, clock/16, and clock/4 */
    static const h8_u16 dividers[6] = { 8192, 2048, 256, 64, 16, 4 };

    if (!phi_running)
      return;
    state->prescaler += states;
    while (state->prescaler >= dividers[cks])
    {
      state->prescaler -= dividers[cks];
      h8_tb1_count(system);
    }
  }
  else
  {
    /* subclock/1024 or subclock/256: one count every (clock / subclock) * divider states */
    h8_u32 period = system->clock * (cks == 6 ? 1024 : 256);

    if (!sub_running)
      return;
    state->phase += states * H8_CLOCK_SUB;
    while (state->phase >= period)
    {
      state->phase -= period;
      h8_tb1_count(system);
    }
  }
}

void h8_tb1_init(h8_system_t *system)
{
  system->vmem.raw[H8_REG_TMB1].u = H8_TMB1_RESERVED;
  system->timer_b1.tcb1 = 0;
  system->timer_b1.tlb1 = 0;
  system->timer_b1.prescaler = 0;
  system->timer_b1.phase = 0;
}

/**
 * Register handlers
 */

void h8_tb1_tmb1_out(h8_system_t *system, h8_byte_t *byte,
                     const h8_byte_t value)
{
  H8_UNUSED(system);
  byte->u = value.u | H8_TMB1_RESERVED;
}

void h8_tb1_tcb1_in(h8_system_t *system, h8_byte_t *byte)
{
  byte->u = system->timer_b1.tcb1;
}

void h8_tb1_tlb1_out(h8_system_t *system, h8_byte_t *byte,
                     const h8_byte_t value)
{
  /* 9.2.3: Writing TLB1 also loads the value into TCB1 */
  system->timer_b1.tlb1 = value.u;
  system->timer_b1.tcb1 = value.u;
  byte->u = value.u;
}
