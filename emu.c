#include <stdio.h>
#include <string.h>

#include "system.h"

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
  system->cpu.ccr.flags.v = ((a > 0 && b > 0 && c < 0) ||
                            (a < 0 && b < 0 && c > 0));
}

/**
 * =============================================================================
 * Assembly function implementations
 * =============================================================================
 */

#define H8_MOV_OP(name, type, hcbit) \
/** Moves a type from one location to another */ \
type mov##name(h8_system_t *system, type dst, const type src) \
{ \
  H8_UNUSED(dst); \
  ccr_zn(system, src.i); \
  return src; \
}
H8_MOV_OP(b, h8_byte_t, 4)
H8_MOV_OP(w, h8_word_t, 12)
H8_MOV_OP(l, h8_long_t, 28)

#define H8_ADD_OP(name, type, hcbit) \
type add##name(h8_system_t *system, type dst, const type src) \
{ \
  type result; \
  result.i = dst.i + src.i; \
  system->cpu.ccr.flags.c = result.u < src.u; \
  ccr_zn(system, result.i); \
  ccr_v(system, dst.i, src.i, result.i); \
  system->cpu.ccr.flags.h = (result.u & (1 << hcbit)) != (dst.u & (1 << hcbit)); \
  return result; \
}
H8_ADD_OP(b, h8_byte_t, 4)
H8_ADD_OP(w, h8_word_t, 12)
H8_ADD_OP(l, h8_long_t, 28)

#define H8_SUB_OP(name, type, hcbit) \
type sub##name(h8_system_t *system, type dst, const type src) \
{ \
  type result; \
  result.i = dst.i - src.i; \
  system->cpu.ccr.flags.c = result.u < src.u; \
  ccr_zn(system, result.i); \
  ccr_v(system, dst.i, src.i, result.i); \
  system->cpu.ccr.flags.h = (result.u & (1 << hcbit)) != (dst.u & (1 << hcbit)); \
  return result; \
}
H8_SUB_OP(b, h8_byte_t, 4)
H8_SUB_OP(w, h8_word_t, 12)
H8_SUB_OP(l, h8_long_t, 28)

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
  dst->h.u = (dst->l.u & B10000000) ? 0xFF : 0x00;
  system->cpu.ccr.flags.n = 0; /** @todo was this right? */
  system->cpu.ccr.flags.z = dst->u == 0;
  system->cpu.ccr.flags.v = 0;
}

static void exts_l(h8_system_t *system, h8_long_t *dst)
{
  dst->h.u = (dst->c.u & B10000000) ? 0xFFFF : 0x0000;
  system->cpu.ccr.flags.n = (dst->c.u & B10000000) ? 1 : 0;
  system->cpu.ccr.flags.z = dst->u == 0;
  system->cpu.ccr.flags.v = 0;
}

/**
 * Register IO functions
 */

#define H8_IO_DUMMY_IN H8_UNUSED(system); H8_UNUSED(byte);
#define H8_IO_DUMMY_OUT H8_UNUSED(system); H8_UNUSED(byte); H8_UNUSED(value);

H8_OUT(pdr1o)
{
  unsigned i;

  for (i = 0; i < system->device_count; i++)
  {
    if (system->devices[i].port == H8_HOOKUP_PORT_1 &&
        !(system->devices[i].select & value.u))
    {
      system->ssu_device = &system->devices[i];
      *byte = value;
      return;
    }
  }
  system->ssu_device = NULL;
  *byte = value;
}

H8_OUT(pdr3o)
{
  unsigned i;

  for (i = 0; i < system->device_count; i++)
    if (system->devices[i].port == H8_HOOKUP_PORT_3 &&
        !(system->devices[i].select & value.u))
      system->ssu_device = &system->devices[i];

  *byte = value;
}

/**
 * 8.3.1 Port Data Register 8 (PDR8)
 */

H8_OUT(pdr8o)
{
  unsigned i;

  for (i = 0; i < system->device_count; i++)
    if (system->devices[i].port == H8_HOOKUP_PORT_8)
      system->devices[i].write(&system->devices[i], byte, value);

  *byte = value;
}

H8_OUT(pdr9o)
{
  unsigned i;

  for (i = 0; i < system->device_count; i++)
  {
    if (system->devices[i].port == H8_HOOKUP_PORT_9 &&
        !(system->devices[i].select & value.u))
    {
      system->ssu_device = &system->devices[i];
      *byte = value;
      return;
    }
  }
  system->ssu_device = NULL;
  *byte = value;
}

/**
 * 8.5.1 Port Data Register B (PDRB)
 */

