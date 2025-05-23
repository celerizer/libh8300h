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
  H8_BITFIELD_3
  (
    /**
     * Reserved
     * These bits are always read as 0 and cannot be modified.
     */
    h8_u8 reserved : 4,

    /**
     * IrDA Clock Select (unimplemented)
     */
    h8_u8 clock_select : 3,

    /**
     * IrDA Enable
     * Selects whether the SCI3 I/O pins function as the SCI3 or IrDA.
     */
    h8_u8 enable : 1
  ) flags;
  h8_byte_t raw;
} h8_ircr_t;

typedef union
{
  H8_BITFIELD_6
  (
    /**
     * RXD3/IrRXD Pin Input Data Inversion Switch (unimplemented)
     * Selects whether input data of the RXD3/IrRXD pin is inverted or not.
     */
    h8_u8 scinv0 : 1,

    h8_u8 scinv1 : 1,

    h8_u8 reserved1 : 2,

    /**
     * P32/TXD3/IrTXD Pin Function Switch
     * Selects whether pin P32/TXD3/IrTXD is used as P32 or as TXD3/IrTXD.
     */
    h8_u8 spc3 : 1,

    h8_u8 reserved2 : 1,

    h8_u8 reserved3 : 2
  ) flags;
  h8_byte_t raw;
} h8_scr3_t;

typedef struct
{
  h8_byte_t smr3;
  h8_byte_t brr3;
  h8_scr3_t scr3;
  h8_byte_t tdr3;
  h8_byte_t ssr3;
  h8_byte_t rdr3;
  h8_byte_t semr;
  h8_byte_t unknown1[8];
  h8_ircr_t ircr;
  h8_byte_t unknown2[8];
} h8_sci3_t;

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
#define H8_REG_SSCRH 0xF0E0

/** 15.3.2 SS Control Register L (SSCRL) */
typedef union h8_sscrl_t
{
  h8_byte_t raw;
} h8_sscrl_t;
#define H8_REG_SSCRL 0xF0E1

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
  h8_sscrl_t sscrl;
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

typedef enum
{
  H8_ADC_AN0 = B0100,
  H8_ADC_AN1,
  H8_ADC_AN2,
  H8_ADC_AN3,
  H8_ADC_AN4,
  H8_ADC_AN5,

  H8_ADC_MAX
} h8_adc_channel;

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
