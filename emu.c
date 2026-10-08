#include "logger.h"
#include "system.h"

#include <string.h>

#define H8_DEBUG_PRINT_FETCH 0
#define H8_DEBUG_PRINT_REGISTERS 0

#define H8_ERROR(a) \
{ \
  system->error_code = a; \
  system->error_line = __LINE__; \
}

#define H8_OP(a) void a(h8_system_t *system)
typedef void (*H8_OP_T)(h8_system_t*);

/** A macro to unroll opcodes that take a Rd in AL into 16 functions */
#define H8_UNROLL(op) \
  op(0, system->cpu.regs[0].byte.rh) \
  op(1, system->cpu.regs[1].byte.rh) \
  op(2, system->cpu.regs[2].byte.rh) \
  op(3, system->cpu.regs[3].byte.rh) \
  op(4, system->cpu.regs[4].byte.rh) \
  op(5, system->cpu.regs[5].byte.rh) \
  op(6, system->cpu.regs[6].byte.rh) \
  op(7, system->cpu.regs[7].byte.rh) \
  op(8, system->cpu.regs[0].byte.rl) \
  op(9, system->cpu.regs[1].byte.rl) \
  op(a, system->cpu.regs[2].byte.rl) \
  op(b, system->cpu.regs[3].byte.rl) \
  op(c, system->cpu.regs[4].byte.rl) \
  op(d, system->cpu.regs[5].byte.rl) \
  op(e, system->cpu.regs[6].byte.rl) \
  op(f, system->cpu.regs[7].byte.rl)

/**
 * Updates the zero and negative flags based on a given value, and sets
 * overflow flag to 0.
 */
void ccr_zn(h8_system_t *system, signed val)
{
  system->cpu.ccr.flags.z = val == 0;
  system->cpu.ccr.flags.n = val < 0;
  system->cpu.ccr.flags.v = 0;
}

void ccr_v(h8_system_t *system, signed a, signed b, signed c)
{
  system->cpu.ccr.flags.v = ((a >= 0 && b >= 0 && c < 0) ||
                            (a < 0 && b < 0 && c >= 0));
}

/**
 * =============================================================================
 * Assembly function implementations
 * =============================================================================
 */

#define H8_MOV_OP(name, type, hcbit) \
/** Moves a type from one location to another */ \
type mov_##name(h8_system_t *system, type dst, const type src) \
{ \
  H8_UNUSED(dst); \
  ccr_zn(system, src.i); \
  return src; \
}
H8_MOV_OP(b, h8_byte_t, 4)
H8_MOV_OP(w, h8_word_t, 12)
H8_MOV_OP(l, h8_long_t, 28)

#define H8_ADD_OP(name, type, hcbit) \
type add_##name(h8_system_t *system, type dst, const type src) \
{ \
  type result; \
  result.i = dst.i + src.i; \
  system->cpu.ccr.flags.c = result.u < dst.u; \
  ccr_zn(system, result.i); \
  ccr_v(system, dst.i, src.i, result.i); \
  system->cpu.ccr.flags.h = (result.u & (1 << hcbit)) != (dst.u & (1 << hcbit)); \
  return result; \
}
H8_ADD_OP(b, h8_byte_t, 4)
H8_ADD_OP(w, h8_word_t, 12)
H8_ADD_OP(l, h8_long_t, 28)

/** Add immediate without modifying status bits */
void adds_l(h8_system_t *system, h8_long_t *dst, const unsigned src)
{
  H8_UNUSED(system);
  dst->u = dst->u + src;
}

#define H8_SUB_OP(name, type, hcbit) \
type sub_##name(h8_system_t *system, type dst, const type src) \
{ \
  type result; \
  result.i = dst.i - src.i; \
  system->cpu.ccr.flags.c = src.u > dst.u; \
  ccr_zn(system, result.i); \
  ccr_v(system, dst.i, -src.i, result.i); \
  system->cpu.ccr.flags.h = (result.u & (1 << hcbit)) != (dst.u & (1 << hcbit)); \
  return result; \
}
H8_SUB_OP(b, h8_byte_t, 4)
H8_SUB_OP(w, h8_word_t, 12)
H8_SUB_OP(l, h8_long_t, 28)

/** Subtract immediate without modifying status bits */
void subs_l(h8_system_t *system, h8_long_t *dst, const int src)
{
  H8_UNUSED(system);
  dst->i = dst->i - src;
}

static void daa_b(h8_system_t *system, h8_byte_t *dst)
{
  h8_u8 adjust = 0;
  h8_bool carry_out = 0;

  if ((dst->u & 0x0F) > 9 || system->cpu.ccr.flags.h)
    adjust |= 0x06;
  if (((dst->u & 0xF0) >> 4) > 9 || system->cpu.ccr.flags.c)
  {
    adjust |= 0x60;
    carry_out = 1;
  }
  dst->u += adjust;

  system->cpu.ccr.flags.c = carry_out;
  system->cpu.ccr.flags.h = ((adjust & 0x06) != 0);
  ccr_zn(system, dst->i);
}

/**
 * Decimal adjust after subtraction, using C and H from the preceding SUB,
 * SUBX or NEG. C is left unchanged.
 */
static void das_b(h8_system_t *system, h8_byte_t *dst)
{
  h8_u8 adjust = 0;

  if (system->cpu.ccr.flags.h)
    adjust |= 0x06;
  if (system->cpu.ccr.flags.c)
    adjust |= 0x60;
  dst->u = (h8_u8)(dst->u - adjust);
  ccr_zn(system, dst->i);
}

static void extu_w(h8_system_t *system, h8_word_t *dst)
{
  dst->h.u = 0;
  system->cpu.ccr.flags.n = 0;
  system->cpu.ccr.flags.z = dst->u == 0;
  system->cpu.ccr.flags.v = 0;
}

static void extu_l(h8_system_t *system, h8_long_t *dst)
{
  dst->h.u = 0;
  system->cpu.ccr.flags.n = 0;
  system->cpu.ccr.flags.z = dst->u == 0;
  system->cpu.ccr.flags.v = 0;
}

static void exts_w(h8_system_t *system, h8_word_t *dst)
{
  if (dst->l.u & B10000000)
  {
    dst->h.u = 0xFF;
    system->cpu.ccr.flags.n = 1;
  }
  else
  {
    dst->h.u = 0x00;
    system->cpu.ccr.flags.n = 0;
  }
  system->cpu.ccr.flags.z = dst->u == 0;
  system->cpu.ccr.flags.v = 0;
}

static void exts_l(h8_system_t *system, h8_long_t *dst)
{
  if (dst->c.u & B10000000)
  {
    dst->h.u = 0xFFFF;
    system->cpu.ccr.flags.n = 1;
  }
  else
  {
    dst->h.u = 0x0000;
    system->cpu.ccr.flags.n = 0;
  }
  system->cpu.ccr.flags.z = dst->u == 0;
  system->cpu.ccr.flags.v = 0;
}

#define H8_ROTL_OP(name, type, carry) \
static void rotl_##name(h8_system_t *system, type *dst) \
{ \
  h8_bool msb = (dst->u & carry) ? 1 : 0; \
  dst->u <<= 1; \
  dst->u |= msb; \
  system->cpu.ccr.flags.c = msb; \
  ccr_zn(system, dst->i); \
}
H8_ROTL_OP(b, h8_byte_t, 0x80)
H8_ROTL_OP(w, h8_word_t, 0x8000)
H8_ROTL_OP(l, h8_long_t, 0x80000000)

#define H8_ROTR_OP(name, type, carry) \
static void rotr_##name(h8_system_t *system, type *dst) \
{ \
  unsigned lsb = (dst->u & 1) ? carry : 0; \
  system->cpu.ccr.flags.c = (dst->u & 1); \
  dst->u >>= 1; \
  dst->u |= lsb; \
  ccr_zn(system, dst->i); \
}
H8_ROTR_OP(b, h8_byte_t, 0x80)
H8_ROTR_OP(w, h8_word_t, 0x8000)
H8_ROTR_OP(l, h8_long_t, 0x80000000)

#define H8_ROTXL_OP(name, type, carry) \
static void rotxl_##name(h8_system_t *system, type *dst) \
{ \
  h8_bool msb = (dst->u & carry) ? 1 : 0; \
  dst->u <<= 1; \
  dst->u |= system->cpu.ccr.flags.c; \
  system->cpu.ccr.flags.c = msb; \
  ccr_zn(system, dst->i); \
}
H8_ROTXL_OP(b, h8_byte_t, 0x80)
H8_ROTXL_OP(w, h8_word_t, 0x8000)
H8_ROTXL_OP(l, h8_long_t, 0x80000000)

#define H8_ROTXR_OP(name, type, carry) \
static void rotxr_##name(h8_system_t *system, type *dst) \
{ \
  h8_bool lsb = (dst->u & 1); \
  dst->u >>= 1; \
  dst->u |= (system->cpu.ccr.flags.c ? carry : 0); \
  system->cpu.ccr.flags.c = lsb; \
  ccr_zn(system, dst->i); \
}
H8_ROTXR_OP(b, h8_byte_t, 0x80)
H8_ROTXR_OP(w, h8_word_t, 0x8000)
H8_ROTXR_OP(l, h8_long_t, 0x80000000)

#define H8_SHAL_OP(name, type, carry) \
static void shal_##name(h8_system_t *system, type *dst) \
{ \
  system->cpu.ccr.flags.c = (dst->u & carry) ? 1 : 0; \
  dst->u <<= 1; \
  ccr_zn(system, dst->i); \
}
H8_SHAL_OP(b, h8_byte_t, 0x80)
H8_SHAL_OP(w, h8_word_t, 0x8000)
H8_SHAL_OP(l, h8_long_t, 0x80000000)

#define H8_SHAR_OP(name, type, carry) \
static void shar_##name(h8_system_t *system, type *dst) \
{ \
  type sign; \
  sign.u = dst->u & carry; \
  system->cpu.ccr.flags.c = (dst->u & 1) ? 1 : 0; \
  dst->u >>= 1; \
  dst->u = sign.u | (dst->u & ~carry); \
  ccr_zn(system, dst->i); \
}
H8_SHAR_OP(b, h8_byte_t, 0x80)
H8_SHAR_OP(w, h8_word_t, 0x8000)
H8_SHAR_OP(l, h8_long_t, 0x80000000)

#define H8_SHL_OP(name, type, carry, direction, op) \
static void shl##direction##_##name(h8_system_t *system, type *dst) \
{ \
  system->cpu.ccr.flags.c = (dst->u & carry) ? 1 : 0; \
  dst->u op 1; \
  ccr_zn(system, dst->i); \
}
H8_SHL_OP(b, h8_byte_t, 0x80, l, <<=)
H8_SHL_OP(w, h8_word_t, 0x8000, l, <<=)
H8_SHL_OP(l, h8_long_t, 0x80000000, l, <<=)
H8_SHL_OP(b, h8_byte_t, 0x01, r, >>=)
H8_SHL_OP(w, h8_word_t, 0x0001, r, >>=)
H8_SHL_OP(l, h8_long_t, 0x00000001, r, >>=)

/**
 * Register IO functions
 */

#define H8_IO_DUMMY_IN H8_UNUSED(system); H8_UNUSED(byte);
#define H8_IO_DUMMY_OUT H8_UNUSED(system); H8_UNUSED(byte); H8_UNUSED(value);

H8_IN(pdr1i)
{
  unsigned i;

  for (i = 0; i < 3; i++)
  {
    if (system->pdr1_in[i].device && system->pdr1_in[i].func)
    {
      byte->u &= ~(1 << i);
      byte->u |= (system->pdr1_in[i].func(system->pdr1_in[i].device) & 0x01) << i;
    }
  }
}

H8_OUT(pdr1o)
{
  unsigned i;

  *byte = value;
  for (i = 0; i < 3; i++)
  {
    /* P10 is FTIOA, which Timer W can take over */
    if (i == 0 && h8_tw_drives_pin(system, H8_TW_PIN_A))
      continue;
    if (system->pdr1_out[i].device && system->pdr1_out[i].func)
      system->pdr1_out[i].func(system->pdr1_out[i].device, (value.u >> i) & 1);
  }
}

H8_IN(pdr3i)
{
  unsigned i;

  for (i = 0; i < 3; i++)
  {
    if (system->pdr3_in[i].device && system->pdr3_in[i].func)
    {
      byte->u &= ~(1 << i);
      byte->u |= (system->pdr3_in[i].func(system->pdr3_in[i].device) & 0x01) << i;
    }
  }
}

H8_OUT(pdr3o)
{
  unsigned i;

  for (i = 0; i < 3; i++)
    if (system->pdr3_out[i].device && system->pdr3_out[i].func)
      system->pdr3_out[i].func(system->pdr3_out[i].device, (value.u >> i) & 1);

  *byte = value;
}

/**
 * 8.3.1 Port Data Register 8 (PDR8)
 */

H8_IN(pdr8i)
{
  unsigned i;

  for (i = 0; i < 3; i++)
  {
    if (system->pdr8_in[i].device && system->pdr8_in[i].func)
    {
      byte->u &= ~(1 << (i + 2));
      byte->u |= (system->pdr8_in[i].func(system->pdr8_in[i].device) & 0x01) << (i + 2);
    }
  }
}

H8_OUT(pdr8o)
{
  unsigned i;

  *byte = value;
  for (i = 0; i < 3; i++)
  {
    /* P82 to P84 are FTIOB to FTIOD, which Timer W can take over */
    if (h8_tw_drives_pin(system, H8_TW_PIN_B + i))
      continue;
    if (system->pdr8_out[i].device && system->pdr8_out[i].func)
      system->pdr8_out[i].func(system->pdr8_out[i].device, (value.u >> (i + 2)) & 1);
  }
}

H8_IN(pdr9i)
{
  unsigned i;

  for (i = 0; i < 4; i++)
  {
    if (system->pdr9_in[i].device && system->pdr9_in[i].func)
    {
      byte->u &= ~(1 << i);
      byte->u |= (system->pdr9_in[i].func(system->pdr9_in[i].device) & 0x01) << i;
    }
  }
}

H8_OUT(pdr9o)
{
  unsigned i;

  for (i = 0; i < 4; i++)
    if (system->pdr9_out[i].device && system->pdr9_out[i].func)
      system->pdr9_out[i].func(system->pdr9_out[i].device, (value.u >> i) & 1);

  *byte = value;
}

/**
 * 8.5.1 Port Data Register B (PDRB)
 */

H8_IN(pdrbi)
{
  unsigned i;

  for (i = 0; i < 6; i++)
  {
    if (system->pdrb_in[i].device && system->pdrb_in[i].func)
    {
      byte->u &= ~(1 << i);
      byte->u |= (system->pdrb_in[i].func(system->pdrb_in[i].device) & 1) << i;
    }
  }
}

