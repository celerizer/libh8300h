#include "comparator.h"
#include "system.h"

/** The A/D channel whose pin each comparator input shares (pins 28 and 27) */
static const unsigned h8_comparator_pins[2] = { H8_ADC_AN4, H8_ADC_AN5 };

/**
 * Returns whether the voltage on a comparator's input pin is above its
 * reference voltage (18.3.1, 18.3.2).
 */
static h8_bool h8_comparator_compare(h8_system_t *system, unsigned channel)
{
  h8_u8 cmcr = system->vmem.raw[H8_REG_CMCR0 + channel].u;
  h8_system_adc_t *pin =
    &system->adc[h8_comparator_pins[channel] - H8_ADC_AN0];
  h8_bool *output = &system->comparator.output[channel];
  h8_u32 level, reference;
  h8_word_t value;

  /* A halted comparator, or one in module standby, gives no result */
  if (!(system->vmem.raw[H8_REG_CKSTPR2].u & H8_CKSTPR2_COMPCKSTP) ||
      !(cmcr & H8_CMCR_CME))
  {
    *output = FALSE;
    return FALSE;
  }

  /** @todo Nothing is connected to the external VCref pin */
  if (cmcr & H8_CMCR_CMR)
  {
    *output = FALSE;
    return FALSE;
  }

  /* The pin voltage, as a fraction of Vcc in 1/1024ths */
  if (!pin->device || !pin->func)
    return *output = FALSE;
  value = pin->func(pin->device);
  level = value.u >> 6;

  /**
   * The internal reference is (11 + CRS) / 30 of Vcc. With hysteresis, a
   * result of 1 stays until the voltage falls to (9 + CRS) / 30 instead.
   */
  reference = 11 + (cmcr & H8_CMCR_CRS);
  if ((cmcr & H8_CMCR_CMLS) && *output)
    reference -= 2;
  *output = level * 30 > reference * 1024;

  return *output;
}

void h8_comparator_cmdr_in(h8_system_t *system, h8_byte_t *byte)
{
  byte->u &= H8_CMDR_CMF0 | H8_CMDR_CMF1;
  if (h8_comparator_compare(system, 0))
    byte->u |= H8_CMDR_CDR0;
  if (h8_comparator_compare(system, 1))
    byte->u |= H8_CMDR_CDR1;
}

void h8_comparator_cmdr_out(h8_system_t *system, h8_byte_t *byte,
                            const h8_byte_t value)
{
  H8_UNUSED(system);
  byte->u &= value.u & (H8_CMDR_CMF0 | H8_CMDR_CMF1);
}

void h8_comparator_init(h8_system_t *system)
{
  system->vmem.raw[H8_REG_CMCR0].u = 0;
  system->vmem.raw[H8_REG_CMCR1].u = 0;
  system->vmem.raw[H8_REG_CMDR].u = 0;
  system->comparator.output[0] = FALSE;
  system->comparator.output[1] = FALSE;
}
