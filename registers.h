#ifndef H8_REGISTERS_H
#define H8_REGISTERS_H

#include "types.h"

/**
 * FLMCR1 is a register that makes the flash memory enter the programming mode,
 * programming-verifying mode, erasing mode, or erasing-verifying mode.
 * Mapped to 0xF020.
 */
typedef union
{
  H8_BITFIELD_8
  (
    /** Program */
    h8_u8 p : 1,

    /** Erase */
    h8_u8 e : 1,

    /** Program-Verify */
    h8_u8 pv : 1,

    /** Erase-Verify */
    h8_u8 ev : 1,

    /** Program Setup */
    h8_u8 psu : 1,

    /** Erase Setup */
    h8_u8 esu : 1,

    /** Software Write Enable */
    h8_u8 swe : 1,

    /** Reserved */
    h8_u8 r : 1
  ) flags;
  h8_byte_t raw;
} h8_flmcr1_t;

typedef union
{
  H8_BITFIELD_2
  (
    h8_u8 cks : 4,
    h8_u8 reserved : 4
  ) flags;
  h8_byte_t raw;
} h8_tmwd_t;

typedef union
{
  H8_BITFIELD_8
  (
    h8_u8 wrst : 1,
    h8_u8 b0wi : 1,
    h8_u8 wdon : 1,
    h8_u8 b2wi : 1,
    h8_u8 tcsrwe : 1,
    h8_u8 b4wi : 1,
    h8_u8 tcwe : 1,
    h8_u8 b6wi : 1
  ) flags;
  h8_byte_t raw;
} h8_tcsrwd1_t;

typedef struct
{
  h8_tmwd_t tmwd;
  h8_tcsrwd1_t tcsrwd1;
  h8_byte_t tcsrwd2;
  h8_byte_t tcwd;
} h8_wdt_t;

/** 15.3.1 SS Control Register H (SSCRH) */
typedef union h8_sscrh_t
{
  H8_BITFIELD_7
  (
    /** SCS Pin Select */
    h8_u8 css : 2,

    /** SSCK Pin Select */
    h8_u8 scks : 1,

    /** SOL Write Protect */
    h8_u8 solp : 1,

    /** Serial Data Output Level Setting */
    h8_u8 sol : 1,

    /** Serial Data Open-Drain Output Select */
    h8_u8 soos : 1,

    /** Bidirectional Mode Enable */
    h8_u8 bide : 1,

    /** Master/Slave Device Select */
    h8_u8 mss : 1
  ) flags;
  h8_byte_t raw;
} h8_sscrh_t;

/** 15.3.3 SS Mode Register (SSMR) */
typedef union h8_ssmr_t
{
  H8_BITFIELD_7
  (
    /** Transfer clock rate select 0 */
    h8_u8 cks0 : 1,

    /** Transfer clock rate select 1 */
    h8_u8 cks1 : 1,

    /** Transfer clock rate select 2 */
    h8_u8 cks2 : 1,

    h8_u8 reserved : 2,

    /** Clock Phase Select */
    h8_u8 cphs : 1,

    /** Clock Polarity Select */
    h8_u8 cpos : 1,

    /** MSB-First/LSB-First Select */
    h8_u8 mls : 1
  ) flags;
  h8_byte_t raw;
} h8_ssmr_t;

typedef union
{
  H8_BITFIELD_8
  (
    h8_u8 ceie : 1,
    h8_u8 rie : 1,
    h8_u8 tie : 1,
    h8_u8 teie : 1,
    h8_u8 reserved : 1,
    h8_u8 rsstp : 1,
    h8_u8 re : 1,
    h8_u8 te : 1
  ) flags;
  h8_byte_t raw;
} h8_sser_t;

typedef union h8_sssr_t
{
  H8_BITFIELD_8
  (
    /** Conflict Error Flag */
    h8_u8 ce : 1,

    /** Receive Data Register Full */
    h8_u8 rdrf : 1,

    /** Transmit Data Empty */
    h8_u8 tdre : 1,

    /** Transmit End */
    h8_u8 tend : 1,

    h8_u8 reserved1 : 1,
    h8_u8 reserved2 : 1,

    /** Overrun Error Flag */
    h8_u8 orer : 1,

    h8_u8 reserved3 : 1
  ) flags;
  h8_byte_t raw;
} h8_sssr_t;

/** @todo Where is SSTRSR? */
typedef struct
{
  h8_sscrh_t sscrh;
  h8_byte_t sscrl;
  h8_ssmr_t ssmr;
  h8_sser_t sser;
  h8_sssr_t sssr;
  h8_byte_t unused[4];
  h8_byte_t ssrdr;
  h8_byte_t unused2;
  h8_byte_t sstdr;
  h8_byte_t unused3[4];
} h8_ssu_t;

/** @todo Timer W */
typedef struct
{
  h8_byte_t tmrw;
  h8_byte_t tcrw;
  h8_byte_t tierw;
  h8_byte_t tsrw;
  h8_byte_t tior0;
  h8_byte_t tior1;
  h8_word_be_t tcnt;
  h8_word_be_t gra;
  h8_word_be_t grb;
  h8_word_be_t grc;
  h8_word_be_t grd;
} h8_tw_t;

/**
 * 17.3.1 A/D Result Register (ADRR)
 * Mapped to FFBC / FFBD
 */
typedef union h8_adrr_t
{
  h8_word_be_t raw;
} h8_adrr_t;

/**
 * 7.3.2 A/D Mode Register (AMR)
 * Mapped to FFBE
 */
typedef union
{
  H8_BITFIELD_4
  (
    /** Channel Select */
    h8_u8 ch : 4,

    /** Clock Select (unimplemented) */
    h8_u8 cks : 2,

    /** External Trigger Select */
    h8_u8 trge : 1,

    h8_u8 reserved : 1
  ) flags;
  h8_byte_t raw;
} h8_amr_t;

/**
 * 7.3.3 A/D Start Register (ADSR)
 * Mapped to FFBF
 */
typedef union
{
  H8_BITFIELD_3
  (
    h8_u8 reserved : 6,

    /** Ladder Resistance Select */
    h8_u8 lads : 1,

    /**
     * A/D Start/Finish? Set to 1 to start, then it sets itself to 0 when
     * finished
     */
    h8_u8 adsf : 1
  ) flags;
  h8_byte_t raw;
} h8_adsr_t;

typedef struct
{
  h8_adrr_t adrr;
  h8_amr_t amr;
  h8_adsr_t adsr;
} h8_adc_t;

#endif