H8_OUT(pdrbo)
{
  /* Read-only */
  H8_IO_DUMMY_OUT
}

/**
 * 15.3.5 SS Status Register (SSSR)
 * F0E4
 */

H8_IN(sssri)
{
  system->vmem.parts.io1.ssu.sssr.flags.tend = 1;
  system->vmem.parts.io1.ssu.sssr.flags.tdre = 1;
  system->vmem.parts.io1.ssu.sssr.flags.rdrf = 1;
  *byte = system->vmem.parts.io1.ssu.sssr.raw;
}

H8_OUT(sssro)
{
  h8_sssr_t *sssr = (h8_sssr_t*)byte;
  const h8_sssr_t *sssr_val = (h8_sssr_t*)&value;

  H8_UNUSED(system);

  if (!sssr_val->flags.ce)
    sssr->flags.ce = 0;
  if (!sssr_val->flags.rdrf)
    sssr->flags.rdrf = 0;
  if (!sssr_val->flags.tdre)
    sssr->flags.tdre = 0;
  if (!sssr_val->flags.tend)
    sssr->flags.tend = 0;
  if (!sssr_val->flags.orer)
    sssr->flags.orer = 0;
}

/**
 * 15.3.6 SS Receive Data Register (SSRDR)
 * F0E9
 */

H8_IN(ssrdri)
{
  unsigned i;

  for (i = 0; i < system->device_count; i++)
    if (system->devices[i].ssu_in)
      system->devices[i].ssu_in(&system->devices[i], byte);
}

H8_OUT(ssrdro)
{
  /* Read-only */
  H8_IO_DUMMY_OUT
}

/**
 * 15.3.7 SS Transmit Data Register (SSTDR)
 * F0EB
 * @todo SSMR controls flip-on-store
 */

H8_OUT(sstdro)
{
  unsigned i;

  for (i = 0; i < system->device_count; i++)
    if (system->devices[i].ssu_out)
      system->devices[i].ssu_out(&system->devices[i], byte, value);
}

/**
 * 15.3.8 SS Shift Register (SSTRSR)
 * ????
 */

H8_IN(adsri)
{
  /** @todo Conversion finishes as soon as it starts, so ADSF reads as 0 */
  *byte = system->vmem.parts.io2.adc.adsr.raw;
}

/**
 * 17.4.1: Converts the selected channel, stores the result in ADRR and sets
 * IRRAD. Channels with nothing connected read as 0.
 */
static void h8_adc_convert(h8_system_t *system)
{
  unsigned channel = system->vmem.parts.io2.adc.amr.flags.ch;
  h8_word_t result;

  result.u = 0;
  if (channel >= H8_ADC_AN0 && channel < H8_ADC_MAX)
  {
    h8_system_adc_t *adc = &system->adc[channel - H8_ADC_AN0];

    if (adc->device && adc->func)
      result = adc->func(adc->device);
  }

  /* Only the upper 10 bits hold the result */
  system->vmem.parts.io2.adc.adrr.raw.h = result.h;
  system->vmem.parts.io2.adc.adrr.raw.l.u = result.l.u & 0xC0;
  system->vmem.raw[H8_REG_IRR2].u |= H8_IRQAD;
}

H8_OUT(adrrho)
{
  H8_IO_DUMMY_OUT
}

H8_OUT(adrrlo)
{
  H8_IO_DUMMY_OUT
}

H8_OUT(amro)
{
  /* 17.3.2: Bit 7 is reserved and always reads as 0 */
  H8_UNUSED(system);
  byte->u = value.u & 0x7F;
}

H8_OUT(adsro)
{
  h8_adsr_t adsr;

  adsr.raw = value;
  adsr.flags.reserved = B00111111;
  if (adsr.flags.adsf)
  {
    /* Complete the conversion immediately, which clears ADSF */
    h8_adc_convert(system);
    adsr.flags.adsf = 0;
  }

  *byte = adsr.raw;
}

static H8_IN_T reg_ins[0x160] =
{
  /* IO region 1 (0xF020) */
  /* 0xF020 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF030 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF040 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF050 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF060 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF070 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF080 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF090 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0A0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0B0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0C0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0D0 */
  NULL, h8_tb1_tcb1_in, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, h8_comparator_cmdr_in, NULL,
  /* 0xF0E0 */
  NULL, NULL, NULL, NULL, sssri, NULL, NULL, NULL,
  NULL, ssrdri, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0F0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

  /* IO region 2 (0xFF80) */
  /* 0xFF80 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFF90 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, h8_sci3_ssr3_in, h8_sci3_rdr3_in, NULL, NULL,
  /* 0xFFA0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFB0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, adsri,
  /* 0xFFC0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFD0 */
  NULL, NULL, NULL, NULL, pdr1i, NULL, pdr3i, NULL,
  NULL, NULL, NULL, pdr8i, pdr9i, NULL, pdrbi, NULL,
  /* 0xFFE0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFF0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static H8_OUT_T reg_outs[0x160] =
{
  /* IO region 1 (0xF020) */
  /* 0xF020 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF030 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF040 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF050 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF060 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, h8_interrupt_flag_out,
  NULL, NULL, NULL, NULL, h8_rtc_rtccr1_out, NULL, NULL, NULL,
  /* 0xF070 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF080 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF090 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0A0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0B0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0C0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0D0 */
  h8_tb1_tmb1_out, h8_tb1_tlb1_out, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, h8_comparator_cmdr_out, NULL,
  /* 0xF0E0 */
  NULL, NULL, NULL, NULL, sssro, NULL, NULL, NULL,
  NULL, ssrdro, NULL, sstdro, NULL, NULL, NULL, NULL,
  /* 0xF0F0 */
  h8_tw_tmrw_out, h8_tw_tcrw_out, h8_tw_tierw_out, h8_tw_tsrw_out,
  h8_tw_tior_out, h8_tw_tior_out, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

  /* IO region 2 (0xFF80) */
  /* 0xFF80 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFF90 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, h8_sci3_scr3_out, h8_sci3_tdr3_out, h8_sci3_ssr3_out, NULL,
  NULL, NULL,
  /* 0xFFA0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFB0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, adrrho, adrrlo, amro, adsro,
  /* 0xFFC0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFD0 */
  NULL, NULL, NULL, NULL, pdr1o, NULL, pdr3o, NULL,
  NULL, NULL, NULL, pdr8o, pdr9o, NULL, pdrbo, NULL,
  /* 0xFFE0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFF0 */
  NULL, NULL, NULL, NULL, NULL, NULL, h8_interrupt_flag_out,
  h8_interrupt_flag_out,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

unsigned h8_read(const h8_system_t *system, void *buffer,
                 const unsigned address, unsigned size)
{
  if (address >= sizeof(system->vmem))
    return 0;
  else
  {
    unsigned max_size = sizeof(system->vmem) - address;

    if (size > max_size)
      size = max_size;
    memcpy(buffer, &system->vmem.raw[address], size);

    return size;
  }
}

unsigned h8_write(h8_system_t *system, const void *buffer,
                  const unsigned address, const unsigned size,
                  const h8_bool force)
{
  if ((address >= 0xf780 && address < 0xff80) || force)
  {
    memcpy(&system->vmem.raw[address], buffer, size);
    return size;
  }
  else
    return 0;
}

void *h8_find(h8_system_t *system, unsigned address)
{
#if H8_PROFILING
  /*if (address >= 0xf780 && address < 0xff80)*/
    system->reads[address & 0xFFFF] += 1;
#endif
  return &system->vmem.raw[address & 0xFFFF];
}

static H8_IN_T h8_register_in(h8_system_t *system, unsigned address)
{
  if (address >= H8_MEMORY_REGION_IO1 &&
      address < H8_MEMORY_REGION_IO1 + sizeof(system->vmem.parts.io1))
  {
#if H8_DEBUG_PRINT_REGISTERS
    if (!reg_ins[address - H8_MEMORY_REGION_IO1])
      h8_log(H8_LOG_WARN, H8_LOG_CPU, "%04X input not implemented.", address);
#endif
    return reg_ins[address - H8_MEMORY_REGION_IO1];
  }
  else if (address >= H8_MEMORY_REGION_IO2 &&
           address < H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io2))
  {
#if H8_DEBUG_PRINT_REGISTERS
    if (!reg_ins[address - H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io1)])
      h8_log(H8_LOG_WARN, H8_LOG_CPU, "%04X input not implemented.", address);
#endif
    return reg_ins[address - H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io1)];
  }
  else
    return NULL;
}

static H8_OUT_T h8_register_out(h8_system_t *system, unsigned address)
{
  if (address >= H8_MEMORY_REGION_IO1 &&
      address < H8_MEMORY_REGION_IO1 + sizeof(system->vmem.parts.io1))
  {
#if H8_DEBUG_PRINT_REGISTERS
    if (!reg_outs[address - H8_MEMORY_REGION_IO1])
      printf("Better not implement %04X output, BOY\n", address);
#endif
    return reg_outs[address - H8_MEMORY_REGION_IO1];
  }
  else if (address >= H8_MEMORY_REGION_IO2 &&
           address < H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io2))
  {
#if H8_DEBUG_PRINT_REGISTERS
    if (!reg_outs[address - H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io1)])
      printf("Better not implement %04X output, BOY\n", address);
#endif
    return reg_outs[address - H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io1)];
  }
  else
    return NULL;
}

/**
 * @brief h8_byte_in Reads a byte in after any necessary IO input function.
 * @param system
 * @param address
 * @return The byte after IO
 */
static h8_byte_t h8_byte_in(h8_system_t *system, unsigned address)
{
  H8_IN_T in = h8_register_in(system, address);
  h8_byte_t *byte = h8_find(system, address);

  /* NTR-027 hack */
  /* system->vmem.raw[0xFB8C].u = 0x13; */

  if (in)
    in(system, byte);

  return *byte;
}

static void h8_byte_out(h8_system_t *system, const unsigned address,
                        const h8_byte_t value)
{
  if ((address >= H8_MEMORY_REGION_IO1 &&
        address < H8_MEMORY_REGION_IO1 + sizeof(system->vmem.parts.io1)) ||
      address >= H8_MEMORY_REGION_RAM_2K)
  {
    H8_OUT_T out = h8_register_out(system, address & 0xFFFF);
    h8_byte_t *byte = h8_find(system, address & 0xFFFF);

    if (out)
      out(system, byte, value);
    else
      *byte = value;
  }
  else
    h8_log(H8_LOG_WARN, H8_LOG_CPU,
           "Write to invalid address 0x%04X -> 0x%02X", address, value.u);
}

static h8_bool h8_is_io(const unsigned address)
{
  return (address >= H8_MEMORY_REGION_IO1 &&
          address < H8_MEMORY_REGION_IO1 + sizeof(h8_io1_t)) ||
         address >= H8_MEMORY_REGION_IO2;
}

static unsigned h8_byte_states(const unsigned address)
{
  const unsigned a = address & 0xFFFF;

  if ((a >= 0xF0E0 && a <= 0xF0EB) || (a >= 0xFF98 && a <= 0xFF9D) ||
      a == 0xFFA6)
    return 3;
  else
    return 2;
}

static unsigned h8_word_states(const unsigned address)
{
  const unsigned a = address & 0xFFFE;

  /* Timer W TCNT and GRA to GRD, ECPWCR, ECPWDR, and ADRR are 16-bit */
  if (!h8_is_io(a) || (a >= 0xF0F6 && a <= 0xF0FE) || a == 0xFF8C ||
      a == 0xFF8E || a == 0xFFBC)
    return 2;
  else
    return h8_byte_states(a) + h8_byte_states(a + 1);
}

static void h8_count_b(h8_system_t *system, const unsigned address)
{
  system->step_states += h8_byte_states(address);
}

static void h8_count_w(h8_system_t *system, const unsigned address)
{
  system->step_states += h8_word_states(address);
}

static void h8_count_l(h8_system_t *system, const unsigned address)
{
  system->step_states += h8_word_states(address) + h8_word_states(address + 2);
}

static h8_byte_t h8_read_b(h8_system_t *system, const unsigned address)
{
  h8_count_b(system, address);
  return h8_byte_in(system, address);
}

static void h8_write_b(h8_system_t *system, const unsigned address,
                       const h8_byte_t value)
{
  h8_count_b(system, address);
  h8_byte_out(system, address, value);
}

h8_byte_t h8_peek_b(h8_system_t *system, const unsigned address)
{
  return *(h8_byte_t*)h8_find(system, address);
}

void h8_poke_b(h8_system_t *system, const unsigned address,
               const h8_byte_t val)
{
  *(h8_byte_t*)h8_find(system, address) = val;
}

/**
 * Reads a word from explicit big-endian memory to native endianness.
 */
static h8_word_t h8_read_w(h8_system_t *system, unsigned address)
{
  h8_word_t w;

  h8_count_w(system, address);
  w.h = h8_byte_in(system, address);
  w.l = h8_byte_in(system, address + 1);

  return w;
}

/**
 * Writes a 16-bit word from native endianness to explicit big-endian memory.
 */
static void h8_write_w(h8_system_t *system, unsigned address, h8_word_t val)
{
  h8_count_w(system, address);
  h8_byte_out(system, address, val.h);
  h8_byte_out(system, address + 1, val.l);
}

h8_word_t h8_peek_w(h8_system_t *system, const unsigned address)
{
  h8_word_t w;

  w.h = h8_peek_b(system, address);
  w.l = h8_peek_b(system, address + 1);

  return w;
}

void h8_poke_w(h8_system_t *system, const unsigned address,
               const h8_word_t val)
{
  h8_poke_b(system, address, val.h);
  h8_poke_b(system, address + 1, val.l);
}

/**
 * Reads a long in native endianness.
 */
static h8_long_t h8_read_l(h8_system_t *system, const unsigned address)
{
  h8_long_t l;

  h8_count_l(system, address);
  l.a = h8_byte_in(system, address);
  l.b = h8_byte_in(system, address + 1);
  l.c = h8_byte_in(system, address + 2);
  l.d = h8_byte_in(system, address + 3);

  return l;
}

static void h8_write_l(h8_system_t *system, const unsigned address,
                          const h8_long_t val)
{
  h8_count_l(system, address);
  h8_byte_out(system, address, val.a);
  h8_byte_out(system, address + 1, val.b);
  h8_byte_out(system, address + 2, val.c);
  h8_byte_out(system, address + 3, val.d);
}