H8_IN(pdrbi)
{
  unsigned i;

  for (i = 0; i < system->device_count; i++)
    if (system->devices[i].port == H8_HOOKUP_PORT_B)
      system->devices[i].read(&system->devices[i], byte);

  byte->u &= B00111111;
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
  if (system->ssu_device && system->ssu_device->read)
    system->ssu_device->read(system->ssu_device, byte);
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
  system->vmem.parts.io1.ssu.sssr.flags.tend = 0;
  system->vmem.parts.io1.ssu.sssr.flags.tdre = 0;
  system->vmem.parts.io1.ssu.sssr.flags.rdrf = 0;
  if (system->ssu_device && system->ssu_device->write)
  {
    system->ssu_device->write(system->ssu_device, byte, value);
    system->vmem.parts.io1.ssu.sssr.flags.tend = 1;
    system->vmem.parts.io1.ssu.sssr.flags.tdre = 1;
    system->vmem.parts.io1.ssu.sssr.flags.rdrf = 1;
  }
}

/**
 * 15.3.8 SS Shift Register (SSTRSR)
 * ????
 */

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
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
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
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFA0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFB0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFC0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFD0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, pdrbi, NULL,
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
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xF0E0 */
  NULL, NULL, NULL, NULL, sssro, NULL, NULL, NULL,
  NULL, ssrdro, NULL, sstdro, NULL, NULL, NULL, NULL,
  /* 0xF0F0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

  /* IO region 2 (0xFF80) */
  /* 0xFF80 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFF90 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFA0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFB0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFC0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFD0 */
  NULL, NULL, NULL, NULL, pdr1o, NULL, pdr3o, NULL,
  pdr8o, NULL, NULL, NULL, pdr9o, NULL, pdrbo, NULL,
  /* 0xFFE0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* 0xFFF0 */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
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
  if ((address >= 0xfb80 && address < 0xff80) || force)
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
  /*if (address >= 0xfb80 && address < 0xff80)*/
    system->reads[address & 0xFFFF] += 1;
#endif
  return &system->vmem.raw[address & 0xFFFF];
}

static H8_IN_T h8_register_in(h8_system_t *system, unsigned address)
{
  if (address >= H8_MEMORY_REGION_IO1 &&
      address < H8_MEMORY_REGION_IO1 + sizeof(system->vmem.parts.io1))
  {
    if (!reg_ins[address - H8_MEMORY_REGION_IO1])
      printf("Better not implement %04X input, BOY\n", address);
    return reg_ins[address - H8_MEMORY_REGION_IO1];
  }
  else if (address >= H8_MEMORY_REGION_IO2 &&
           address < H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io2))
  {
    if (!reg_ins[address - H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io1)])
      printf("Better not implement %04X input, BOY\n", address);
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
    if (!reg_outs[address - H8_MEMORY_REGION_IO1])
      printf("Better not implement %04X output, BOY\n", address);
    return reg_outs[address - H8_MEMORY_REGION_IO1];
  }
  else if (address >= H8_MEMORY_REGION_IO2 &&
           address < H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io2))
  {
    if (!reg_outs[address - H8_MEMORY_REGION_IO2 + sizeof(system->vmem.parts.io1)])
      printf("Better not implement %04X output, BOY\n", address);
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

  if (in)
    in(system, byte);

  return *byte;
}

static void h8_byte_out(h8_system_t *system, const unsigned address,
                        const h8_byte_t value)
{
  H8_OUT_T out = h8_register_out(system, address);
  h8_byte_t *byte = h8_find(system, address);

  if (out)
    out(system, byte, value);
  else
    *byte = value;
}

static h8_byte_t h8_read_b(h8_system_t *system, const unsigned address)
{
  return h8_byte_in(system, address);
}

static void h8_write_b(h8_system_t *system, const unsigned address,
                          const h8_byte_t value)
{
  h8_byte_out(system, address, value);
}

static h8_byte_t h8_peek_b(h8_system_t *system, const unsigned address)
{
  return *(h8_byte_t*)h8_find(system, address);
}

static void h8_poke_b(h8_system_t *system, const unsigned address,
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

  w.h = h8_byte_in(system, address);
  w.l = h8_byte_in(system, address + 1);

  return w;
}

/**
 * Writes a 16-bit word from native endianness to explicit big-endian memory.
 */
static void h8_write_w(h8_system_t *system, unsigned address, h8_word_t val)
{
  h8_byte_out(system, address, val.h);
  h8_byte_out(system, address + 1, val.l);
}

static h8_word_t h8_peek_w(h8_system_t *system, const unsigned address)
{
  h8_word_t w;

  w.h = h8_peek_b(system, address);
  w.l = h8_peek_b(system, address + 1);

  return w;
}

static void h8_poke_w(h8_system_t *system, const unsigned address,
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

  l.a = h8_byte_in(system, address);
  l.b = h8_byte_in(system, address + 1);
  l.c = h8_byte_in(system, address + 2);
  l.d = h8_byte_in(system, address + 3);

  return l;
}

