#include "timer_w.h"
#include "system.h"

static h8_u16 h8_tw_get(const h8_word_be_t *reg)
{
  return (h8_u16)((reg->h.u << 8) | reg->l.u);
}

static void h8_tw_set(h8_word_be_t *reg, const h8_u16 value)
{
  reg->h.u = (h8_u8)(value >> 8);
  reg->l.u = (h8_u8)(value & 0xFF);
}

static const h8_word_be_t *h8_tw_gr(const h8_tw_t *tw,
                                    const unsigned channel)
{
  switch (channel)
  {
  case H8_TW_PIN_A:
    return &tw->gra;
  case H8_TW_PIN_B:
    return &tw->grb;
  case H8_TW_PIN_C:
    return &tw->grc;
  default:
    return &tw->grd;
  }
}

/** Returns the TIOR compare match output setting of a channel */
static unsigned h8_tw_io(const h8_tw_t *tw, const unsigned channel)
{
  switch (channel)
  {
  case H8_TW_PIN_A:
    return tw->tior0.flags.io_lo;
  case H8_TW_PIN_B:
    return tw->tior0.flags.io_hi;
  case H8_TW_PIN_C:
    return tw->tior1.flags.io_lo;
  default:
    return tw->tior1.flags.io_hi;
  }
}

/** Returns whether a channel's general register is set to input capture */
static h8_bool h8_tw_io2(const h8_tw_t *tw, const unsigned channel)
{
  switch (channel)
  {
  case H8_TW_PIN_A:
    return tw->tior0.flags.io2_lo;
  case H8_TW_PIN_B:
    return tw->tior0.flags.io2_hi;
  case H8_TW_PIN_C:
    return tw->tior1.flags.io2_lo;
  default:
    return tw->tior1.flags.io2_hi;
  }
}

/** Returns whether a channel outputs PWM. FTIOA has no PWM mode. */
static h8_bool h8_tw_pwm(const h8_tw_t *tw, const unsigned channel)
{
  switch (channel)
  {
  case H8_TW_PIN_B:
    return tw->tmrw.flags.pwmb;
  case H8_TW_PIN_C:
    return tw->tmrw.flags.pwmc;
  case H8_TW_PIN_D:
    return tw->tmrw.flags.pwmd;
  default:
    return FALSE;
  }
}

/** Returns the TCRW initial output level setting of a channel */
static h8_bool h8_tw_to(const h8_tw_t *tw, const unsigned channel)
{
  switch (channel)
  {
  case H8_TW_PIN_A:
    return tw->tcrw.flags.toa;
  case H8_TW_PIN_B:
    return tw->tcrw.flags.tob;
  case H8_TW_PIN_C:
    return tw->tcrw.flags.toc;
  default:
    return tw->tcrw.flags.tod;
  }
}

/**
 * Returns whether a channel's general register is compared against TCNT.
 * In PWM mode GRB to GRD act as output compare registers automatically.
 * GRC and GRD stop comparing when they act as buffers for GRA and GRB.
 */
static h8_bool h8_tw_compares(const h8_tw_t *tw, const unsigned channel)
{
  if ((channel == H8_TW_PIN_C && tw->tmrw.flags.bufea) ||
      (channel == H8_TW_PIN_D && tw->tmrw.flags.bufeb))
    return FALSE;
  else if (h8_tw_pwm(tw, channel))
    return TRUE;
  else
    return !h8_tw_io2(tw, channel);
}

/** Returns where the device hooked up to a timer pin is connected */
static h8_system_pin_out_t *h8_tw_pin_out(h8_system_t *system,
                                          const unsigned pin)
{
  switch (pin)
  {
  case H8_TW_PIN_A:
    return &system->pdr1_out[0];
  case H8_TW_PIN_B:
    return &system->pdr8_out[0];
  case H8_TW_PIN_C:
    return &system->pdr8_out[1];
  default:
    return &system->pdr8_out[2];
  }
}