h8_long_t h8_peek_l(h8_system_t *system, const unsigned address)
{
  h8_long_t l;

  l.a = h8_peek_b(system, address);
  l.b = h8_peek_b(system, address + 1);
  l.c = h8_peek_b(system, address + 2);
  l.d = h8_peek_b(system, address + 3);

  return l;
}

void h8_poke_l(h8_system_t *system, const unsigned address,
                      const h8_long_t val)
{
  h8_poke_b(system, address, val.a);
  h8_poke_b(system, address + 1, val.b);
  h8_poke_b(system, address + 2, val.c);
  h8_poke_b(system, address + 3, val.d);
}

/**
 * Returns a pointer to a direct register accessed as a byte.
 * reg immediate is a 3-bit register number and 1 bit to specify RL or RH.
 */
static h8_byte_t *rd_b(h8_system_t *system, unsigned reg)
{
  if (reg & 0x8)
    return &system->cpu.regs[reg & 0x7].byte.rl;
  else
    return &system->cpu.regs[reg & 0x7].byte.rh;
}

/**
 * Returns a pointer to a direct register accessed as a word.
 * reg immediate is a 3-bit register number and 1 bit to specify E or R.
 */
static h8_word_t *rd_w(h8_system_t *system, unsigned reg)
{
  if (reg & 0x8)
    return &system->cpu.regs[reg & 0x7].word.e;
  else
    return &system->cpu.regs[reg & 0x7].word.r;
}

/**
 * Returns a pointer to a direct register accessed as a long.
 */
static h8_long_t *rd_l(h8_system_t *system, unsigned reg)
{
  return &system->cpu.regs[reg & 0x7].er;
}

typedef unsigned h8_aptr;
#define H8_APTR_MASK 0x0000FFFF

/** General register, accessed as an address */
static h8_aptr er(h8_system_t *system, unsigned ers)
{
  return rd_l(system, ers)->u & H8_APTR_MASK;
}

/** General register, accessed as an address with post-increment */
static h8_aptr erpi_b(h8_system_t *system, unsigned ers)
{
  h8_aptr temp = rd_l(system, ers)->l.u;
  rd_l(system, ers)->u += 1;
  return temp;
}

/** General register, accessed as an address with post-increment */
static h8_aptr erpi_w(h8_system_t *system, unsigned ers)
{
  h8_aptr temp = rd_l(system, ers)->l.u;
  rd_l(system, ers)->u += 2;
  return temp;
}

/** General register, accessed as an address with post-increment */
static h8_aptr erpi_l(h8_system_t *system, unsigned ers)
{
  h8_aptr temp = rd_l(system, ers)->l.u;
  rd_l(system, ers)->u += 4;
  return temp;
}

/** General register, accessed as an address with pre-decrement */
static h8_aptr erpd_b(h8_system_t *system, unsigned ers)
{
  rd_l(system, ers)->u -= 1;
  return rd_l(system, ers)->u;
}

static h8_aptr erpd_w(h8_system_t *system, unsigned ers)
{
  rd_l(system, ers)->u -= 2;
  return rd_l(system, ers)->u;
}

static h8_aptr erpd_l(h8_system_t *system, unsigned ers)
{
  rd_l(system, ers)->u -= 4;
  return rd_l(system, ers)->u;
}

/** General register, accessed as an address with 16-bit displacement */
static h8_aptr erd16(h8_system_t *system, unsigned ers, signed d)
{
  return (h8_aptr)((h8_s32)(rd_l(system, ers)->u) + d) & 0xFFFF;
}

/** General register, accessed as an address with 24-bit displacement */
static h8_aptr erd24(h8_system_t *system, unsigned ers, signed d)
{
  return (h8_aptr)((h8_s32)(rd_l(system, ers)->u) + d);
}

/** 8-bit absolute address */
static h8_aptr aa8(const h8_byte_t aa)
{
  return 0xFF00 | aa.u;
}

/** 16-bit absolute address */
static h8_aptr aa16(const h8_word_t aa)
{
  return aa.u;
}

/** 24-bit absolute address */
static h8_aptr aa24(const h8_long_t aa)
{
  return aa.u & H8_APTR_MASK;
}

#define H8_RS_RD(name, type) \
static void rs_rd_##name(h8_system_t *system, type rs, type *rd, \
                         type(*action)(h8_system_t*, type, const type)) \
{ \
  *rd = action(system, *rd, rs); \
}
H8_RS_RD(b, h8_byte_t)
H8_RS_RD(w, h8_word_t)
H8_RS_RD(l, h8_long_t)

#define H8_MS_RD(name, type) \
static void ms_rd_##name(h8_system_t *system, unsigned ms, type *rd, \
                         type(*action)(h8_system_t*, type, const type)) \
{ \
  type ms_val = h8_read_##name(system, ms); \
  *rd = action(system, *rd, ms_val); \
}
H8_MS_RD(b, h8_byte_t)
H8_MS_RD(w, h8_word_t)
H8_MS_RD(l, h8_long_t)

#define H8_RS_MD(name, type) \
static void rs_md_##name(h8_system_t *system, const type rs, unsigned md, \
                         type(*action)(h8_system_t*, type, const type)) \
{ \
  type md_val = h8_peek_##name(system, md); \
  if (action != mov_##name) \
    h8_count_##name(system, md); \
  h8_write_##name(system, md, action(system, md_val, rs)); \
}
H8_RS_MD(b, h8_byte_t)
H8_RS_MD(w, h8_word_t)
H8_RS_MD(l, h8_long_t)

void addx(h8_system_t *system, h8_byte_t *dst, const h8_byte_t src)
{
  h8_byte_t result;

  result.i = dst->i + src.i + system->cpu.ccr.flags.c;
  system->cpu.ccr.flags.c = result.u < dst->u || (result.u == dst->u && system->cpu.ccr.flags.c);
  system->cpu.ccr.flags.v = ((dst->i >= 0 && src.i >= 0 && result.i < 0) ||
                            (dst->i < 0 && src.i < 0 && result.i >= 0));
  system->cpu.ccr.flags.n = result.i < 0;

  /* Retains its previous value when the result is
   * zero; otherwise cleared to 0 */
  if (result.u)
    system->cpu.ccr.flags.z = 0;

  system->cpu.ccr.flags.h = (result.u & (1 << 4)) != (dst->u & (1 << 4));

  *dst = result;
}

void subx(h8_system_t *system, h8_byte_t *dst, const h8_byte_t src)
{
  h8_byte_t result;

  result.i = dst->i - src.i - system->cpu.ccr.flags.c;
  system->cpu.ccr.flags.c = dst->u < src.u || (dst->u == src.u && system->cpu.ccr.flags.c);
  system->cpu.ccr.flags.v = ((dst->i >= 0 && src.i < 0 && result.i < 0) ||
                            (dst->i < 0 && src.i >= 0 && result.i >= 0));
  system->cpu.ccr.flags.n = result.i < 0;

  /* Retains its previous value when the result is
   * zero; otherwise cleared to 0 */
  if (result.u)
    system->cpu.ccr.flags.z = 0;

  system->cpu.ccr.flags.h = (result.u & (1 << 4)) != (dst->u & (1 << 4));

  *dst = result;
}

/* MULXU multiplies as unsigned values; MULXS below is the signed version */
h8_word_t mulxu_b(h8_word_t dst, const h8_byte_t src)
{
  h8_word_t result;
  result.u = (h8_u16)(src.u * dst.l.u);
  return result;
}

h8_long_t mulxu_w(h8_long_t dst, const h8_word_t src)
{
  h8_long_t result;
  result.u = (h8_u32)src.u * dst.l.u;
  return result;
}

h8_word_t mulxs_b(h8_system_t *system, h8_word_t dst, const h8_byte_t src)
{
  h8_word_t result;
  result.i = src.i * dst.l.i;
  ccr_zn(system, result.i);
  return result;
}

h8_long_t mulxs_w(h8_system_t *system, h8_long_t dst, const h8_word_t src)
{
  h8_long_t result;
  result.i = src.i * dst.l.i;
  ccr_zn(system, result.i);
  return result;
}

h8_word_t divxu_b(h8_system_t *system, h8_word_t top, h8_byte_t bottom)
{
  h8_word_t result = top;

  if (bottom.u != 0)
  {
    result.l.u = (h8_u8)(top.u / bottom.u);
    result.h.u = top.u % bottom.u;
  }
  system->cpu.ccr.flags.n = bottom.i < 0;
  system->cpu.ccr.flags.z = bottom.u == 0;

  return result;
}

h8_long_t divxu_w(h8_system_t *system, h8_long_t top, h8_word_t bottom)
{
  h8_long_t result = top;

  if (bottom.u != 0)
  {
    result.l.u = (h8_u16)(top.u / bottom.u);
    result.h.u = top.u % bottom.u;
  }
  system->cpu.ccr.flags.n = bottom.i < 0;
  system->cpu.ccr.flags.z = bottom.u == 0;

  return result;
}

h8_word_t divxs_b(h8_system_t *system, h8_word_t top, h8_byte_t bottom)
{
  h8_word_t result = {0};

  if (bottom.u != 0)
  {
    result.l.i = (h8_s8)(top.i / bottom.i);
    result.h.i = top.i % bottom.i;
  }
  system->cpu.ccr.flags.n = result.l.i < 0;
  system->cpu.ccr.flags.z = bottom.u == 0;

  return result;
}

h8_long_t divxs_w(h8_system_t *system, h8_long_t top, h8_word_t bottom)
{
  h8_long_t result = {0};

  if (bottom.u != 0)
  {
    result.l.i = (h8_s16)(top.i / bottom.i);
    result.h.i = top.i % bottom.i;
  }
  system->cpu.ccr.flags.n = result.l.i < 0;
  system->cpu.ccr.flags.z = bottom.u == 0;

  return result;
}

#define H8_AND_OP(name, type) \
type and_##name(h8_system_t *system, type dst, const type src) \
{ \
  dst.u &= src.u; \
  ccr_zn(system, dst.i); \
  return dst; \
}
H8_AND_OP(b, h8_byte_t)
H8_AND_OP(w, h8_word_t)
H8_AND_OP(l, h8_long_t)

#define H8_OR_OP(name, type) \
type or_##name(h8_system_t *system, type dst, const type src) \
{ \
  dst.u |= src.u; \
  ccr_zn(system, dst.i); \
  return dst; \
}
H8_OR_OP(b, h8_byte_t)
H8_OR_OP(w, h8_word_t)
H8_OR_OP(l, h8_long_t)

#define H8_XOR_OP(name, type) \
type xor_##name(h8_system_t *system, type dst, const type src) \
{ \
  dst.u ^= src.u; \
  ccr_zn(system, dst.i); \
  return dst; \
}
H8_XOR_OP(b, h8_byte_t)
H8_XOR_OP(w, h8_word_t)
H8_XOR_OP(l, h8_long_t)

#define H8_NOT_OP(name, type) \
void not_##name(h8_system_t *system, type *dst) \
{ \
  dst->u = ~dst->u; \
  ccr_zn(system, dst->i); \
}
H8_NOT_OP(b, h8_byte_t)
H8_NOT_OP(w, h8_word_t)
H8_NOT_OP(l, h8_long_t)

#define H8_NEG_OP(name, type) \
void neg_##name(h8_system_t *system, type *src) \
{ \
  src->i = 0 - src->i; \
  ccr_zn(system, src->i); \
  /** @todo CCR */ \
}
H8_NEG_OP(b, h8_byte_t)
H8_NEG_OP(w, h8_word_t)
H8_NEG_OP(l, h8_long_t)

static void incb(h8_system_t *system, h8_byte_t *rd)
{
  h8_byte_t one;
  h8_u8 prev_h = system->cpu.ccr.flags.h;
  h8_u8 prev_c = system->cpu.ccr.flags.c;
  one.u = 1;
  *rd = add_b(system, *rd, one);
  system->cpu.ccr.flags.h = prev_h;
  system->cpu.ccr.flags.c = prev_c;
}

static void incw(h8_system_t *system, h8_word_t *rd, unsigned n)
{
  h8_word_t val;
  h8_u8 prev_h = system->cpu.ccr.flags.h;
  h8_u8 prev_c = system->cpu.ccr.flags.c;
  val.u = n;
  *rd = add_w(system, *rd, val);
  system->cpu.ccr.flags.h = prev_h;
  system->cpu.ccr.flags.c = prev_c;
}

static void incl(h8_system_t *system, h8_long_t *rd, unsigned n)
{
  h8_long_t val;
  h8_u8 prev_h = system->cpu.ccr.flags.h;
  h8_u8 prev_c = system->cpu.ccr.flags.c;
  val.u = n;
  *rd = add_l(system, *rd, val);
  system->cpu.ccr.flags.h = prev_h;
  system->cpu.ccr.flags.c = prev_c;
}

static void decb(h8_system_t *system, h8_byte_t *rd)
{
  h8_byte_t one;
  h8_u8 prev_h = system->cpu.ccr.flags.h;
  h8_u8 prev_c = system->cpu.ccr.flags.c;
  one.u = 1;
  *rd = sub_b(system, *rd, one);
  system->cpu.ccr.flags.h = prev_h;
  system->cpu.ccr.flags.c = prev_c;
}

static void decw(h8_system_t *system, h8_word_t *rd, unsigned n)
{
  h8_word_t val;
  h8_u8 prev_h = system->cpu.ccr.flags.h;
  h8_u8 prev_c = system->cpu.ccr.flags.c;
  val.u = n;
  *rd = sub_w(system, *rd, val);
  system->cpu.ccr.flags.h = prev_h;
  system->cpu.ccr.flags.c = prev_c;
}

static void decl(h8_system_t *system, h8_long_t *rd, unsigned n)
{
  h8_long_t val;
  h8_u8 prev_h = system->cpu.ccr.flags.h;
  h8_u8 prev_c = system->cpu.ccr.flags.c;
  val.u = n;
  *rd = sub_l(system, *rd, val);
  system->cpu.ccr.flags.h = prev_h;
  system->cpu.ccr.flags.c = prev_c;
}

/** @todo Hacky */
#define H8_CMP_OP(name, type) \
type cmp_##name(h8_system_t *system, type dst, const type src) \
{ \
  sub_##name(system, dst, src); \
  return dst; \
}
H8_CMP_OP(b, h8_byte_t)
H8_CMP_OP(w, h8_word_t)
H8_CMP_OP(l, h8_long_t)

h8_byte_t bset(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  H8_UNUSED(system);
  dst.u |= (1 << (src.u & B00000111));
  return dst;
}

h8_byte_t bclr(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  H8_UNUSED(system);
  dst.u &= ~(1 << (src.u & B00000111));
  return dst;
}

h8_byte_t bnot(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  H8_UNUSED(system);
  dst.u ^= (1 << (src.u & B00000111));
  return dst;
}

void btst(h8_system_t *system, h8_byte_t *dst, unsigned src)
{
#if H8_SAFETY
  if (src > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BTST parameter - %02X at %04X",
           src, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.z = dst->u & (1 << (src & B00000111)) ? 0 : 1;
}

h8_byte_t bst(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
#if H8_SAFETY
  if (src.u > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BST parameter - %02X at %04X",
           src.u, system->cpu.pc - 2);
  }
#endif
  dst.u &= ~(1 << src.u);
  dst.u |= (system->cpu.ccr.flags.c << (src.u));
  return dst;
}

h8_byte_t bist(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
#if H8_SAFETY
  if (src.u > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BIST parameter - %02X at %04X",
           src.u, system->cpu.pc - 2);
  }
#endif
  dst.u &= ~(1 << src.u);
  dst.u |= ((!system->cpu.ccr.flags.c) << src.u);
  return dst;
}