static void h8_write_l(h8_system_t *system, const unsigned address,
                          const h8_long_t val)
{
  h8_byte_out(system, address, val.a);
  h8_byte_out(system, address + 1, val.b);
  h8_byte_out(system, address + 2, val.c);
  h8_byte_out(system, address + 3, val.d);
}

static h8_long_t h8_peek_l(h8_system_t *system, const unsigned address)
{
  h8_long_t l;

  l.a = h8_peek_b(system, address);
  l.b = h8_peek_b(system, address + 1);
  l.c = h8_peek_b(system, address + 2);
  l.d = h8_peek_b(system, address + 3);

  return l;
}

static void h8_poke_l(h8_system_t *system, const unsigned address,
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
  return rd_l(system, ers)->u + d;
}

/** General register, accessed as an address with 24-bit displacement */
static h8_aptr erd24(h8_system_t *system, unsigned ers, signed d)
{
  return rd_l(system, ers)->u + d;
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
  h8_write_##name(system, md, action(system, md_val, rs)); \
}
H8_RS_MD(b, h8_byte_t)
H8_RS_MD(w, h8_word_t)
H8_RS_MD(l, h8_long_t)

#define H8_MS_MD(name, type) \
static void ms_md_##name(h8_system_t *system, unsigned ms, unsigned md, \
                         type(*action)(h8_system_t*, type, const type)) \
{ \
  type ms_val = h8_read_##name(system, ms); \
  type md_val = h8_peek_##name(system, md); \
  h8_write_##name(system, md, action(system, md_val, ms_val)); \
}
H8_MS_MD(b, h8_byte_t)
H8_MS_MD(w, h8_word_t)
H8_MS_MD(l, h8_long_t)

void addx(h8_system_t *system, h8_byte_t *dst, const h8_byte_t src)
{
  h8_byte_t result;

  result.i = dst->i + src.i + system->cpu.ccr.flags.c;
  system->cpu.ccr.flags.c = result.u < src.u; \
  system->cpu.ccr.flags.v = ((dst->i > 0 && src.i > 0 && result.i < 0) ||
                            (dst->i < 0 && src.i < 0 && result.i > 0));
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
  system->cpu.ccr.flags.c = result.u < src.u; \
  system->cpu.ccr.flags.v = ((dst->i > 0 && src.i > 0 && result.i < 0) ||
                            (dst->i < 0 && src.i < 0 && result.i > 0));
  system->cpu.ccr.flags.n = result.i < 0;

  /* Retains its previous value when the result is
   * zero; otherwise cleared to 0 */
  if (result.u)
    system->cpu.ccr.flags.z = 0;

  system->cpu.ccr.flags.h = (result.u & (1 << 4)) != (dst->u & (1 << 4));

  *dst = result;
}

#define H8_AND_OP(name, type) \
type and##name(h8_system_t *system, type dst, const type src) \
{ \
  dst.u &= src.u; \
  ccr_zn(system, dst.i); \
  return dst; \
}
H8_AND_OP(b, h8_byte_t)
H8_AND_OP(w, h8_word_t)
H8_AND_OP(l, h8_long_t)

#define H8_OR_OP(name, type) \
type or##name(h8_system_t *system, type dst, const type src) \
{ \
  dst.u |= src.u; \
  ccr_zn(system, dst.i); \
  return dst; \
}
H8_OR_OP(b, h8_byte_t)
H8_OR_OP(w, h8_word_t)
H8_OR_OP(l, h8_long_t)

#define H8_XOR_OP(name, type) \
type xor##name(h8_system_t *system, type dst, const type src) \
{ \
  dst.u ^= src.u; \
  ccr_zn(system, dst.i); \
  return dst; \
}
H8_XOR_OP(b, h8_byte_t)
H8_XOR_OP(w, h8_word_t)
H8_XOR_OP(l, h8_long_t)

#define H8_NOT_OP(name, type) \
void not##name(h8_system_t *system, type *dst) \
{ \
  dst->u = ~dst->u; \
  ccr_zn(system, dst->i); \
}
H8_NOT_OP(b, h8_byte_t)
H8_NOT_OP(w, h8_word_t)
H8_NOT_OP(l, h8_long_t)

#define H8_NEG_OP(name, type) \
void neg##name(h8_system_t *system, type *src) \
{ \
  src->i = 0 - src->i; \
  /** @todo CCR */ \
}
H8_NEG_OP(b, h8_byte_t)
H8_NEG_OP(w, h8_word_t)
H8_NEG_OP(l, h8_long_t)

/** @todo Hacky */
#define H8_CMP_OP(name, type) \
type cmp##name(h8_system_t *system, type dst, const type src) \
{ \
  type tmp = dst; \
  sub##name(system, dst, src); \
  return tmp; \
}
H8_CMP_OP(b, h8_byte_t)
H8_CMP_OP(w, h8_word_t)
H8_CMP_OP(l, h8_long_t)