/** Returns the level a pin has when its port data register drives it */
static h8_bool h8_tw_port_level(const h8_system_t *system, const unsigned pin)
{
  if (pin == H8_TW_PIN_A)
    return system->vmem.raw[H8_REG_PDR1].u & 1;
  else
    /* FTIOB to FTIOD are P82 to P84 */
    return (system->vmem.raw[H8_REG_PDR8].u >> (pin + 1)) & 1;
}

h8_bool h8_tw_drives_pin(const h8_system_t *system, unsigned pin)
{
  const h8_tw_t *tw = &system->vmem.parts.io1.tw;

  if (h8_tw_pwm(tw, pin))
    return TRUE;
  else if (h8_tw_io2(tw, pin))
    return FALSE;
  else
    return h8_tw_io(tw, pin) != H8_TIOR_OUTPUT_NONE;
}

void h8_tw_update_pins(h8_system_t *system)
{
  h8_tw_state_t *state = &system->timer_w;
  unsigned pin;

  for (pin = 0; pin < H8_TW_PIN_SIZE; pin++)
  {
    h8_system_pin_out_t *out = h8_tw_pin_out(system, pin);
    h8_bool driven = h8_tw_drives_pin(system, pin);
    h8_bool notify = FALSE;
    h8_bool level = FALSE;

    if (driven)
    {
      level = state->output[pin];
      if (!state->pin_driven[pin] || level != state->pin_level[pin])
      {
        state->pin_level[pin] = level;
        notify = TRUE;
      }
    }
    else if (state->pin_driven[pin])
    {
      /* The pin has returned to its port data register */
      level = h8_tw_port_level(system, pin);
      notify = TRUE;
    }
    state->pin_driven[pin] = driven;

    if (notify && out->device && out->func)
      out->func(out->device, level);
  }
}

/**
 * Clocks TCNT once, generating compare matches, flags, buffer transfers, and
 * output changes.
 */
static void h8_tw_count(h8_system_t *system)
{
  h8_tw_t *tw = &system->vmem.parts.io1.tw;
  h8_tw_state_t *state = &system->timer_w;
  h8_u16 tcnt = h8_tw_get(&tw->tcnt);
  unsigned matches = 0;
  unsigned channel;

  /**
   * 10.5.2: The compare match signal is generated in the last state in which
   * TCNT and the general register match, when TCNT is clocked to the next
   * value.
   */
  for (channel = 0; channel < H8_TW_PIN_SIZE; channel++)
    if (h8_tw_compares(tw, channel) &&
        tcnt == h8_tw_get(h8_tw_gr(tw, channel)))
      matches |= 1 << channel;

  /**
   * 10.5.4: When cleared by compare match A, the counter counts from 0 to
   * GRA, so its cycle is GRA + 1.
   */
  if ((matches & (1 << H8_TW_PIN_A)) && tw->tcrw.flags.cclr)
    tcnt = 0;
  else
  {
    if (tcnt == 0xFFFF)
      tw->tsrw.flags.ovf = 1;
    tcnt = (h8_u16)(tcnt + 1);
  }
  h8_tw_set(&tw->tcnt, tcnt);

  if (!matches)
    return;

  /* IMFA to IMFD share their bit positions with the channel numbers */
  tw->tsrw.raw.u |= matches;

  /* 10.3.8: Buffered values are transferred on compare match */
  if ((matches & (1 << H8_TW_PIN_A)) && tw->tmrw.flags.bufea)
    tw->gra = tw->grc;
  if ((matches & (1 << H8_TW_PIN_B)) && tw->tmrw.flags.bufeb)
    tw->grb = tw->grd;

  for (channel = 0; channel < H8_TW_PIN_SIZE; channel++)
  {
    h8_bool duty = (matches >> channel) & 1;

    if (h8_tw_pwm(tw, channel))
    {
      /**
       * 10.4.2: The output goes to TOx at compare match A and to the inverse
       * at the duty compare match. It does not change if both happen at once.
       */
      h8_bool period = matches & (1 << H8_TW_PIN_A);

      if (period && !duty)
        state->output[channel] = h8_tw_to(tw, channel);
      else if (duty && !period)
        state->output[channel] = !h8_tw_to(tw, channel);
    }
    else if (duty)
    {
      switch (h8_tw_io(tw, channel))
      {
      case H8_TIOR_OUTPUT_0:
        state->output[channel] = 0;
        break;
      case H8_TIOR_OUTPUT_1:
        state->output[channel] = 1;
        break;
      case H8_TIOR_OUTPUT_TOGGLE:
        state->output[channel] = !state->output[channel];
        break;
      default:
        break;
      }
    }
  }

  h8_tw_update_pins(system);
}