void bld(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BLD parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c = val.u & (1 << (bit & B00000111)) ? 1 : 0;
}

void bild(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BILD parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c = val.u & (1 << (bit & B00000111)) ? 0 : 1;
}

void bor(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BOR parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c |= val.u & (1 << (bit & B00000111)) ? 1 : 0;
}

void bior(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BIOR parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c |= val.u & (1 << (bit & B00000111)) ? 0 : 1;
}

void bxor(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BXOR parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c ^= val.u & (1 << (bit & B00000111)) ? 1 : 0;
}

void bixor(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BIXOR parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c ^= val.u & (1 << (bit & B00000111)) ? 0 : 1;
}

void band(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BAND parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c &= val.u & (1 << (bit & B00000111)) ? 1 : 0;
}

void biand(h8_system_t *system, h8_byte_t val, unsigned bit)
{
#if H8_SAFETY
  if (bit > B0111)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Bad BIAND parameter - %02X at %04X",
           bit, system->cpu.pc - 2);
  }
#endif
  system->cpu.ccr.flags.c &= val.u & (1 << (bit & B00000111)) ? 0 : 1;
}

void bsr(h8_system_t *system, signed offset)
{
#if H8_SAFETY
  if (offset > 0x7fff || offset < -0x7fff)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    return;
  }
  else
  {
#endif
    h8_word_t sp;

    sp.u = (h8_u16)system->cpu.pc;
    system->cpu.regs[7].er.u -= 2;
    h8_write_w(system, system->cpu.regs[7].er.u, sp);
    system->cpu.pc += offset;
#if H8_SAFETY
  }
#endif
}

/**
 * Performs exception handling for an interrupt or trap (3.6, figure 3.5):
 * pushes PC and CCR, masks interrupts, and jumps through the vector table.
 * The pushed PC is the address of the next instruction to execute.
 */
static void h8_exception(h8_system_t *system, const unsigned vector)
{
  h8_word_t w;

  w.u = (h8_u16)system->cpu.pc;
  system->cpu.regs[7].er.u -= 2;
  h8_write_w(system, system->cpu.regs[7].er.u, w);

  /* CCR is saved as a word; the odd byte is ignored on return */
  w.h = system->cpu.ccr.raw;
  w.l = system->cpu.ccr.raw;
  system->cpu.regs[7].er.u -= 2;
  h8_write_w(system, system->cpu.regs[7].er.u, w);

  system->cpu.ccr.flags.i = 1;
  system->cpu.pc = h8_read_w(system, vector * 2).u;
}

/**
 * Reads a 2-byte big-endian value from the address referenced by PC to the
 * data bus, then increments PC by 2.
 */
void h8_fetch(h8_system_t *system)
{
  system->dbus.bits = h8_read_w(system, system->cpu.pc);
  system->cpu.pc += 2;
#if H8_DEBUG_PRINT_FETCH
  printf("%02X %02X ", system->dbus.a.u, system->dbus.b.u);
#endif
}

/**
 * Fetches the two instruction words holding a 24-bit absolute address or
 * displacement (H'00 or the sign, then 24 bits), sign-extended to 32 bits.
 */
static h8_long_t h8_fetch_24(h8_system_t *system)
{
  h8_instruction_t op = system->dbus;
  h8_long_t value;

  h8_fetch(system);
  value.h = system->dbus.bits;
  h8_fetch(system);
  value.l = system->dbus.bits;
  value.a.u = (value.b.u & 0x80) ? 0xFF : 0x00;
  system->dbus = op;

  return value;
}

/**
 * =============================================================================
 * Opcode decoding implementations
 * =============================================================================
 */

H8_OP(op00)
{
  /** NOP */
  (void)system;
}

/** @todo Nasty code */
H8_OP(op01)
{
  switch (system->dbus.b.u)
  {
  case 0x00:
    h8_fetch(system);
    switch (system->dbus.a.u)
    {
    case 0x69:
      if (system->dbus.bh & B1000)
        /** MOV.L ERs, @ERd */
        rs_md_l(system, *rd_l(system, system->dbus.bl), er(system, system->dbus.bh), mov_l);
      else
        /** MOV.L @ERs, ERd */
        ms_rd_l(system, er(system, system->dbus.bh), rd_l(system, system->dbus.bl), mov_l);
      break;
    case 0x6B:
    {
      switch (system->dbus.bh)
      {
      case 0x0:
      {
        /** MOV.L @aa:16, ERd */
        h8_u8 erd = system->dbus.bl;

        h8_fetch(system);
        ms_rd_l(system, aa16(system->dbus.bits), rd_l(system, erd), mov_l);
        break;
      }
      case 0x2:
        /** MOV.L @aa:24, ERd */
        ms_rd_l(system, aa24(h8_fetch_24(system)), rd_l(system, system->dbus.bl), mov_l);
        break;
      case 0x8:
      {
        /** MOV.L ERs, @aa:16 */
        h8_u8 ers = system->dbus.bl;

        h8_fetch(system);
        rs_md_l(system, *rd_l(system, ers), system->dbus.bits.u, mov_l);
        break;
      }
      case 0xA:
        /** MOV.L ERs, @aa:24 */
        rs_md_l(system, *rd_l(system, system->dbus.bl), aa24(h8_fetch_24(system)), mov_l);
        break;
      default:
        H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
      }
      break;
    }
    case 0x6D:
      if (system->dbus.bh & B1000)
        /** MOV.L ERs, @-ERd */
        rs_md_l(system, *rd_l(system, system->dbus.bl), erpd_l(system, system->dbus.bh), mov_l);
      else
        /** MOV.L @ERs+, ERd */
        ms_rd_l(system, erpi_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), mov_l);
      break;
    case 0x6F:
    {
      h8_byte_t sd = system->dbus.b;

      h8_fetch(system);
      if (sd.h & B1000)
        /** MOV.L ERs, @(d:16, ERd) */
        rs_md_l(system, *rd_l(system, sd.l), erd16(system, sd.h, system->dbus.bits.i), mov_l);
      else
        /** MOV.L @(d:16, ERs), ERd */
        ms_rd_l(system, erd16(system, sd.h, system->dbus.bits.i), rd_l(system, sd.l), mov_l);
      break;
    }
    case 0x78:
    {
      h8_u8 ern = system->dbus.bh;
      h8_long_t disp;

      h8_fetch(system);
      disp = h8_fetch_24(system);
      if (system->dbus.a.u != 0x6B || ern & B1000 || system->dbus.bl & B1000)
        H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
      else if (system->dbus.bh == 0x2)
        /** MOV.L @(d:24, ERs), ERd */
        ms_rd_l(system, erd24(system, ern, disp.i), rd_l(system, system->dbus.bl), mov_l);
      else if (system->dbus.bh == 0xA)
        /** MOV.L ERs, @(d:24, ERd) */
        rs_md_l(system, *rd_l(system, system->dbus.bl), erd24(system, ern, disp.i), mov_l);
      else
        H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
      break;
    }
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
    break;
  case 0x40:
  {
    h8_bool store;
    h8_aptr address;
    h8_word_t w;

    h8_fetch(system);
    store = (system->dbus.bh & B1000) != 0;
    switch (system->dbus.a.u)
    {
    case 0x69:
      /** @ERs / @ERd */
      address = er(system, system->dbus.bh);
      break;
    case 0x6B:
      if ((system->dbus.bh & B0111) == 0x0)
      {
        /** @aa:16 */
        h8_fetch(system);
        address = aa16(system->dbus.bits);
      }
      else if ((system->dbus.bh & B0111) == 0x2)
        /** @aa:24 */
        address = aa24(h8_fetch_24(system));
      else
      {
        H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
        return;
      }
      break;
    case 0x6D:
      /** @ERs+ / @-ERd */
      address = store ? erpd_w(system, system->dbus.bh) :
                        erpi_w(system, system->dbus.bh);
      break;
    case 0x6F:
    {
      /** @(d:16, ERs) / @(d:16, ERd) */
      h8_u8 ern = system->dbus.bh;

      h8_fetch(system);
      address = erd16(system, ern, system->dbus.bits.i);
      break;
    }
    case 0x78:
    {
      h8_u8 ern = system->dbus.bh;
      h8_long_t disp;

      h8_fetch(system);
      disp = h8_fetch_24(system);
      if (system->dbus.a.u != 0x6B)
      {
        H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
        return;
      }
      store = (system->dbus.bh & B1000) != 0;
      address = erd24(system, ern, disp.i);
      break;
    }
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
      return;
    }

    if (store)
    {
      /** STC.W CCR, <EAd> */
      w.h = system->cpu.ccr.raw;
      w.l = system->cpu.ccr.raw;
      h8_write_w(system, address, w);
    }
    else
      /** LDC.W <EAs>, CCR */
      system->cpu.ccr.raw = h8_read_w(system, address).h;
    break;
  }
  case 0x80:
    /** SLEEP: enters the power-down mode selected by SYSCR1 and SYSCR2 */
    h8_power_sleep(system);
    break;
  case 0xC0:
    h8_fetch(system);
    switch (system->dbus.a.u)
    {
    case 0x50:
      /** MULXS.B Rs, Rd */
      *rd_w(system, system->dbus.bl) = mulxs_b(system, *rd_w(system, system->dbus.bl), *rd_b(system, system->dbus.bh));
      break;
    case 0x52:
      /** MULXS.W Rs, ERd */
      *rd_l(system, system->dbus.bl) = mulxs_w(system, *rd_l(system, system->dbus.bl), *rd_w(system, system->dbus.bh));
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
    break;
  case 0xD0:
    h8_fetch(system);
    switch (system->dbus.a.u)
    {
    case 0x51:
      /** DIVXS.B Rs, Rd */
      *rd_w(system, system->dbus.bl) = divxs_b(system, *rd_w(system, system->dbus.bl), *rd_b(system, system->dbus.bh));
      break;
    case 0x53:
      /** DIVXS.W Rs, ERd */
      *rd_l(system, system->dbus.bl) = divxs_w(system, *rd_l(system, system->dbus.bl), *rd_w(system, system->dbus.bh));
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
    break;
  case 0xF0:
    h8_fetch(system);
    if (system->dbus.bh & B1000 || system->dbus.bl & B1000)
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    else switch (system->dbus.a.u)
    {
    case 0x64:
      /** OR.L ERs, ERd */
      rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), or_l);
      break;
    case 0x65:
      /** XOR.L ERs, ERd */
      rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), xor_l);
      break;
    case 0x66:
      /** AND.L ERs, ERd */
      rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), and_l);
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op02)
{
  /** STC CCR, Rd */
  *rd_b(system, system->dbus.bl) = system->cpu.ccr.raw;
}

H8_OP(op03)
{
  /** LDC Rs, CCR */
  system->cpu.ccr.raw = *rd_b(system, system->dbus.bl);
}

H8_OP(op04)
{
  /** ORC #xx:8, CCR */
  system->cpu.ccr.raw.u |= system->dbus.b.u;
}

H8_OP(op05)
{
  /** XORC #xx:8, CCR */
  system->cpu.ccr.raw.u ^= system->dbus.b.u;
}

H8_OP(op06)
{
  /** ANDC #xx:8, CCR */
  system->cpu.ccr.raw.u &= system->dbus.b.u;
}

H8_OP(op07)
{
  /** LDC #xx:8, CCR */
  system->cpu.ccr.raw = system->dbus.b;
}

H8_OP(op08)
{
  /** ADD.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), add_b);
}

H8_OP(op09)
{
  /** ADD.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), add_w);
}

H8_OP(op0a)
{
  if (system->dbus.bh == 0x00)
  {
    /** INC.B Rd */
    incb(system, rd_b(system, system->dbus.bl));
  }
  else if (system->dbus.bh & B1000)
    /** ADD.L ERs, ERd */
    rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), add_l);
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op0b)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** @todo Verify ADDS.L #1, ERd */
    adds_l(system, rd_l(system, system->dbus.bl), 1);
    break;
  case 0x5:
    /** INC.W #1, Rd */
    incw(system, rd_w(system, system->dbus.bl), 1);
    break;
  case 0x7:
    /** INC.L #1, ERd */
    incl(system, rd_l(system, system->dbus.bl), 1);
    break;
  case 0x8:
    /** @todo Verify ADDS.L #2, ERd */
    adds_l(system, rd_l(system, system->dbus.bl), 2);
    break;
  case 0x9:
    /** @todo Verify ADDS.L #4, ERd */
    adds_l(system, rd_l(system, system->dbus.bl), 4);
    break;
  case 0xD:
    /** INC.W #2, Rd */
    incw(system, rd_w(system, system->dbus.bl), 2);
    break;
  case 0xF:
    /** INC.L #2, ERd */
    incl(system, rd_l(system, system->dbus.bl), 2);
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op0c)
{
  /** MOV.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), mov_b);
}

H8_OP(op0d)
{
  /** MOV.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), mov_w);
}

H8_OP(op0e)
{
  /** ADDX.B Rs, Rd */
  addx(system, rd_b(system, system->dbus.bl), *rd_b(system, system->dbus.bh));
}