/**
 * Sets a specified bit in a general register or memory operand to 1. The bit
 * number is specified by 3-bit immediate data or the lower three bits of a
 * general register.
 */
h8_byte_t bset(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  H8_UNUSED(system);
  dst.u |= (1 << (src.u & B00000111));
  return dst;
}

/**
 * Clears a specified bit in a general register or memory operand to 0. The
 * bit number is specified by 3-bit immediate data or the lower three bits of
 * a general register.
 */
h8_byte_t bclr(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  H8_UNUSED(system);
  dst.u &= ~(1 << (src.u & B00000111));
  return dst;
}

/**
 * Inverts a specified bit in a general register or memory operand. The bit
 * number is specified by 3-bit immediate data or the lower three bits of a
 * general register.
 */
h8_byte_t bnot(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  H8_UNUSED(system);
  dst.u ^= (1 << (src.u & B00000111));
  return dst;
}

void btst(h8_system_t *system, h8_byte_t *dst, unsigned src)
{
  system->cpu.ccr.flags.z = dst->u & (1 << (src & B00000111)) ? 0 : 1;
}

h8_byte_t bst(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  dst.u |= (system->cpu.ccr.flags.c << (src.u & B00000111));
  return dst;
}

h8_byte_t bist(h8_system_t *system, h8_byte_t dst, const h8_byte_t src)
{
  dst.u &= ~(system->cpu.ccr.flags.c << (src.u & B00000111));
  return dst;
}

void bld(h8_system_t *system, h8_byte_t val, unsigned bit)
{
  system->cpu.ccr.flags.c = val.u & (1 << (bit & 7)) ? 1 : 0;
}

void bild(h8_system_t *system, h8_byte_t val, unsigned bit)
{
  system->cpu.ccr.flags.c = val.u & (1 << (bit & 7)) ? 0 : 1;
}

void bsr(h8_system_t *system, signed offset)
{
#if H8_SAFETY
  if (offset > INT16_MAX || offset < INT16_MIN)
  {
    H8_ERROR(H8_DEBUG_INVALID_PARAMETER;
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
 * Reads a 2-byte big-endian value from the address referenced by PC to the
 * data bus, then increments PC by 2.
 */
void h8_fetch(h8_system_t *system)
{
  system->dbus.bits = h8_read_w(system, system->cpu.pc);
  system->cpu.pc += 2;
  printf("%02X %02X ", system->dbus.a.u, system->dbus.b.u);
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
    /** @todo MOV.L stuff */
    h8_fetch(system);
    switch (system->dbus.a.u)
    {
    case 0x69:
      /** MOV.L ERs, @ERd */
      rs_md_l(system, *rd_l(system, system->dbus.bl), er(system, system->dbus.bh), movl);
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
        ms_rd_l(system, aa16(system->dbus.bits), rd_l(system, erd), movl);
        break;
      }
      case 0x2:
        H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
        break;
      case 0x8:
      {
        h8_u8 ers = system->dbus.bl;

        h8_fetch(system);
        if (ers)
          /** MOV.L ERs, @aa:16 */
          rs_md_l(system, *rd_l(system, ers), system->dbus.bits.u, movl);
        else
        {
          /** STC.W CCR, @aa:16 */
          h8_word_t w;
          w.u = system->cpu.ccr.raw.u;
          h8_write_w(system, system->dbus.bits.u, w);
        }
        break;
      }
      case 0xA:
        H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
        break;
      default:
        H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
      }
      break;
    }
    case 0x6D:
      /** MOV.L ERs, @-ERd */
      rs_md_l(system, *rd_l(system, system->dbus.bl), erpd_l(system, system->dbus.bh), movl);
      break;
    case 0x6F:
    {
      h8_byte_t sd = system->dbus.b;

      h8_fetch(system);
      if (sd.h & B10000000)
        /** MOV.L ERs, @(d:16, ERd) */
        rs_md_l(system, *rd_l(system, sd.l), erd16(system, sd.h, system->dbus.bits.i), movl);
      else
        /** MOV.L @(d:16, ERs), ERd */
        ms_rd_l(system, erd16(system, sd.h, system->dbus.bits.i), rd_l(system, sd.l), movl);
      break;
    }
    case 0x78:
      H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
    break;
  case 0x40:
    /** @todo STC.W / LDC.W stuff */
    h8_fetch(system);
    switch (system->dbus.a.u)
    {
    case 0x69:
      if (system->dbus.bl & B10000000)
        /** STC.W CCR, @ERd */
        h8_write_b(system, rd_w(system, system->dbus.bl)->u, system->cpu.ccr.raw);
      else
        /** LDC.W @ERs, CCR */
        system->cpu.ccr.raw = h8_read_b(system, rd_w(system, system->dbus.bl)->u);
      break;
    default:
      H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    }
    break;
  case 0x80:
    /** @todo SLEEP */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  case 0xC0:
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  case 0xD0:
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  case 0xF0:
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
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
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), addb);
}