void h8_tw_run(h8_system_t *system, unsigned states, h8_bool phi_running,
               h8_bool sub_running)
{
  h8_tw_t *tw = &system->vmem.parts.io1.tw;
  h8_tw_state_t *state = &system->timer_w;

  /* The counter is halted in module standby or while CTS is clear */
  if (!(system->vmem.raw[H8_REG_CKSTPR2].u & H8_CKSTPR2_TWCKSTP) ||
      !tw->tmrw.flags.cts)
    return;

  switch (tw->tcrw.flags.cks)
  {
  case H8_TW_CLOCK_PHI:
  case H8_TW_CLOCK_PHI_2:
  case H8_TW_CLOCK_PHI_4:
  case H8_TW_CLOCK_PHI_8:
  {
    h8_u32 divider = 1 << tw->tcrw.flags.cks;

    if (!phi_running)
      return;
    state->prescaler += states;
    while (state->prescaler >= divider)
    {
      state->prescaler -= divider;
      h8_tw_count(system);
    }
    break;
  }
  case H8_TW_CLOCK_PHIW:
  case H8_TW_CLOCK_PHIW_4:
  case H8_TW_CLOCK_PHIW_16:
  {
    /* One count every (clock / subclock) * divider states */
    h8_u32 period = system->clock;

    if (!sub_running)
      return;
    if (tw->tcrw.flags.cks == H8_TW_CLOCK_PHIW_4)
      period *= 4;
    else if (tw->tcrw.flags.cks == H8_TW_CLOCK_PHIW_16)
      period *= 16;

    state->phase += states * H8_CLOCK_SUB;
    while (state->phase >= period)
    {
      state->phase -= period;
      h8_tw_count(system);
    }
    break;
  }
  default:
    /* Nothing is connected to FTCI to provide external clock edges */
    break;
  }
}

/**
 * Returns how many counts from now the next compare match or overflow
 * happens, which are the only times the outputs and flags can change.
 */
static h8_u32 h8_tw_counts_until_event(const h8_system_t *system)
{
  const h8_tw_t *tw = &system->vmem.parts.io1.tw;
  h8_u16 tcnt = h8_tw_get(&tw->tcnt);
  h8_u32 counts = (h8_u32)(0xFFFF - tcnt) + 1;
  unsigned channel;

  for (channel = 0; channel < H8_TW_PIN_SIZE; channel++)
    if (h8_tw_compares(tw, channel))
    {
      h8_u32 until = (h8_u32)((h8_tw_get(h8_tw_gr(tw, channel)) - tcnt) &
                              0xFFFF) + 1;

      if (until < counts)
        counts = until;
    }

  return counts;
}