H8_OP(op0f)
{
  if (system->dbus.bh == 0x0)
    /** DAA.B Rd */
    daa_b(system, rd_b(system, system->dbus.bl));
  else if (system->dbus.bh & B1000)
    /** MOV.L ERs, ERd */
    rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), mov_l);
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op10)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** SHLL.B Rd */
    shll_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x1:
    /** SHLL.W Rd */
    shll_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0x3:
    /** SHLL.L ERd */
    shll_l(system, rd_l(system, system->dbus.bl));
    break;
  case 0x8:
    /** SHAL.B Rd */
    shal_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x9:
    /** SHAL.W Rd */
    shal_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0xB:
    /** SHAL.L ERd */
    shal_l(system, rd_l(system, system->dbus.bl));
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op11)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** SHLR.B Rd */
    shlr_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x1:
    /** SHLR.W Rd */
    shlr_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0x3:
    /** SHLR.L ERd */
    shlr_l(system, rd_l(system, system->dbus.bl));
    break;
  case 0x8:
    /** SHAR.B Rd */
    shar_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x9:
    /** SHAR.W Rd */
    shar_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0xB:
    /** SHAR.L ERd */
    shar_l(system, rd_l(system, system->dbus.bl));
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op12)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** ROTXL.B Rd */
    rotxl_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x1:
    /** ROTXL.W Rd */
    rotxl_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0x3:
    /** ROTXL.L ERd */
    rotxl_l(system, rd_l(system, system->dbus.bl));
    break;
  case 0x8:
    /** ROTL.B Rd */
    rotl_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x9:
    /** ROTL.W Rd */
    rotl_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0xB:
    /** ROTL.L ERd */
    rotl_l(system, rd_l(system, system->dbus.bl));
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op13)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** ROTXR.B Rd */
    rotxr_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x1:
    /** ROTXR.W Rd */
    rotxr_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0x3:
    /** ROTXR.L ERd */
    rotxr_l(system, rd_l(system, system->dbus.bl));
    break;
  case 0x8:
    /** ROTR.B Rd */
    rotr_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x9:
    /** ROTR.W Rd */
    rotr_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0xB:
    /** ROTR.L ERd */
    rotr_l(system, rd_l(system, system->dbus.bl));
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op14)
{
  /** OR.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), or_b);
}

H8_OP(op15)
{
  /** XOR.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), xor_b);
}

H8_OP(op16)
{
  /** AND.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), and_b);
}

H8_OP(op17)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** NOT.B Rd */
    not_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x1:
    /** NOT.W Rd */
    not_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0x3:
    /** NOT.L ERd */
    not_l(system, rd_l(system, system->dbus.bl));
    break;
  case 0x5:
    /** EXTU.W Rd */
    extu_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0x7:
    /** EXTU.L ERd */
    extu_l(system, rd_l(system, system->dbus.bl));
    break;
  case 0x8:
    /** NEG.B Rd */
    neg_b(system, rd_b(system, system->dbus.bl));
    break;
  case 0x9:
    /** NEG.W Rd */
    neg_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0xB:
    /** NEG.L ERd */
    neg_l(system, rd_l(system, system->dbus.bl));
    break;
  case 0xD:
    /** EXTS.W Rd */
    exts_w(system, rd_w(system, system->dbus.bl));
    break;
  case 0xF:
    /** EXTS.L ERd */
    exts_l(system, rd_l(system, system->dbus.bl));
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op18)
{
  /** SUB.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), sub_b);
}

H8_OP(op19)
{
  /** SUB.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), sub_w);
}

H8_OP(op1a)
{
  if (system->dbus.bh == 0x00)
  {
    /** DEC.B Rd */
    decb(system, rd_b(system, system->dbus.bl));
  }
  else if (system->dbus.bh & B1000)
    /** SUB.L ERs, ERd */
    rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), sub_l);
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op1b)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** SUBS.L #1, ERd */
    subs_l(system, rd_l(system, system->dbus.bl), 1);
    break;
  case 0x5:
    /** DEC.W #1, Rd */
    decw(system, rd_w(system, system->dbus.bl), 1);
    break;
  case 0x7:
    /** DEC.L #1, ERd */
    decl(system, rd_l(system, system->dbus.bl), 1);
    break;
  case 0x8:
    /** SUBS.L #2, ERd */
    subs_l(system, rd_l(system, system->dbus.bl), 2);
    break;
  case 0x9:
    /** SUBS.L #4, ERd */
    subs_l(system, rd_l(system, system->dbus.bl), 4);
    break;
  case 0xD:
    /** DEC.W #2, Rd */
    decw(system, rd_w(system, system->dbus.bl), 2);
    break;
  case 0xF:
    /** DEC.L #2, ERd */
    decl(system, rd_l(system, system->dbus.bl), 2);
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op1c)
{
  /** CMP.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh),
          rd_b(system, system->dbus.bl), cmp_b);
}

H8_OP(op1d)
{
  /** CMP.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh),
          rd_w(system, system->dbus.bl), cmp_w);
}

H8_OP(op1e)
{
  /** SUBX Rs, Rd */
  subx(system, rd_b(system, system->dbus.bl), *rd_b(system, system->dbus.bh));
}