H8_OP(op09)
{
  /** ADD.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), addw);
}

H8_OP(op0a)
{
  if (system->dbus.bh == 0x00)
  {
    /** INC.B Rd */
    h8_byte_t b;

    b.i = 1;
    rs_rd_b(system, b, rd_b(system, system->dbus.bl), addb);
  }
  else if (system->dbus.bh & 0x8)
    /** ADD.L ERs, ERd */
    rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), addl);
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op0b)
{
  h8_long_t l;
  h8_word_t w;

  switch (system->dbus.bh)
  {
  case 0x0:
    /** @todo Verify ADDS.L #1, ERd */
    l.u = 1;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), addl);
    break;
  case 0x5:
    /** INC.W #1, Rd */
    w.u = 1;
    rs_rd_w(system, w, rd_w(system, system->dbus.bl), addw);
    break;
  case 0x7:
    /** INC.L #1, ERd */
    l.u = 1;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), addl);
    break;
  case 0x8:
    /** @todo Verify ADDS.L #2, ERd */
    l.u = 2;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), addl);
    break;
  case 0x9:
    /** @todo Verify ADDS.L #4, ERd */
    l.u = 4;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), addl);
    break;
  case 0xD:
    /** INC.W #2, Rd */
    w.u = 2;
    rs_rd_w(system, w, rd_w(system, system->dbus.bl), addw);
    break;
  case 0xF:
    /** INC.L #2, ERd */
    l.u = 2;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), addl);
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op0c)
{
  /** MOV.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), movb);
}

H8_OP(op0d)
{
  /** MOV.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), movw);
}

H8_OP(op0e)
{
  /** ADDX.B Rs, Rd */
  addx(system, rd_b(system, system->dbus.bl), *rd_b(system, system->dbus.bh));
}

H8_OP(op14)
{
  /** OR.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), orb);
}

H8_OP(op15)
{
  /** XOR.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), xorb);
}

H8_OP(op16)
{
  /** AND.B Rs, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), andb);
}

H8_OP(op17)
{
  switch (system->dbus.bh)
  {
  case 0x0:
    /** NOT.B Rd */
    notb(system, rd_b(system, system->dbus.bl));
    break;
  case 0x1:
    /** NOT.W Rd */
    notw(system, rd_w(system, system->dbus.bl));
    break;
  case 0x3:
    /** NOT.L ERd */
    notl(system, rd_l(system, system->dbus.bl));
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
    negb(system, rd_b(system, system->dbus.bl));
    break;
  case 0x9:
    /** NEG.W Rd */
    negw(system, rd_w(system, system->dbus.bl));
    break;
  case 0xB:
    /** NEG.L ERd */
    negl(system, rd_l(system, system->dbus.bl));
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
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), subb);
}

H8_OP(op19)
{
  /** SUB.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), subw);
}

H8_OP(op1a)
{
  if (system->dbus.bh == 0x00)
  {
    /** DEC.B Rd */
    h8_byte_t b;

    b.i = 1;
    rs_rd_b(system, b, rd_b(system, system->dbus.bl), subb);
  }
  else if (system->dbus.bh & 0x8)
    /** SUB.L ERs, ERd */
    rs_rd_l(system, *rd_l(system, system->dbus.bh), rd_l(system, system->dbus.bl), subl);
  else
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
}

H8_OP(op1b)
{
  h8_long_t l;
  h8_word_t w;

  switch (system->dbus.bh)
  {
  case 0x0:
    /** SUBS.L #1, ERd */
    l.u = 1;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), subl);
    break;
  case 0x5:
    /** DEC.W #1, Rd */
    w.u = 1;
    rs_rd_w(system, w, rd_w(system, system->dbus.bl), subw);
    break;
  case 0x7:
    /** DEC.L #1, ERd */
    l.u = 1;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), subl);
    break;
  case 0x8:
    /** SUBS.L #2, ERd */
    l.u = 2;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), subl);
    break;
  case 0x9:
    /** SUBS.L #4, ERd */
    l.u = 4;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), subl);
    break;
  case 0xD:
    /** DEC.W #2, Rd */
    w.u = 2;
    rs_rd_w(system, w, rd_w(system, system->dbus.bl), subw);
    break;
  case 0xF:
    /** DEC.L #2, ERd */
    l.u = 2;
    rs_rd_l(system, l, rd_l(system, system->dbus.bl), subl);
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op1c)
{
  /** CMP.B Rs, Rd */
  cmpb(system, *rd_b(system, system->dbus.bh), *rd_b(system, system->dbus.bl));
}

H8_OP(op1d)
{
  /** CMP.W Rs, Rd */
  cmpw(system, *rd_w(system, system->dbus.bh), *rd_w(system, system->dbus.bl));
}