h8_u32 h8_tw_states_until_event(const h8_system_t *system, h8_u32 max,
                                h8_bool phi_running, h8_bool sub_running)
{
  const h8_tw_t *tw = &system->vmem.parts.io1.tw;
  const h8_tw_state_t *state = &system->timer_w;
  h8_u32 counts, states;

  if (!(system->vmem.raw[H8_REG_CKSTPR2].u & H8_CKSTPR2_TWCKSTP) ||
      !tw->tmrw.flags.cts)
    return max;

  counts = h8_tw_counts_until_event(system);
  switch (tw->tcrw.flags.cks)
  {
  case H8_TW_CLOCK_PHI:
  case H8_TW_CLOCK_PHI_2:
  case H8_TW_CLOCK_PHI_4:
  case H8_TW_CLOCK_PHI_8:
  {
    h8_u32 divider = 1 << tw->tcrw.flags.cks;

    if (!phi_running || counts > max / divider + 1)
      return max;
    states = counts * divider - state->prescaler;
    break;
  }
  case H8_TW_CLOCK_PHIW:
  case H8_TW_CLOCK_PHIW_4:
  case H8_TW_CLOCK_PHIW_16:
  {
    /* Mirrors h8_tw_run: a count happens when phase reaches period */
    h8_u32 period = system->clock;

    if (!sub_running)
      return max;
    if (tw->tcrw.flags.cks == H8_TW_CLOCK_PHIW_4)
      period *= 4;
    else if (tw->tcrw.flags.cks == H8_TW_CLOCK_PHIW_16)
      period *= 16;
    if (counts > max / (period / H8_CLOCK_SUB + 1) + 1)
      return max;
    states = (counts * period - state->phase + H8_CLOCK_SUB - 1) /
             H8_CLOCK_SUB;
    break;
  }
  default:
    return max;
  }

  if (states < 1)
    states = 1;
  return states < max ? states : max;
}

void h8_tw_init(h8_system_t *system)
{
  h8_tw_t *tw = &system->vmem.parts.io1.tw;
  h8_tw_state_t *state = &system->timer_w;
  unsigned pin;

  tw->tmrw.raw.u = H8_TMRW_RESERVED;
  tw->tcrw.raw.u = 0;
  tw->tierw.raw.u = H8_TIERW_RESERVED;
  tw->tsrw.raw.u = H8_TSRW_RESERVED;
  tw->tior0.raw.u = H8_TIOR_RESERVED;
  tw->tior1.raw.u = H8_TIOR_RESERVED;
  h8_tw_set(&tw->tcnt, 0x0000);
  h8_tw_set(&tw->gra, 0xFFFF);
  h8_tw_set(&tw->grb, 0xFFFF);
  h8_tw_set(&tw->grc, 0xFFFF);
  h8_tw_set(&tw->grd, 0xFFFF);

  state->prescaler = 0;
  state->phase = 0;
  for (pin = 0; pin < H8_TW_PIN_SIZE; pin++)
  {
    state->output[pin] = FALSE;
    state->pin_driven[pin] = FALSE;
    state->pin_level[pin] = FALSE;
  }
}

/**
 * Register write handlers
 */

void h8_tw_tmrw_out(h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value)
{
  byte->u = value.u | H8_TMRW_RESERVED;
  h8_tw_update_pins(system);
}

void h8_tw_tcrw_out(h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value)
{
  const h8_tw_t *tw = &system->vmem.parts.io1.tw;
  unsigned pin;

  *byte = value;

  /* 10.3.2: Changing TOx is immediately reflected in the output value */
  for (pin = 0; pin < H8_TW_PIN_SIZE; pin++)
    system->timer_w.output[pin] = h8_tw_to(tw, pin);
  h8_tw_update_pins(system);
}

void h8_tw_tierw_out(h8_system_t *system, h8_byte_t *byte,
                     const h8_byte_t value)
{
  H8_UNUSED(system);
  byte->u = value.u | H8_TIERW_RESERVED;
}

void h8_tw_tsrw_out(h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value)
{
  /**
   * 10.3.4: Flags can only be cleared by writing 0.
   * @todo Hardware only clears a flag that was read as 1 beforehand.
   */
  H8_UNUSED(system);
  byte->u = (byte->u & value.u & H8_TSRW_FLAGS) | H8_TSRW_RESERVED;
}

void h8_tw_tior_out(h8_system_t *system, h8_byte_t *byte,
                    const h8_byte_t value)
{
  byte->u = value.u | H8_TIOR_RESERVED;
  h8_tw_update_pins(system);
}