H8_OP(op1f)
{
  if (system->dbus.bh == 0x0)
    /** DAS.B Rd */
    das_b(system, rd_b(system, system->dbus.bl));
  else if (system->dbus.bh & B1000)
    /** CMP.L ERs, ERd */
    rs_rd_l(system, *rd_l(system, system->dbus.bh),
            rd_l(system, system->dbus.bl), cmp_l);
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

#define OP2X(al, reg) \
void op2##al(h8_system_t *system) \
{ \
  /** MOV.B @aa:8, Rd */ \
  ms_rd_b(system, aa8(system->dbus.b), &reg, mov_b); \
}
H8_UNROLL(OP2X)

#define OP3X(al, reg) \
void op3##al(h8_system_t *system) \
{ \
  /** MOV.B Rs, @aa:8 */ \
  rs_md_b(system, reg, aa8(system->dbus.b), mov_b); \
}
H8_UNROLL(OP3X)

H8_OP(op40)
{
  /** BRA d:8 */
  system->cpu.pc += system->dbus.b.i;
}

H8_OP(op41)
{
  /** BRN d:8 */
  (void)system;
}

H8_OP(op42)
{
  /** BHI d:8 */
  if (!(system->cpu.ccr.flags.c || system->cpu.ccr.flags.z))
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op43)
{
  /** BLS d:8 */
  if (system->cpu.ccr.flags.c || system->cpu.ccr.flags.z)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op44)
{
  /** BCC d:8 */
  if (!system->cpu.ccr.flags.c)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op45)
{
  /** BCS d:8 */
  if (system->cpu.ccr.flags.c)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op46)
{
  /** BNE d:8 */
  if (!system->cpu.ccr.flags.z)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op47)
{
  /** BEQ d:8 */
  if (system->cpu.ccr.flags.z)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op48)
{
  /** BVC d:8 */
  if (!system->cpu.ccr.flags.v)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op49)
{
  /** BVS d:8 */
  if (system->cpu.ccr.flags.v)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op4a)
{
  /** BPL d:8 */
  if (!system->cpu.ccr.flags.n)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op4b)
{
  /** BMI d:8 */
  if (system->cpu.ccr.flags.n)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op4c)
{
  /** BGE d:8 */
  if (!(system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v))
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op4d)
{
  /** BLT d:8 */
  if (system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v)
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op4e)
{
  /** BGT d:8 */
  if (!(system->cpu.ccr.flags.z || (system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v)))
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op4f)
{
  /** BLE d:8 */
  if (system->cpu.ccr.flags.z || (system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v))
    system->cpu.pc += system->dbus.b.i;
}

H8_OP(op50)
{
  /** MULXU.B Rs, Rd */
  *rd_w(system, system->dbus.bl) = mulxu_b(*rd_w(system, system->dbus.bl), *rd_b(system, system->dbus.bh));
}

H8_OP(op51)
{
  /** DIVXU.B Rs, Rd */
  *rd_w(system, system->dbus.bl) = divxu_b(system, *rd_w(system, system->dbus.bl), *rd_b(system, system->dbus.bh));
}

H8_OP(op52)
{
  /** MULXU.W Rs, ERd */
  if (system->dbus.bl & B1000)
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  else
    *rd_l(system, system->dbus.bl) = mulxu_w(*rd_l(system, system->dbus.bl), *rd_w(system, system->dbus.bh));
}

H8_OP(op53)
{
  /** DIVXU.W Rs, ERd */
  *rd_l(system, system->dbus.bl) = divxu_w(system, *rd_l(system, system->dbus.bl), *rd_w(system, system->dbus.bh));
}

H8_OP(op54)
{
  /** RTS */
  if (system->dbus.b.u == 0x70)
  {
    system->cpu.pc = h8_read_w(system, system->cpu.regs[7].er.u).u;
    system->cpu.regs[7].er.u += 2;
  }
  else
    /** @todo What does this do on HW? objdump says it's invalid */
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op55)
{
  /** BSR d:8 */
  bsr(system, system->dbus.b.i);
}

H8_OP(op56)
{
  /** RTE */
  if (system->dbus.b.u == 0x70)
  {
    /* Only high byte of CCR is restored */
    system->cpu.ccr.raw = h8_read_w(system, system->cpu.regs[7].er.u).h;
    system->cpu.regs[7].er.u += 2;
    system->cpu.pc = h8_read_w(system, system->cpu.regs[7].er.u).u;
    system->cpu.regs[7].er.u += 2;
  }
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op57)
{
  /** TRAPA #x:2 */
  if ((system->dbus.b.u & 0xCF) == 0)
    h8_exception(system, H8_VECTOR_TRAPA0 + (system->dbus.bh & B0011));
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op58)
{
  h8_u8 condition = system->dbus.bh;

  h8_fetch(system);
  switch (condition)
  {
  case 0x0:
    /** BRA d:16 */
    system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x1:
    /** BRN d:16 (Branch Never, just skip) */
    break;
  case 0x2:
    /** BHI d:16 */
    if (!(system->cpu.ccr.flags.c || system->cpu.ccr.flags.z))
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x3:
    /** BLS d:16 */
    if (system->cpu.ccr.flags.c || system->cpu.ccr.flags.z)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x4:
    /** BCC (BHS) d:16 */
    if (!system->cpu.ccr.flags.c)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x5:
    /** BCS (BLO) d:16 */
    if (system->cpu.ccr.flags.c)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x6:
    /** BNE d:16 */
    if (!system->cpu.ccr.flags.z)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x7:
    /** BEQ d:16 */
    if (system->cpu.ccr.flags.z)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x8:
    /** BVC d:16 */
    if (!system->cpu.ccr.flags.v)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0x9:
    /** BVS d:16 */
    if (system->cpu.ccr.flags.v)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0xA:
    /** BPL d:16 */
    if (!system->cpu.ccr.flags.n)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0xB:
    /** BMI d:16 */
    if (system->cpu.ccr.flags.n)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0xC:
    /** BGE d:16 */
    if (!(system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v))
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0xD:
    /** BLT d:16 */
    if (system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v)
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0xE:
    /** BGT d:16 */
    if (!(system->cpu.ccr.flags.z || (system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v)))
      system->cpu.pc += system->dbus.bits.i;
    break;
  case 0xF:
    /** BLE d:16 */
    if (system->cpu.ccr.flags.z || (system->cpu.ccr.flags.n ^ system->cpu.ccr.flags.v))
      system->cpu.pc += system->dbus.bits.i;
    break;
  default:
    H8_ERROR(H8_DEBUG_UNREACHABLE_CODE)
  }
}


H8_OP(op59)
{
  /** JMP @ERs */
  system->cpu.pc = aa24(*rd_l(system, system->dbus.bh));
}

H8_OP(op5a)
{
  /** JMP @aa:24 */
  h8_long_t addr;

  addr.b = system->dbus.b;
  h8_fetch(system);
  addr.l = system->dbus.bits;
  system->cpu.pc = aa24(addr);
}

H8_OP(op5b)
{
  /** JMP @@aa:8 */
  system->cpu.pc = h8_read_w(system, system->dbus.b.u).u;
}

H8_OP(op5c)
{
  /** BSR d:16 */
  h8_fetch(system);
  bsr(system, system->dbus.bits.i);
}

H8_OP(op5d)
{
  /** JSR @ERn */
  h8_word_t sp;

  sp.u = (h8_u16)system->cpu.pc;
  system->cpu.regs[7].er.u -= 2;
  h8_write_w(system, system->cpu.regs[7].er.u, sp);
  system->cpu.pc = rd_w(system, system->dbus.bh)->u;
}

H8_OP(op5e)
{
  /** JSR @aa:24 */
  h8_word_t sp;

  system->cpu.regs[7].er.u -= 2;
  h8_fetch(system);
  sp.u = (h8_u16)system->cpu.pc;
  h8_write_w(system, system->cpu.regs[7].er.u, sp);
  system->cpu.pc = system->dbus.bits.u;
}

H8_OP(op5f)
{
  /** JSR @@aa:8 */
  h8_word_t sp, target;

  target = h8_read_w(system, system->dbus.b.u);
  sp.u = (h8_u16)system->cpu.pc;
  system->cpu.regs[7].er.u -= 2;
  h8_write_w(system, system->cpu.regs[7].er.u, sp);
  system->cpu.pc = target.u;
}

H8_OP(op60)
{
  /** BSET Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bset);
}

H8_OP(op61)
{
  /** BNOT Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bnot);
}

H8_OP(op62)
{
  /** BCLR Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bclr);
}

H8_OP(op63)
{
  /** BTST Rs, Rd */
  btst(system, rd_b(system, system->dbus.bl), rd_b(system, system->dbus.bh)->u);
}

H8_OP(op64)
{
  /** OR.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), or_w);
}

H8_OP(op65)
{
  /** XOR.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), xor_w);
}

H8_OP(op66)
{
  /** AND.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), and_w);
}

H8_OP(op67)
{
  h8_byte_t immediate;

  immediate.u = system->dbus.bh & B0111;
  if (system->dbus.bh & B1000)
    /** BIST #xx:3, Rd */
    rs_rd_b(system, immediate, rd_b(system, system->dbus.bl), bist);
  else
    /** BST #xx:3, Rd */
    rs_rd_b(system, immediate, rd_b(system, system->dbus.bl), bst);
}

H8_OP(op68)
{
  if (system->dbus.bh & B1000)
    /** MOV.B Rs, @ERd */
    rs_md_b(system, *rd_b(system, system->dbus.bl), er(system, system->dbus.bh), mov_b);
  else
    /** MOV.B @ERs, Rd */
    ms_rd_b(system, er(system, system->dbus.bh), rd_b(system, system->dbus.bl), mov_b);
}

H8_OP(op69)
{
  if (system->dbus.bh & B1000)
    /** MOV.W Rs, @ERd */
    rs_md_w(system, *rd_w(system, system->dbus.bl), er(system, system->dbus.bh), mov_w);
  else
    /** MOV.W @ERs, Rd */
    ms_rd_w(system, er(system, system->dbus.bh), rd_w(system, system->dbus.bl), mov_w);
}

H8_OP(op6a)
{
  h8_word_t func = system->dbus.bits;

  h8_fetch(system);
  switch (func.l.h)
  {
  case 0x0:
    /** MOV.B @aa:16, Rd */
    ms_rd_b(system, system->dbus.bits.u, rd_b(system, func.l.l), mov_b);
    break;
  case 0x2:
    /** MOV.B @aa:24, Rd */
    h8_fetch(system);
    ms_rd_b(system, system->dbus.bits.u, rd_b(system, func.l.l), mov_b);
    break;
  case 0x4:
    /** MOVFPE @aa:16, Rd */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  case 0x8:
    /** MOV.B Rs, @aa:16 */
    rs_md_b(system, *rd_b(system, func.l.l), system->dbus.bits.u, mov_b);
    break;
  case 0xA:
    /** MOV.B Rs, @aa:24 @todo only works for non-extended mode */
    h8_fetch(system);
    rs_md_b(system, *rd_b(system, func.l.l), system->dbus.bits.u, mov_b);
    break;
  case 0xC:
    /** MOVFPE Rs, @aa:16 */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op6b)
{
  h8_word_t func = system->dbus.bits;

  h8_fetch(system);
  switch (func.l.h)
  {
  case 0x0:
    /** MOV.W @aa:16, Rd */
    ms_rd_w(system, system->dbus.bits.u, rd_w(system, func.l.l), mov_w);
    break;
  case 0x2:
    /** MOV.W @aa:24, Rd */
    h8_fetch(system);
    ms_rd_w(system, system->dbus.bits.u, rd_w(system, func.l.l), mov_w);
    break;
  case 0x8:
    /** MOV.W Rs, @aa:16 */
    rs_md_w(system, *rd_w(system, func.l.l), system->dbus.bits.u, mov_w);
    break;
  case 0xA:
    /** MOV.W Rs, @aa:24 */
    h8_fetch(system);
    rs_md_w(system, *rd_w(system, func.l.l), system->dbus.bits.u, mov_w);
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op6c)
{
  if (system->dbus.bh & B1000)
    /** MOV.B Rs, @-ERd */
    rs_md_b(system, *rd_b(system, system->dbus.bl), erpd_b(system, system->dbus.bh), mov_b);
  else
    /** MOV.B @ERs+, Rd */
    ms_rd_b(system, erpi_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), mov_b);
}

H8_OP(op6d)
{
  if (system->dbus.bh & B1000)
    /** MOV.W Rs, @-ERd */
    rs_md_w(system, *rd_w(system, system->dbus.bl), erpd_w(system, system->dbus.bh), mov_w);
  else
    /** MOV.W @ERs+, Rd */
    ms_rd_w(system, erpi_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), mov_w);
}

H8_OP(op6e)
{
  h8_instruction_t func = system->dbus;

  h8_fetch(system);
  if (func.bh & B1000)
    /** MOV.B Rs, @(d:16, ERd) */
    rs_md_b(system, *rd_b(system, func.bl), erd16(system, func.bh, system->dbus.bits.i), mov_b);
  else
    /** MOV.B @(d:16, ERs), Rd */
    ms_rd_b(system, erd16(system, func.bh, system->dbus.bits.i), rd_b(system, func.bl), mov_b);
}

H8_OP(op6f)
{
  h8_instruction_t func = system->dbus;

  h8_fetch(system);
  if (func.bh & B1000)
    /** MOV.W Rs, @(d:16, ERd) */
    rs_md_w(system, *rd_w(system, func.bl), erd16(system, func.bh, system->dbus.bits.i), mov_w);
  else
    /** MOV.W @(d:16, ERs), Rd */
    ms_rd_w(system, erd16(system, func.bh, system->dbus.bits.i), rd_w(system, func.bl), mov_w);
}

H8_OP(op70)
{
  /** BSET #xx:3, Rd */
  h8_byte_t immediate;

  immediate.u = system->dbus.bh;
  rs_rd_b(system, immediate, rd_b(system, system->dbus.bl), bset);
}

H8_OP(op71)
{
  /** BNOT #xx:3, Rd */
  h8_byte_t immediate;

  immediate.u = system->dbus.bh;
  rs_rd_b(system, immediate, rd_b(system, system->dbus.bl), bnot);
}

H8_OP(op72)
{
  /** BCLR #xx:3, Rd */
  h8_byte_t immediate;

  immediate.u = system->dbus.bh;
  rs_rd_b(system, immediate, rd_b(system, system->dbus.bl), bclr);
}

H8_OP(op73)
{
  /** BTST #xx:3, Rd */
  btst(system, rd_b(system, system->dbus.bl), system->dbus.bh);
}

H8_OP(op74)
{
  if (system->dbus.bh & B1000)
    /** BIOR #xx:3, Rd */
    bior(system, *rd_b(system, system->dbus.bl), system->dbus.bh & B0111);
  else
    /** BOR #xx:3, Rd */
    bor(system, *rd_b(system, system->dbus.bl), system->dbus.bh);
}

H8_OP(op75)
{
  if (system->dbus.bh & B1000)
    /** BIXOR #xx:3, Rd */
    bixor(system, *rd_b(system, system->dbus.bl), system->dbus.bh & B0111);
  else
    /** BXOR #xx:3, Rd */
    bxor(system, *rd_b(system, system->dbus.bl), system->dbus.bh);
}

H8_OP(op76)
{
  if (system->dbus.bh & B1000)
    /** BIAND #xx:3, Rd */
    biand(system, *rd_b(system, system->dbus.bl), system->dbus.bh & B0111);
  else
    /** BAND #xx:3, Rd */
    band(system, *rd_b(system, system->dbus.bl), system->dbus.bh);
}

H8_OP(op77)
{
  if (system->dbus.bh & B1000)
    /** BILD #xx:3, Rd */
    bild(system, *rd_b(system, system->dbus.bl), system->dbus.bh & B0111);
  else
    /** BLD #xx:3, Rd */
    bld(system, *rd_b(system, system->dbus.bl), system->dbus.bh);
}

H8_OP(op78)
{
  h8_instruction_t curr = system->dbus;
  h8_instruction_t op;
  h8_long_t address;
  unsigned r1, r2;

  h8_fetch(system);
  r1 = curr.bh;
  r2 = system->dbus.bl;
  op = system->dbus;

  /* The 24-bit displacement follows as two more instruction words */
  h8_fetch(system);
  address.h = system->dbus.bits;
  h8_fetch(system);
  address.l = system->dbus.bits;
  /* Sign-extend from 24 bits */
  address.a.u = (address.b.u & 0x80) ? 0xFF : 0x00;
  system->dbus = op;

  switch (system->dbus.a.u)
  {
  case 0x6A:
    if (system->dbus.bh == 0x2)
      /** MOV.B @(d:24, ERs), Rd */
      ms_rd_b(system, erd24(system, r1, address.i), rd_b(system, r2), mov_b);
    else if (system->dbus.bh == 0xA)
      /** MOV.B Rs, @(d:24, ERd) */
      rs_md_b(system, *rd_b(system, r2), erd24(system, r1, address.i), mov_b);
    else
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    break;
  case 0x6B:
    if (system->dbus.bh == 0x2)
      /** MOV.W @(d:24, ERs), Rd */
      ms_rd_w(system, erd24(system, r1, address.i), rd_w(system, r2), mov_w);
    else if (system->dbus.bh == 0xA)
      /** MOV.W Rs, @(d:24, ERd) */
      rs_md_w(system, *rd_w(system, r2), erd24(system, r1, address.i), mov_w);
    else
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op79)
{
  h8_instruction_t curr = system->dbus;

  h8_fetch(system);
  switch (curr.bh)
  {
  case 0:
    /** MOV.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), mov_w);
    break;
  case 1:
    /** ADD.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), add_w);
    break;
  case 2:
    /** CMP.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), cmp_w);
    break;
  case 3:
    /** SUB.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), sub_w);
    break;
  case 4:
    /** OR.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), or_w);
    break;
  case 5:
    /** XOR.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), xor_w);
    break;
  case 6:
    /** AND.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), and_w);
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op7a)
{
  h8_long_t imm = h8_read_l(system, system->cpu.pc);

  system->cpu.pc += sizeof(h8_long_t);
  switch (system->dbus.bh)
  {
  case 0:
    /** MOV.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), mov_l);
    break;
  case 1:
    /** ADD.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), add_l);
    break;
  case 2:
    /** CMP.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), cmp_l);
    break;
  case 3:
    /** SUB.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), sub_l);
    break;
  case 4:
    /** OR.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), or_l);
    break;
  case 5:
    /** XOR.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), xor_l);
    break;
  case 6:
    /** AND.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), and_l);
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op7d)
{
  h8_word_t func = system->dbus.bits;

  h8_fetch(system);
  if (system->dbus.ah == 0x6)
  {
    switch (system->dbus.al)
    {
    case 0x0:
      /** BSET Rs, @ERd */
      rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.h), bset);
      break;
    case 0x1:
      /** BNOT Rs, @ERd */
      rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.h), bnot);
      break;
    case 0x2:
      /** BCLR Rs, @ERd */
      rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.h), bclr);
      break;
    case 0x7:
    {
      h8_byte_t immediate;

      /* Bit 3 of the immediate byte selects BIST; the bit number is 0-7 */
      immediate.u = system->dbus.bh & B0111;
      if (system->dbus.bh & B1000)
        /** BIST #xx:3, @ERd */
        rs_md_b(system, immediate, er(system, func.l.h), bist);
      else
        /** BST #xx:3, @ERd */
        rs_md_b(system, immediate, er(system, func.l.h), bst);
      break;
    }
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else if (system->dbus.ah == 0x7)
  {
    h8_byte_t immediate;

    immediate.u = system->dbus.bh;
    switch (system->dbus.al)
    {
    case 0x0:
      /** BSET #xx:3, @ERd */
      rs_md_b(system, immediate, er(system, func.l.h), bset);
      break;
    case 0x1:
      /** BNOT #xx:3, @ERd */
      rs_md_b(system, immediate, er(system, func.l.h), bnot);
      break;
    case 0x2:
      /** BCLR #xx:3, @ERd */
      rs_md_b(system, immediate, er(system, func.l.h), bclr);
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op7c)
{
  h8_u8 erd = system->dbus.bh;
  h8_byte_t val;

  h8_fetch(system);
  if (erd & B1000)
  {
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    return;
  }
  val = h8_read_b(system, er(system, erd));
  if (system->dbus.a.u == 0x63)
    /** BTST Rn, @ERd */
    btst(system, &val, rd_b(system, system->dbus.bh)->u);
  else if (system->dbus.ah == 0x7)
  {
    switch (system->dbus.al)
    {
    case 0x3:
      /** BTST #xx:3, @ERd */
      btst(system, &val, system->dbus.bh);
      break;
    case 0x4:
      if (system->dbus.bh & B1000)
        /** BIOR #xx:3, @ERd */
        bior(system, val, system->dbus.bh & B0111);
      else
        /** BOR #xx:3, @ERd */
        bor(system, val, system->dbus.bh);
      break;
    case 0x5:
      if (system->dbus.bh & B1000)
        /** BIXOR #xx:3, @ERd */
        bixor(system, val, system->dbus.bh & B0111);
      else
        /** BXOR #xx:3, @ERd */
        bxor(system, val, system->dbus.bh);
      break;
    case 0x6:
      if (system->dbus.bh & B1000)
        /** BIAND #xx:3, @ERd */
        biand(system, val, system->dbus.bh & B0111);
      else
        /** BAND #xx:3, @ERd */
        band(system, val, system->dbus.bh);
      break;
    case 0x7:
      if (system->dbus.bh & B1000)
        /** BILD #xx:3, @ERd */
        bild(system, val, system->dbus.bh & B0111);
      else
        /** BLD #xx:3, @ERd */
        bld(system, val, system->dbus.bh);
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op7e)
{
  h8_byte_t aa = system->dbus.b;
  h8_byte_t val;

  h8_fetch(system);
  val = h8_read_b(system, aa8(aa));
  if (system->dbus.a.u == 0x63)
    /** BTST Rn, @aa:8 */
    btst(system, &val, rd_b(system, system->dbus.bh)->u);
  else if (system->dbus.ah == 0x7)
  {
    switch (system->dbus.al)
    {
    case 0x3:
      /** BTST #xx:3, @aa:8 */
      btst(system, &val, system->dbus.bh);
      break;
    case 0x4:
      if (system->dbus.bh & B1000)
        /** BIOR #xx:3, @aa:8 */
        bior(system, val, system->dbus.bh & B0111);
      else
        /** BOR #xx:3, @aa:8 */
        bor(system, val, system->dbus.bh);
      break;
    case 0x5:
      if (system->dbus.bh & B1000)
        /** BIXOR #xx:3, @aa:8 */
        bixor(system, val, system->dbus.bh & B0111);
      else
        /** BXOR #xx:3, @aa:8 */
        bxor(system, val, system->dbus.bh);
      break;
    case 0x6:
      if (system->dbus.bh & B1000)
        /** BIAND #xx:3, @aa:8 */
        biand(system, val, system->dbus.bh & B0111);
      else
        /** BAND #xx:3, @aa:8 */
        band(system, val, system->dbus.bh);
      break;
    case 0x7:
      if (system->dbus.bh & B1000)
        /** BILD #xx:3, @aa:8 */
        bild(system, val, system->dbus.bh & B0111);
      else
        /** BLD #xx:3, @aa:8 */
        bld(system, val, system->dbus.bh);
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op7f)
{
  h8_word_t func = system->dbus.bits;

  h8_fetch(system);
  if (system->dbus.ah == 0x6)
  {
    switch (system->dbus.al)
    {
    case 0x0:
      /** BSET Rs, @aa:8 */
      rs_md_b(system, *rd_b(system, system->dbus.bh), aa8(func.l), bset);
      break;
    case 0x1:
      /** BNOT Rs, @aa:8 */
      rs_md_b(system, *rd_b(system, system->dbus.bh), aa8(func.l), bnot);
      break;
    case 0x2:
      /** BCLR Rs, @aa:8 */
      rs_md_b(system, *rd_b(system, system->dbus.bh), aa8(func.l), bclr);
      break;
    case 0x7:
    {
      h8_byte_t immediate;

      /* Bit 3 of the immediate byte selects BIST; the bit number is 0-7 */
      immediate.u = system->dbus.bh & B0111;
      if (system->dbus.bh & B1000)
        /** BIST #xx:3, @aa:8 */
        rs_md_b(system, immediate, aa8(func.l), bist);
      else
        /** BST #xx:3, @aa:8 */
        rs_md_b(system, immediate, aa8(func.l), bst);
      break;
    }
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else if (system->dbus.ah == 0x7)
  {
    h8_byte_t immediate;

    immediate.u = system->dbus.bh;
    switch (system->dbus.al)
    {
    case 0x0:
      /** BSET #xx:3, @aa:8 */
      rs_md_b(system, immediate, aa8(func.l), bset);
      break;
    case 0x1:
      /** BNOT #xx:3, @aa:8 */
      rs_md_b(system, immediate, aa8(func.l), bnot);
      break;
    case 0x2:
      /** BCLR #xx:3, @aa:8 */
      rs_md_b(system, immediate, aa8(func.l), bclr);
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

#define OP8X(al, reg) \
void op8##al(h8_system_t *system) \
{ \
  /** ADD.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, add_b); \
}
H8_UNROLL(OP8X)

#define OP9X(al, reg) \
void op9##al(h8_system_t *system) \
{ \
  /** ADDX.B #xx:8, Rd */ \
  addx(system, &reg, system->dbus.b); \
}
H8_UNROLL(OP9X)

#define OPAX(al, reg) \
void opa##al(h8_system_t *system) \
{ \
  /** CMP.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, cmp_b); \
}
H8_UNROLL(OPAX)

#define OPBX(al, reg) \
void opb##al(h8_system_t *system) \
{ \
  /** SUBX.B #xx:8, Rd */ \
  subx(system, &reg, system->dbus.b); \
}
H8_UNROLL(OPBX)

#define OPCX(al, reg) \
void opc##al(h8_system_t *system) \
{ \
  /** OR.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, or_b); \
}
H8_UNROLL(OPCX)

#define OPDX(al, reg) \
void opd##al(h8_system_t *system) \
{ \
  /** XOR.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, xor_b); \
}
H8_UNROLL(OPDX)

#define OPEX(al, reg) \
void ope##al(h8_system_t *system) \
{ \
  /** AND.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, and_b); \
}
H8_UNROLL(OPEX)

#define OPFX(al, reg) \
void opf##al(h8_system_t *system) \
{ \
  /** MOV.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, mov_b); \
}
H8_UNROLL(OPFX)

void h8_init(h8_system_t *system)
{
  /* Setup default non-zero values */
  system->cpu.ccr.flags.i = 1;

  system->vmem.parts.io1.ssu.sscrh.flags.solp = 1;
  system->vmem.parts.io1.ssu.sssr.flags.tdre = 1;

  h8_tw_init(system);
  h8_tb1_init(system);
  h8_comparator_init(system);
  h8_rtc_init(system);
  h8_sci3_init(system);
  h8_ir_init(&system->ir);
  h8_power_init(system);

  /* 5.1: Power-down mode and module standby settings */
  system->vmem.raw[H8_REG_SYSCR1].u = H8_SYSCR1_INITIAL;
  system->vmem.raw[H8_REG_SYSCR2].u = H8_SYSCR2_INITIAL;
  system->vmem.raw[H8_REG_CKSTPR1].u = H8_CKSTPR1_INITIAL;

  /* 3.4, 8.5.2, 8.6.2: External interrupt pins */
  system->vmem.raw[H8_REG_IEGR].u = 0;
  system->vmem.raw[H8_REG_PMRB].u = 0;
  system->vmem.raw[H8_REG_PFCR].u = 0;
  system->irq.pin_level[0] = FALSE;
  system->irq.pin_level[1] = FALSE;

  /* 3.4: All interrupts disabled with no requests pending */
  system->vmem.raw[H8_REG_IENR1].u = 0;
  system->vmem.raw[H8_REG_IENR2].u = 0;
  system->vmem.raw[H8_REG_IRR1].u = 0;
  system->vmem.raw[H8_REG_IRR2].u = 0;
  system->vmem.raw[H8_REG_CKSTPR2].u = H8_CKSTPR2_INITIAL;

  /* 14.3.6: SCR3 resets to 0, with all SCI3 interrupt requests disabled */
  system->vmem.parts.io2.aec_sci3.scr3.raw.u = 0;
  system->vmem.parts.io2.aec_sci3.brr3.u = 0xFF;
  system->vmem.parts.io2.aec_sci3.tdr3.u = 0xFF;
  system->vmem.parts.io2.aec_sci3.ssr3.raw.u = 0x84;

  system->vmem.parts.io2.wdt.tmwd.flags.reserved = B1111;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.b0wi = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.wdon = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.b2wi = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.b4wi = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.b6wi = 1;

  system->vmem.parts.io2.adc.adsr.flags.reserved = B00111111;

  system->clock = H8_CLOCK_DEFAULT;
  system->run_budget = 0;

  /* Jump to program entrypoint */
  system->cpu.pc = h8_read_w(system, 0).u;
  system->step_states = 0;
}

/**
 * Returns the states an instruction spends beyond the bus accesses the
 * emulator performs as it executes: prefetching the next instruction after a
 * branch, and internal operations. Instruction fetches and data accesses are
 * counted as they happen. See Appendix A.3, table A.4.
 * Must be called after the first instruction word is fetched.
 */
static unsigned h8_extra_states(h8_system_t *system)
{
  const h8_instruction_t op = system->dbus;

  switch (op.a.u)
  {
  case 0x01:
  {
    h8_word_t next = h8_peek_w(system, system->cpu.pc);

    switch (op.b.u)
    {
    case 0x00:
    case 0x40:
      /* MOV.L, LDC and STC with @ERs+ or @-ERd, including PUSH.L and POP.L */
      return next.h.u == 0x6D ? 2 : 0;
    case 0xC0:
    case 0xD0:
      /* MULXS and DIVXS: 12 internal states for bytes, 20 for words */
      return next.h.u & 0x02 ? 20 : 12;
    default:
      return 0;
    }
  }
  case 0x50:
  case 0x51:
    /* MULXU.B, DIVXU.B */
    return 12;
  case 0x52:
  case 0x53:
    /* MULXU.W, DIVXU.W */
    return 20;
  case 0x54:
  case 0x56:
    /* RTS, RTE: prefetch and 2 internal states */
    return 4;
  case 0x57:
    /* TRAPA: prefetch and 4 internal states */
    return 6;
  case 0x55:
  case 0x59:
  case 0x5D:
    /* BSR d:8, JMP @ERn, JSR @ERn: prefetch */
    return 2;
  case 0x58:
  case 0x5A:
  case 0x5C:
  case 0x5E:
    /* Bcc d:16, JMP @aa:24, BSR d:16, JSR @aa:24: 2 internal states */
    return 2;
  case 0x5B:
    /* JMP @@aa:8: prefetch and 2 internal states; the address read is counted */
    return 4;
  case 0x5F:
    /* JSR @@aa:8: prefetch; the address read and stack write are counted */
    return 2;
  case 0x6C:
  case 0x6D:
    /* MOV with @ERs+ or @-ERd, including PUSH.W and POP.W */
    return 2;
  default:
    /* Bcc d:8 always prefetches the next instruction */
    return op.ah == 0x4 ? 2 : 0;
  }
}

static H8_OP_T funcs[256] =
{
  op00, op01, op02, op03, op04, op05, op06, op07,
  op08, op09, op0a, op0b, op0c, op0d, op0e, op0f,
  op10, op11, op12, op13, op14, op15, op16, op17,
  op18, op19, op1a, op1b, op1c, op1d, op1e, op1f,
  op20, op21, op22, op23, op24, op25, op26, op27,
  op28, op29, op2a, op2b, op2c, op2d, op2e, op2f,
  op30, op31, op32, op33, op34, op35, op36, op37,
  op38, op39, op3a, op3b, op3c, op3d, op3e, op3f,
  op40, op41, op42, op43, op44, op45, op46, op47,
  op48, op49, op4a, op4b, op4c, op4d, op4e, op4f,
  op50, op51, op52, op53, op54, op55, op56, op57,
  op58, op59, op5a, op5b, op5c, op5d, op5e, op5f,
  op60, op61, op62, op63, op64, op65, op66, op67,
  op68, op69, op6a, op6b, op6c, op6d, op6e, op6f,
  op70, op71, op72, op73, op74, op75, op76, op77,
  op78, op79, op7a, NULL, op7c, op7d, op7e, op7f,
  op80, op81, op82, op83, op84, op85, op86, op87,
  op88, op89, op8a, op8b, op8c, op8d, op8e, op8f,
  op90, op91, op92, op93, op94, op95, op96, op97,
  op98, op99, op9a, op9b, op9c, op9d, op9e, op9f,
  opa0, opa1, opa2, opa3, opa4, opa5, opa6, opa7,
  opa8, opa9, opaa, opab, opac, opad, opae, opaf,
  opb0, opb1, opb2, opb3, opb4, opb5, opb6, opb7,
  opb8, opb9, opba, opbb, opbc, opbd, opbe, opbf,
  opc0, opc1, opc2, opc3, opc4, opc5, opc6, opc7,
  opc8, opc9, opca, opcb, opcc, opcd, opce, opcf,
  opd0, opd1, opd2, opd3, opd4, opd5, opd6, opd7,
  opd8, opd9, opda, opdb, opdc, opdd, opde, opdf,
  ope0, ope1, ope2, ope3, ope4, ope5, ope6, ope7,
  ope8, ope9, opea, opeb, opec, oped, opee, opef,
  opf0, opf1, opf2, opf3, opf4, opf5, opf6, opf7,
  opf8, opf9, opfa, opfb, opfc, opfd, opfe, opff
};

void h8_step(h8_system_t *system)
{
  h8_power_mode mode = system->power.mode;
  h8_bool phi_running, sub_running;
  unsigned vector;
  unsigned i;

  if (system->error_code)
    return;

  system->step_states = 0;
  system->power.wait_states = 0;

  /* 5.2: External interrupt pins are sampled in every mode */
  h8_interrupt_pins(system);

  /**
   * 3.6: Interrupts are accepted between instructions while the I bit in CCR
   * is clear. Exception handling takes the place of an instruction.
   */
  vector = system->cpu.ccr.flags.i ? 0 : h8_interrupt_pending(system);

  if (h8_power_halted(system) && !vector)
  {
    /**
     * 5.2: The CPU stays halted until an enabled interrupt is requested while
     * the I bit is clear. Let a short time pass for the peripherals, but stop
     * at the next Timer W event so its output edges land on time.
     */
    phi_running = h8_power_phi_running(system);
    system->step_states = h8_tw_states_until_event(system,
      h8_power_idle_states(system), phi_running,
      phi_running || system->power.mode == H8_POWER_SUBSLEEP);
  }
  else
  {
    if (h8_power_halted(system))
    {
      /* The interrupt clears the power-down mode before it is handled */
      h8_power_wake(system);
      mode = system->power.mode;
    }

    if (vector)
    {
      h8_exception(system, vector);

      /**
       * Table 3.4: Saving PC and CCR and the vector fetch are counted as
       * accesses; add the instruction fetch and internal processing.
       */
      system->step_states += 8;
    }
    else
    {
      H8_OP_T function;

#if H8_SAFETY
      if (system->cpu.pc > 0xFFFF || system->cpu.pc & 1)
        H8_ERROR(H8_DEBUG_BAD_PC)
#endif

      h8_fetch(system);
      system->step_states += h8_extra_states(system);

      function = funcs[system->dbus.a.u];

      if (function)
        function(system);
      else
      {
        h8_log(H8_LOG_ERROR, H8_LOG_CPU, "Undefined opcode - %02X%02X at %04X",
               system->dbus.a.u, system->dbus.b.u, system->cpu.pc - 2);
        H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
      }

      if (system->error_code)
        h8_log(H8_LOG_ERROR, H8_LOG_CPU, "CRITICAL EMULATION ERROR %u at %u",
               system->error_code, system->error_line);

#if H8_PROFILING
      system->instructions++;
#endif
    }

    /* Convert from the clock the CPU ran on, and add any stabilization wait */
    system->step_states = h8_power_to_phi(system, mode, system->step_states) +
                          system->power.wait_states;
  }

  /* Peripherals and devices run in parallel with the CPU */
  phi_running = h8_power_phi_running(system);
  sub_running = h8_power_sub_running(system);
  for (i = 0; i < system->device_count; i++)
    if (system->devices[i].step)
      system->devices[i].step(&system->devices[i], system->step_states,
                              system->clock);

  /* Table 5.3: Timer W counts on phiW in subactive and subsleep, not watch */
  h8_tw_run(system, system->step_states, phi_running,
            phi_running || system->power.mode == H8_POWER_SUBACTIVE ||
            system->power.mode == H8_POWER_SUBSLEEP);
  h8_tb1_run(system, system->step_states, phi_running, sub_running);
  if (sub_running)
    h8_rtc_run(system, system->step_states, phi_running);
  h8_ir_run(system, system->step_states);
  if (phi_running)
    h8_sci3_run(system, system->step_states);
}

void h8_run(h8_system_t *system)
{
  system->run_budget += (h8_s32)(system->clock / 60);
  while (system->run_budget > 0 && !system->error_code)
  {
    h8_step(system);
    system->run_budget -= (h8_s32)system->step_states;
  }
}

#if H8_TESTS

#include <stdio.h>
#include <stdlib.h>

#define H8_TEST_FAIL(a) { printf("Test failed in %s on line %u.\n", \
  __FILE__, \
  __LINE__); \
  exit(a); }

void h8_test_add(void)
{
  h8_system_t system = {0};
  h8_byte_t dst_byte, src_byte, result;

  /* 10 + 5 = 15 */
  dst_byte.u = 10;
  src_byte.u = 5;
  result = add_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 0 ||
      system.cpu.ccr.flags.n != 0 ||
      result.u != 15)
    H8_TEST_FAIL(1)

  /* 255 + 1 = 0 */
  dst_byte.u = 255;
  src_byte.u = 1;
  result = add_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 1 ||
      system.cpu.ccr.flags.z != 1 ||
      system.cpu.ccr.flags.v != 0 ||
      system.cpu.ccr.flags.n != 0 ||
      result.u != 0)
    H8_TEST_FAIL(2)

  /* 127 + 1 = 128 */
  dst_byte.u = 127;
  src_byte.u = 1;
  result = add_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 1 ||
      system.cpu.ccr.flags.n != 1 ||
      result.u != 128)
    H8_TEST_FAIL(3)

  /* -128 + (-1) = 127 */
  dst_byte.i = -128;
  src_byte.i = -1;
  result = add_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 1 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 1 ||
      system.cpu.ccr.flags.n != 0 ||
      result.u != 127)
    H8_TEST_FAIL(4)

  /* 0 + 0 = 0 */
  dst_byte.u = 0;
  src_byte.u = 0;
  result = add_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.h != 0 ||
      system.cpu.ccr.flags.z != 1 ||
      system.cpu.ccr.flags.v != 0 ||
      system.cpu.ccr.flags.n != 0 ||
      result.u != 0)
    H8_TEST_FAIL(5)

  /* 127 + 127 = 254 */
  dst_byte.u = 127;
  src_byte.u = 127;
  result = add_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 1 ||
      system.cpu.ccr.flags.n != 1 ||
      result.u != 254)
    H8_TEST_FAIL(7)

  printf("Addition test passed!\n");
}

int h8_test_bit_manip(void)
{
  h8_system_t system = {0};
  h8_byte_t b, bits, result;

  b.u = 0;
  bits.u = B10000010;
  result = bset(&system, b, bits);
  if (result.u != B00000100)
    H8_TEST_FAIL(1)

  bits.u = B00000010;
  result = bclr(&system, b, bits);
  if (result.u != 0)
    H8_TEST_FAIL(2)

  /** @todo Tests for more functions */

  printf("Bit manipulation test passed!\n");

  return 0;
}

void h8_test_bit_order(void)
{
  h8_ccr_t ccr_test;

  ccr_test.raw.u = 0;
  ccr_test.flags.c = 1;
  if (ccr_test.raw.u != 1)
  {
    printf("This program does not support the current bit ordering.\n"
           "Try redefining H8_REVERSE_BITFIELDS.\n");
    H8_TEST_FAIL(1)
  }

  printf("Bit ordering test passed!\n");
}

void h8_test_division(void)
{
  h8_system_t system = {0};
  h8_byte_t b;
  h8_word_t w;
  h8_long_t l;

  w.u = 358;
  b.u = 2;
  w = divxu_b(&system, w, b);
  if (w.l.u != 179 || w.h.u != 0)
    H8_TEST_FAIL(1)

  l.u = 1234567;
  w.u = 890;
  l = divxu_w(&system, l, w);
  if (l.l.u != 1387 || l.h.u != 137)
    H8_TEST_FAIL(2)

  l.u = 999;
  w.u = 0;
  divxu_w(&system, l, w);
  if (system.cpu.ccr.flags.z != 1)
    H8_TEST_FAIL(3)

  printf("Division test passed!\n");
}

void h8_test_shift(void)
{
  h8_system_t system = {0};
  h8_byte_t b;
  h8_word_t w;
  h8_long_t l;

  b.u = 0x55;
  shal_b(&system, &b);
  if (b.u != 0xAA || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(1)

  b.u = 0x80;
  shal_b(&system, &b);
  if (b.u != 0x00 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(2)

  w.u = 0x1234;
  shal_w(&system, &w);
  if (w.u != 0x2468 || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(3)

  w.u = 0x8000;
  shal_w(&system, &w);
  if (w.u != 0x0000 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(4)

  l.u = 0x00010000;
  shal_l(&system, &l);
  if (l.u != 0x00020000 || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(5)

  l.u = 0x80000000;
  shal_l(&system, &l);
  if (l.u != 0x00000000 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(6)

  b.u = 0x80;
  shar_b(&system, &b);
  if (b.u != 0xC0 || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(7)

  b.u = 0x01;
  shar_b(&system, &b);
  if (b.u != 0x00 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(8)

  w.u = 0x8000;
  shar_w(&system, &w);
  if (w.u != 0xC000 || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(9)

  w.u = 0x0001;
  shar_w(&system, &w);
  if (w.u != 0x0000 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(10)

  l.u = 0x80000000;
  shar_l(&system, &l);
  if (l.u != 0xC0000000 || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(11)

  l.u = 0x00000001;
  shar_l(&system, &l);
  if (l.u != 0x00000000 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(12)

  b.u = 0x55;
  shll_b(&system, &b);
  if (b.u != 0xAA || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(13)

  b.u = 0x80;
  shll_b(&system, &b);
  if (b.u != 0x00 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(14)

  b.u = 0xAA;
  shlr_b(&system, &b);
  if (b.u != 0x55 || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(15)

  b.u = 0x01;
  shlr_b(&system, &b);
  if (b.u != 0x00 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(16)

  b.u = 0x85;
  rotl_b(&system, &b);
  if (b.u != 0x0B || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(17)

  w.u = 0x8001;
  rotl_w(&system, &w);
  if (w.u != 0x0003 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(18)

  b.u = 0x85;
  rotr_b(&system, &b);
  if (b.u != 0xC2 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(19)

  w.u = 0x8001;
  rotr_w(&system, &w);
  if (w.u != 0xC000 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(20)

  system.cpu.ccr.flags.c = 1;
  b.u = 0x7F;
  rotxl_b(&system, &b);
  if (b.u != 0xFF || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(21)

  system.cpu.ccr.flags.c = 0;
  b.u = 0x80;
  rotxl_b(&system, &b);
  if (b.u != 0x00 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(22)

  system.cpu.ccr.flags.c = 1;
  b.u = 0xFE;
  rotxr_b(&system, &b);
  if (b.u != 0xFF || system.cpu.ccr.flags.c != 0)
    H8_TEST_FAIL(23)

  system.cpu.ccr.flags.c = 0;
  b.u = 0x01;
  rotxr_b(&system, &b);
  if (b.u != 0x00 || system.cpu.ccr.flags.c != 1)
    H8_TEST_FAIL(24)

  printf("Shift tests passed!\n");
}

/**
 * Tests to ensure the size of the memory map does not exceed a 16-bit address
 * space, that memory regions are of the expected size, and that select CPU
 * registers are in the expected locations.
 */
void h8_test_size(void)
{
  h8_system_t system;

  if (sizeof(system.vmem.parts.io1) != 0xE0)
    H8_TEST_FAIL(1)
  if (sizeof(system.vmem.parts.io2) != 0x80)
    H8_TEST_FAIL(2)
  if (sizeof(system.vmem) != 0x10000)
    H8_TEST_FAIL(3)
  if ((void*)&system.vmem.raw[H8_MEMORY_REGION_IO1] !=
      (void*)&system.vmem.parts.io1)
    H8_TEST_FAIL(4)
  if ((void*)&system.vmem.raw[H8_MEMORY_REGION_IO2] !=
      (void*)&system.vmem.parts.io2)
    H8_TEST_FAIL(5)
  if ((void*)&system.vmem.raw[H8_REG_IRCR] !=
      (void*)&system.vmem.parts.io2.aec_sci3.ircr)
    H8_TEST_FAIL(6)
  if ((void*)&system.vmem.raw[0xffb0] !=
      (void*)&system.vmem.parts.io2.wdt.tmwd)
    H8_TEST_FAIL(7)
  if ((void*)&system.vmem.raw[0xffbe] !=
      (void*)&system.vmem.parts.io2.adc.amr)
    H8_TEST_FAIL(8)

  printf("Size test passed!\n");
}

void h8_test_sub(void)
{
  h8_system_t system = {0};
  h8_byte_t dst_byte, src_byte, result;

  /* 15 - 5 = 10 */
  dst_byte.u = 15;
  src_byte.u = 5;
  result = sub_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 0 ||
      system.cpu.ccr.flags.n != 0 ||
      result.u != 10)
    H8_TEST_FAIL(1)

  /* 10 - 10 = 0 */
  dst_byte.u = 10;
  src_byte.u = 10;
  result = sub_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.z != 1 ||
      system.cpu.ccr.flags.v != 0 ||
      system.cpu.ccr.flags.n != 0 ||
      result.u != 0)
    H8_TEST_FAIL(2)

  /* 1 - 2 = -1 (255) -- Underflow */
  dst_byte.u = 1;
  src_byte.u = 2;
  result = sub_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 1 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 0 ||
      system.cpu.ccr.flags.n != 1 ||
      result.u != 255)
    H8_TEST_FAIL(3)

  /* 128 - 1 = 127 */
  dst_byte.u = 128;
  src_byte.u = 1;
  result = sub_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 1 ||
      system.cpu.ccr.flags.n != 0 ||
      result.u != 127)
    H8_TEST_FAIL(4)

  /* 0 - 1 = -1 (255) */
  dst_byte.u = 0;
  src_byte.u = 1;
  result = sub_b(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 1 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 0 ||
      system.cpu.ccr.flags.n != 1 ||
      result.u != 255)
    H8_TEST_FAIL(5)

  printf("Subtraction test passed!\n");
}

/** Copies an instruction stream into memory at an address and points PC at it */
static void h8_test_load(h8_system_t *system, unsigned address,
                         const h8_u8 *code, unsigned size)
{
  unsigned i;

  for (i = 0; i < size; i++)
    system->vmem.raw[address + i].u = code[i];
  system->cpu.pc = address;
}

void h8_test_multiply(void)
{
  h8_word_t w;
  h8_byte_t b;
  h8_long_t l;

  /* MULXU.B is unsigned: 0x01 * 0xC0 = 0x00C0, not 0xFFC0 */
  w.u = 0xC001;
  b.u = 0xC0;
  if (mulxu_b(w, b).u != 0x00C0)
    H8_TEST_FAIL(1)

  /* 0xFF * 0xFF = 0xFE01 */
  w.u = 0x00FF;
  b.u = 0xFF;
  if (mulxu_b(w, b).u != 0xFE01)
    H8_TEST_FAIL(2)

  /* MULXU.W: 0xFFFF * 0x8000 = 0x7FFF8000 */
  l.u = 0x1234FFFF;
  w.u = 0x8000;
  if (mulxu_w(l, w).u != 0x7FFF8000)
    H8_TEST_FAIL(3)

  printf("Multiplication test passed!\n");
}

void h8_test_timer_w(void)
{
  static h8_system_t system;
  h8_tw_t *tw = &system.vmem.parts.io1.tw;
  h8_byte_t b;
  unsigned i;

  h8_tw_init(&system);
  system.clock = H8_CLOCK_DEFAULT;
  system.vmem.raw[H8_REG_CKSTPR2].u = H8_CKSTPR2_TWCKSTP;

  /* Count on clock, clear on compare match A with GRA = 3: a cycle of 4 */
  b.u = 0x80;
  h8_tw_tcrw_out(&system, &tw->tcrw.raw, b);
  tw->gra.h.u = 0;
  tw->gra.l.u = 3;
  b.u = 0x80;
  h8_tw_tmrw_out(&system, &tw->tmrw.raw, b);

  /* The match is signalled when TCNT is clocked while it equals GRA */
  h8_tw_run(&system, 3, TRUE, TRUE);
  if (tw->tcnt.l.u != 3 || tw->tsrw.flags.imfa)
    H8_TEST_FAIL(1)
  h8_tw_run(&system, 1, TRUE, TRUE);
  if (tw->tcnt.l.u != 0 || !tw->tsrw.flags.imfa)
    H8_TEST_FAIL(2)

  /* TSRW flags are cleared by writing 0, but cannot be set by writing 1 */
  b.u = 0xFE;
  h8_tw_tsrw_out(&system, &tw->tsrw.raw, b);
  if (tw->tsrw.flags.imfa || tw->tsrw.raw.u != H8_TSRW_RESERVED)
    H8_TEST_FAIL(3)
  b.u = 0xFF;
  h8_tw_tsrw_out(&system, &tw->tsrw.raw, b);
  if (tw->tsrw.raw.u != H8_TSRW_RESERVED)
    H8_TEST_FAIL(4)

  /* PWM on FTIOB with TOB = 1: high from match A, low from match B */
  b.u = 0x82;
  h8_tw_tcrw_out(&system, &tw->tcrw.raw, b);
  tw->grb.h.u = 0;
  tw->grb.l.u = 1;
  b.u = 0x81;
  h8_tw_tmrw_out(&system, &tw->tmrw.raw, b);
  tw->tcnt.l.u = 0;
  if (!system.timer_w.output[H8_TW_PIN_B])
    H8_TEST_FAIL(5)
  h8_tw_run(&system, 2, TRUE, TRUE);
  if (system.timer_w.output[H8_TW_PIN_B])
    H8_TEST_FAIL(6)
  h8_tw_run(&system, 2, TRUE, TRUE);
  if (!system.timer_w.output[H8_TW_PIN_B])
    H8_TEST_FAIL(7)

  /* With equal period and duty, the output never changes */
  tw->grb.l.u = 3;
  for (i = 0; i < 8; i++)
  {
    h8_tw_run(&system, 1, TRUE, TRUE);
    if (!system.timer_w.output[H8_TW_PIN_B])
      H8_TEST_FAIL(8)
  }

  printf("Timer W test passed!\n");
}

void h8_test_interrupt(void)
{
  static h8_system_t system;
  /* NOP; RTE at the handler */
  static const h8_u8 code[] = { 0x00, 0x00 };
  static const h8_u8 handler[] = { 0x56, 0x70 };
  h8_tw_t *tw = &system.vmem.parts.io1.tw;

  h8_tw_init(&system);
  h8_test_load(&system, 0x0200, handler, sizeof(handler));
  h8_test_load(&system, 0x0100, code, sizeof(code));
  system.vmem.raw[H8_VECTOR_TIMER_W * 2].u = 0x02;
  system.vmem.raw[H8_VECTOR_TIMER_W * 2 + 1].u = 0x00;
  system.cpu.regs[7].er.u = 0xFF80;
  system.cpu.ccr.raw.u = 0x85;

  /* Requested and enabled, but masked by the I bit */
  tw->tsrw.flags.imfa = 1;
  tw->tierw.flags.imiea = 1;
  if (h8_interrupt_pending(&system) != H8_VECTOR_TIMER_W)
    H8_TEST_FAIL(1)
  h8_step(&system);
  if (system.cpu.pc != 0x0102)
    H8_TEST_FAIL(2)

  /* Unmasked: PC and CCR are pushed and the vector is taken */
  system.cpu.ccr.raw.u = 0x05;
  system.cpu.pc = 0x0100;
  h8_step(&system);
  if (system.cpu.pc != 0x0200 || !system.cpu.ccr.flags.i ||
      system.cpu.regs[7].er.u != 0xFF7C ||
      system.vmem.raw[0xFF7C].u != 0x05 || system.vmem.raw[0xFF7E].u != 0x01 ||
      system.vmem.raw[0xFF7F].u != 0x00)
    H8_TEST_FAIL(3)

  /* RTE restores CCR and PC */
  h8_step(&system);
  if (system.cpu.pc != 0x0100 || system.cpu.ccr.raw.u != 0x05 ||
      system.cpu.regs[7].er.u != 0xFF80)
    H8_TEST_FAIL(4)

  printf("Interrupt test passed!\n");
}

#endif

void h8_test(void)
{
#if H8_TESTS
  h8_test_add();
  h8_test_bit_manip();
  h8_test_bit_order();
  h8_test_division();
  h8_test_interrupt();
  h8_test_multiply();
  h8_test_shift();
  h8_test_size();
  h8_test_sub();
  h8_test_timer_w();
#endif
}