#define OP2X(al, reg) \
void op2##al(h8_system_t *system) \
{ \
  /** MOV.B @aa:8, Rd */ \
  ms_rd_b(system, aa8(system->dbus.b), &reg, movb); \
}
H8_UNROLL(OP2X)

#define OP3X(al, reg) \
void op3##al(h8_system_t *system) \
{ \
  /** MOV.B Rs, @aa:8 */ \
  rs_md_b(system, reg, aa8(system->dbus.b), movb); \
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

/** @todo Does SP assume ER7? Compiled command is 54 70 */
H8_OP(op54)
{
  /** RTS */
  system->cpu.pc = h8_read_w(system, system->cpu.regs[7].er.u).u;
  system->cpu.regs[7].er.u += 2;
}

H8_OP(op55)
{
  /** BSR d:8 */
  bsr(system, system->dbus.b.i);
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
  /** JMP @aa:8 */
  system->cpu.pc = aa8(system->dbus.b);
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
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), orw);
}

H8_OP(op65)
{
  /** XOR.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), xorw);
}

H8_OP(op66)
{
  /** AND.W Rs, Rd */
  rs_rd_w(system, *rd_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), andw);
}

H8_OP(op67)
{
  if (system->dbus.b.u & 0x80)
    rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bist);
  else
    rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bst);
}

H8_OP(op68)
{
  /** MOV.B @ERs, Rd */
  ms_rd_b(system, er(system, system->dbus.bh), rd_b(system, system->dbus.bl), movb);
}

H8_OP(op69)
{
  /** MOV.W @ERs, Rd */
  ms_rd_w(system, er(system, system->dbus.bh), rd_w(system, system->dbus.bl), movw);
}

H8_OP(op6a)
{
  h8_word_t func = system->dbus.bits;

  h8_fetch(system);
  switch (func.l.h)
  {
  case 0x0:
    /** MOV.B @aa:16, Rd */
    ms_rd_b(system, system->dbus.bits.u, rd_b(system, func.l.l), movb);
    break;
  case 0x2:
    /** MOV.B @aa:24, Rd */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  case 0x4:
    /** MOVFPE @aa:16, Rd */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  case 0x8:
    /** MOV.B Rs, @aa:16 */
    rs_md_b(system, *rd_b(system, func.l.l), system->dbus.bits.u, movb);
    break;
  case 0xA:
    /** MOV.B Rs, @aa:24 */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
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
    ms_rd_w(system, system->dbus.bits.u, rd_w(system, func.l.l), movw);
    break;
  case 0x2:
    /** MOV.W @aa:24, Rd */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  case 0x8:
    /** MOV.W Rs, @aa:16 */
    rs_md_w(system, *rd_w(system, func.l.l), system->dbus.bits.u, movw);
    break;
  case 0xA:
    /** MOV.W Rs, @aa:24 */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
    break;
  default:
    H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
  }
}

H8_OP(op6c)
{
  if (system->dbus.bh & 0x8)
    /** MOV.B Rs, @-ERd */
    rs_md_b(system, *rd_b(system, system->dbus.bl), erpd_b(system, system->dbus.bh), movb);
  else
    /** MOV.B @ERs+, Rd */
    ms_rd_b(system, erpi_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), movb);
}

H8_OP(op6d)
{
  if (system->dbus.bh & 0x8)
    /** MOV.W Rs, @-ERd */
    rs_md_w(system, *rd_w(system, system->dbus.bl), erpd_w(system, system->dbus.bh), movw);
  else
    /** MOV.W @ERs+, Rd */
    ms_rd_w(system, erpi_w(system, system->dbus.bh), rd_w(system, system->dbus.bl), movw);
}

H8_OP(op6e)
{
  h8_instruction_t func = system->dbus;

  h8_fetch(system);
  if (func.bh & 0x8)
    /** MOV.B Rs, @(d:16, ERd) */
    rs_md_b(system, *rd_b(system, func.bl), erd16(system, func.bh, system->dbus.bits.u), movb);
  else
    /** MOV.B @(d:16, ERs), Rd */
    ms_rd_b(system, erd16(system, func.bh, system->dbus.bits.u), rd_b(system, func.bl), movb);
}

H8_OP(op6f)
{
  /** MOV.W @(d:16, ERs), Rd */
  h8_instruction_t func = system->dbus;

  h8_fetch(system);
  ms_rd_w(system, erd16(system, func.bh, system->dbus.bits.i), rd_w(system, func.bl), movw);
}

H8_OP(op70)
{
  /** BSET #xx:3, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bset);
}

H8_OP(op71)
{
  /** BNOT #xx:3, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bnot);
}

H8_OP(op72)
{
  /** BCLR #xx:3, Rd */
  rs_rd_b(system, *rd_b(system, system->dbus.bh), rd_b(system, system->dbus.bl), bclr);
}

H8_OP(op73)
{
  /** BTST #xx:3, Rd */
  btst(system, rd_b(system, system->dbus.bl), system->dbus.bh);
}

