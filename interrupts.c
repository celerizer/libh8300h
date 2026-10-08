#include "interrupts.h"
#include "system.h"

/**
 * Returns the lowest set bit position of an 8-bit value, or 8 if none is set.
 */
static unsigned h8_lowest_bit(const h8_u8 value)
{
  unsigned i;

  for (i = 0; i < 8; i++)
    if (value & (1 << i))
      return i;

  return 8;
}

static h8_bool h8_ssu_requested(const h8_system_t *system)
{
  /* 15.4.11, table 15.3 */
  const h8_ssu_t *ssu = &system->vmem.parts.io1.ssu;

  return (ssu->sser.flags.tie && ssu->sssr.flags.tdre) ||
         (ssu->sser.flags.teie && ssu->sssr.flags.tend) ||
         (ssu->sser.flags.rie && (ssu->sssr.flags.rdrf ||
                                  ssu->sssr.flags.orer)) ||
         (ssu->sser.flags.ceie && ssu->sssr.flags.ce);
}

static h8_bool h8_sci3_requested(const h8_system_t *system)
{
  /* 14.7, table 14.13 */
  const h8_aec_sci3_t *sci3 = &system->vmem.parts.io2.aec_sci3;

  return (sci3->scr3.flags.tie && sci3->ssr3.flags.tdre) ||
         (sci3->scr3.flags.teie && sci3->ssr3.flags.tend) ||
         (sci3->scr3.flags.rie && (sci3->ssr3.flags.rdrf ||
                                   sci3->ssr3.flags.oer ||
                                   sci3->ssr3.flags.fer ||
                                   sci3->ssr3.flags.per));
}

/**
 * Reads the pin assigned to IRQ0 or IRQ1 (8.5.2, 8.6.2). Returns FALSE in
 * assigned if the pin is not set up as an interrupt input.
 */
static h8_bool h8_irq_pin(h8_system_t *system, const unsigned irq,
                          h8_bool *assigned)
{
  h8_u8 pfcr = system->vmem.raw[H8_REG_PFCR].u;
  h8_u8 select = irq ? (pfcr >> 2) & 3 : pfcr & 3;
  const h8_system_pin_in_t *pin;

  *assigned = TRUE;
  switch (select)
  {
  case 0:
    /* PB0 or PB1, only while PMRB selects its IRQ function */
    if (!(system->vmem.raw[H8_REG_PMRB].u & (irq ? H8_PMRB_IRQ1 :
                                                    H8_PMRB_IRQ0)))
      *assigned = FALSE;
    pin = &system->pdrb_in[irq];
    break;
  case 1:
    /* P92 or P93 */
    pin = &system->pdr9_in[irq ? 3 : 2];
    break;
  case 2:
    /* P30 or P11 */
    pin = irq ? &system->pdr1_in[1] : &system->pdr3_in[0];
    break;
  default:
    *assigned = FALSE;
    return FALSE;
  }

  return pin->device && pin->func ? pin->func(pin->device) != 0 : FALSE;
}

void h8_interrupt_pins(h8_system_t *system)
{
  unsigned irq;

  for (irq = 0; irq < 2; irq++)
  {
    h8_bool assigned;
    h8_bool level = h8_irq_pin(system, irq, &assigned);

    /* 3.4.1: IEGn selects a rising (1) or falling (0) edge */
    if (assigned && level != system->irq.pin_level[irq] &&
        level == ((system->vmem.raw[H8_REG_IEGR].u >> irq) & 1))
      system->vmem.raw[H8_REG_IRR1].u |= 1 << irq;
    system->irq.pin_level[irq] = level;
  }
}

unsigned h8_interrupt_pending(const h8_system_t *system)
{
  h8_u8 ienr1 = system->vmem.raw[H8_REG_IENR1].u;
  h8_u8 ienr2 = system->vmem.raw[H8_REG_IENR2].u;
  h8_u8 irr1 = system->vmem.raw[H8_REG_IRR1].u;
  h8_u8 irr2 = system->vmem.raw[H8_REG_IRR2].u;
  const h8_tw_t *tw = &system->vmem.parts.io1.tw;

  /* Checked in order of priority, from table 3.1 */
  if (irr1 & ienr1 & H8_IRQ0)
    return H8_VECTOR_IRQ0;
  if (irr1 & ienr1 & H8_IRQ1)
    return H8_VECTOR_IRQ1;
  if (irr1 & ienr1 & H8_IRQAEC)
    return H8_VECTOR_IRQAEC;

  /** @todo COMP0 and COMP1: comparator interrupts are not generated */

  /**
   * 11.3.8: RTCFLG bits 0 to 7 map to the 0.25-second to free-running overflow
   * vectors, in order. RTCCR2 decides which flags get set in the first place.
   */
  if (ienr1 & H8_IENRTC)
  {
    unsigned bit = h8_lowest_bit(system->vmem.raw[H8_REG_RTCFLG].u);

    if (bit < 8)
      return H8_VECTOR_RTC_QUARTER_SECOND + bit;
  }

  /** @todo WDT interval timer mode is not emulated */

  if (irr2 & ienr2 & H8_IRQEC)
    return H8_VECTOR_AEC;
  if (irr2 & ienr2 & H8_IRQTB1)
    return H8_VECTOR_TIMER_B1;
  if (h8_ssu_requested(system))
    return H8_VECTOR_SSU;
  if (tw->tsrw.raw.u & tw->tierw.raw.u & H8_TSRW_FLAGS)
    return H8_VECTOR_TIMER_W;
  if (h8_sci3_requested(system))
    return H8_VECTOR_SCI3;
  if (irr2 & ienr2 & H8_IRQAD)
    return H8_VECTOR_ADC;

  return 0;
}

void h8_interrupt_flag_out(h8_system_t *system, h8_byte_t *byte,
                           const h8_byte_t value)
{
  /** @todo Hardware only clears a flag that was read as 1 beforehand. */
  H8_UNUSED(system);
  byte->u &= value.u;
}