H8_OP(op77)
{
  if (system->dbus.bh & B10000000)
    /** BILD #xx:3, Rd */
    bild(system, *rd_b(system, system->dbus.bl), system->dbus.bh);
  else
    /** BLD #xx:3, Rd */
    bld(system, *rd_b(system, system->dbus.bl), system->dbus.bh);
}

H8_OP(op79)
{
  h8_instruction_t curr = system->dbus;

  h8_fetch(system);
  switch (curr.bh)
  {
  case 0:
    /** MOV.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), movw);
    break;
  case 1:
    /** ADD.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), addw);
    break;
  case 2:
    /** CMP.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), cmpw);
    break;
  case 3:
    /** SUB.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), subw);
    break;
  case 4:
    /** OR.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), orw);
    break;
  case 5:
    /** XOR.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), xorw);
    break;
  case 6:
    /** AND.W #xx:16, Rd */
    rs_rd_w(system, system->dbus.bits, rd_w(system, curr.bl), andw);
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
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), movl);
    break;
  case 1:
    /** ADD.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), addl);
    break;
  case 2:
    /** CMP.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), cmpl);
    break;
  case 3:
    /** SUB.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), subl);
    break;
  case 4:
    /** OR.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), orl);
    break;
  case 5:
    /** XOR.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), xorl);
    break;
  case 6:
    /** AND.L #xx:32, ERd */
    rs_rd_l(system, imm, rd_l(system, system->dbus.bl), andl);
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
      rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.u), bset);
      break;
    case 0x1:
      /** BNOT Rs, @ERd */
      rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.u), bnot);
      break;
    case 0x2:
      /** BCLR Rs, @ERd */
      rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.u), bclr);
      break;
    case 0x7:
      if (system->dbus.bh & 0x8)
        /** BIST #xx:3, @ERd */
        rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.u), bist);
      else
        /** BST #xx:3, @ERd */
        rs_md_b(system, *rd_b(system, system->dbus.bh), er(system, func.l.u), bst);
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else if (system->dbus.ah == 0x7)
  {
    h8_byte_t bits;

    bits.u = system->dbus.bh;
    switch (system->dbus.al)
    {
    case 0x0:
      /** BSET #xx:3, @ERd */
      rs_md_b(system, bits, er(system, func.l.u), bset);
      break;
    case 0x1:
      /** BNOT #xx:3, @ERd */
      rs_md_b(system, bits, er(system, func.l.u), bnot);
      break;
    case 0x2:
      /** BCLR #xx:3, @ERd */
      rs_md_b(system, bits, er(system, func.l.u), bclr);
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
  h8_word_t func = system->dbus.bits;

  h8_fetch(system);
  if (system->dbus.ah == 0x6 && system->dbus.al == 0x3)
    /** BTST? */
    H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
  else if (system->dbus.ah == 0x7)
  {
    switch (system->dbus.al)
    {
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
      H8_ERROR(H8_DEBUG_UNIMPLEMENTED_OPCODE)
      break;
    case 0x7:
      if (system->dbus.bh & 0x8)
        /** BILD #xx:3, @aa:8 */
        bild(system, h8_read_b(system, 0xFF00 | func.l.u), system->dbus.bh);
      else
        /** BLD #xx:3, @aa:8 */
        bld(system, h8_read_b(system, 0xFF00 | func.l.u), system->dbus.bh);
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
      if (system->dbus.bh & 0x8)
        /** BIST #xx:3, @aa:8 */
        rs_md_b(system, *rd_b(system, system->dbus.bh), aa8(func.l), bist);
      else
        /** BST #xx:3, @aa:8 */
        rs_md_b(system, *rd_b(system, system->dbus.bh), aa8(func.l), bst);
      break;
    default:
      H8_ERROR(H8_DEBUG_MALFORMED_OPCODE)
    }
  }
  else if (system->dbus.ah == 0x7)
  {
    h8_byte_t bits;

    bits.u = system->dbus.bh;
    switch (system->dbus.al)
    {
    case 0x0:
      /** BSET #xx:3, @aa:8 */
      rs_md_b(system, bits, aa8(func.l), bset);
      break;
    case 0x1:
      /** BNOT #xx:3, @aa:8 */
      rs_md_b(system, bits, aa8(func.l), bnot);
      break;
    case 0x2:
      /** BCLR #xx:3, @aa:8 */
      rs_md_b(system, bits, aa8(func.l), bclr);
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
  rs_rd_b(system, system->dbus.b, &reg, addb); \
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
  cmpb(system, system->dbus.b, reg); \
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
  rs_rd_b(system, system->dbus.b, &reg, orb); \
}
H8_UNROLL(OPCX)

#define OPDX(al, reg) \
void opd##al(h8_system_t *system) \
{ \
  /** XOR.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, xorb); \
}
H8_UNROLL(OPDX)

#define OPEX(al, reg) \
void ope##al(h8_system_t *system) \
{ \
  /** AND.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, andb); \
}
H8_UNROLL(OPEX)

#define OPFX(al, reg) \
void opf##al(h8_system_t *system) \
{ \
  /** MOV.B #xx:8, Rd */ \
  rs_rd_b(system, system->dbus.b, &reg, movb); \
}
H8_UNROLL(OPFX)

void h8_init(h8_system_t *system)
{
  /* Setup default non-zero values */
  system->cpu.ccr.flags.i = 1;

  system->vmem.parts.io1.ssu.sssr.flags.tdre = 1;

  system->vmem.parts.io2.wdt.tmwd.flags.reserved = B1111;

  system->vmem.parts.io2.wdt.tcsrwd1.flags.b0wi = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.wdon = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.b2wi = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.b4wi = 1;
  system->vmem.parts.io2.wdt.tcsrwd1.flags.b6wi = 1;

  /* Jump to program entrypoint */
  system->cpu.pc = h8_read_w(system, 0).u;
}

static H8_OP_T funcs[256] =
{
  op00, op01, op02, op03, op04, op05, op06, op07,
  op08, op09, op0a, op0b, op0c, op0d, op0e, NULL,
  NULL, NULL, NULL, NULL, op14, op15, op16, op17,
  op18, op19, op1a, op1b, op1c, op1d, NULL, NULL,
  op20, op21, op22, op23, op24, op25, op26, op27,
  op28, op29, op2a, op2b, op2c, op2d, op2e, op2f,
  op30, op31, op32, op33, op34, op35, op36, op37,
  op38, op39, op3a, op3b, op3c, op3d, op3e, op3f,
  op40, op41, op42, op43, op44, op45, op46, op47,
  op48, op49, op4a, op4b, op4c, op4d, op4e, op4f,
  NULL, NULL, NULL, NULL, op54, op55, NULL, NULL,
  op58, op59, op5a, op5b, op5c, op5d, op5e, NULL,
  op60, op61, op62, op63, op64, op65, op66, op67,
  op68, op69, op6a, op6b, op6c, op6d, op6e, op6f,
  op70, op71, op72, op73, NULL, NULL, NULL, op77,
  NULL, op79, op7a, NULL, NULL, op7d, op7e, op7f,
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
  H8_OP_T function;

  if (system->cpu.pc > 0xFFFF || system->cpu.pc & 1)
    H8_ERROR(H8_DEBUG_BAD_PC)

  /*if (system->cpu.pc == 0x336 || system->cpu.pc == 0x350)
    system->cpu.pc += 4;*/

  h8_fetch(system);

  function = funcs[system->dbus.a.u];

  if (function)
    function(system);
  else
  {
    printf("UNDEFINED OPCODE!!! ");
    printf("%02X%02X at %04X\n", system->dbus.a.u, system->dbus.b.u, system->cpu.pc - 2);
    /* pootis breakpoint here */
    return;
  }

  if (system->error_code)
    printf("SYSTEM ERROR!!! %u at %u", system->error_code, system->error_line);

  system->instructions++;
}

void h8_run(h8_system_t *system);

#if H8_TESTS

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
  result = addb(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.h != 0 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 0 ||
      result.i != 15)
    H8_TEST_FAIL(1)

  /* 255 + 2 = 1 */
  dst_byte.u = 255;
  src_byte.u = 2;
  result = addb(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 1 ||
      system.cpu.ccr.flags.h != 1 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 0 ||
      result.i != 1)
    H8_TEST_FAIL(2)

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

void h8_test_size(void)
{
  h8_system_t system = {0};

  if (sizeof(system.vmem.parts.io1) != 0xE0)
    H8_TEST_FAIL(1)
  if (sizeof(system.vmem.parts.io2) != 0x80)
    H8_TEST_FAIL(2)
  if (sizeof(system.vmem) != 0x10000)
    H8_TEST_FAIL(3)

  printf("Size test passed!\n");
}

void h8_test_sub(void)
{
  h8_system_t system = {0};
  h8_byte_t dst_byte, src_byte, result;

  /* 7 - 3 = 4 */
  dst_byte.u = 7;
  src_byte.u = 3;
  result = subb(&system, dst_byte, src_byte);
  if (system.cpu.ccr.flags.c != 0 ||
      system.cpu.ccr.flags.h != 0 ||
      system.cpu.ccr.flags.z != 0 ||
      system.cpu.ccr.flags.v != 0 ||
      result.i != 4)
    H8_TEST_FAIL(1)

  printf("Subtraction test passed!\n");
}

#endif

void h8_test(void)
{
#if H8_TESTS
  h8_test_add();
  h8_test_bit_manip();
  h8_test_bit_order();
  h8_test_size();
  h8_test_sub();
#endif
}
