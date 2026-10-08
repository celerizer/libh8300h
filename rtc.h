#ifndef H8_RTC_H
#define H8_RTC_H

#include "types.h"

#include <time.h>

/**
 * Section 11: Realtime Clock (RTC)
 *
 * Counts seconds, minutes, hours, and the day of the week in BCD from
 * subclock/4, and raises periodic interrupts from 0.25 seconds up to a week.
 * Can instead run as an 8-bit free running counter on a divided system clock.
 * The registers are mapped to H'F067 - H'F06F.
 */

struct h8_system_t;

/**
 * RSECDR counts the BCD-coded second value. The setting range is decimal
 * 00 to 59. It is an 8-bit read register used as a counter, when it operates
 * as a free running counter.
 * Mapped to 0xF068.
 */
typedef union
{
  H8_BITFIELD_3
  (
    /** Counting One's Position. Counts from 0-9. */
    h8_u8 co : 4,

    /** Counting Ten's Position. Counts from 0-5. */
    h8_u8 ct : 3,

    /**
     * RTC Busy.
     * This bit is set to 1 when the RTC is updating (operating) the values of
     * second, minute, hour, and day-of-week data registers. When this bit is
     * 0, the values of second, minute, hour, and day-of-week data registers
     * must be adopted.
     */
    h8_u8 bsy : 1
  ) flags;
  h8_byte_t raw;
} h8_rsecdr_t;

/**
 * RMINDR counts the BCD-coded minute value on the carry generated once per
 * minute by the RSECDR counting. The setting range is decimal 00 to 59.
 * Mapped to 0xF069.
 */
typedef union
{
  H8_BITFIELD_3
  (
    /** Counting One's Position. Counts from 0-9. */
    h8_u8 co : 4,

    /** Counting Ten's Position. Counts from 0-5. */
    h8_u8 ct : 3,

    /** RTC Busy, as in RSECDR */
    h8_u8 bsy : 1
  ) flags;
  h8_byte_t raw;
} h8_rmindr_t;

/**
 * RHRDR counts the BCD-coded hour value on the carry generated once per hour
 * by RMINDR. The range is 00 to 11 or 00 to 23 depending on the 12/24 bit in
 * RTCCR1.
 * Mapped to 0xF06A.
 */
typedef union
{
  H8_BITFIELD_4
  (
    /** Counting One's Position. Counts from 0-9. */
    h8_u8 co : 4,

    /** Counting Ten's Position. Counts from 0-2. */
    h8_u8 ct : 2,

    /** Reserved */
    h8_u8 r : 1,

    /** RTC Busy, as in RSECDR */
    h8_u8 bsy : 1
  ) flags;
  h8_byte_t raw;
} h8_rhrdr_t;

enum
{
  H8_RTC_SUNDAY = 0,
  H8_RTC_MONDAY,
  H8_RTC_TUESDAY,
  H8_RTC_WEDNESDAY,
  H8_RTC_THURSDAY,
  H8_RTC_FRIDAY,
  H8_RTC_SATURDAY,
  H8_RTC_RESERVED
};

/**
 * RWKDR counts the day of the week on the carry generated once per day by
 * RHRDR, from 0 (Sunday) to 6 (Saturday).
 * Mapped to 0xF06B.
 */
typedef union
{
  H8_BITFIELD_3
  (
    /** Day of the week */
    h8_u8 wk : 3,

    /** Reserved */
    h8_u8 r : 4,

    /** RTC Busy, as in RSECDR */
    h8_u8 bsy : 1
  ) flags;
  h8_byte_t raw;
} h8_rwkdr_t;

enum
{
  H8_RTC_12H = 0,
  H8_RTC_24H = 1
};

/**
 * 11.3.5 RTCCR1 controls start/stop and reset of the clock timer.
 * Mapped to 0xF06C.
 */
typedef union
{
  H8_BITFIELD_6
  (
    /** Reserved */
    h8_u8 r : 3,

    /** Interrupt Occurrence Timing */
    h8_u8 intr : 1,

    /** Reset: resets registers and control circuits except RTCCSR */
    h8_u8 rst : 1,

    /** A.M. (0) or P.M. (1) in 12-hour mode */
    h8_u8 pm : 1,

    /** Operating Mode: 12-hour (0) or 24-hour (1) */
    h8_u8 om : 1,

    /** RTC Operation Start */
    h8_u8 run : 1
  ) flags;
  h8_byte_t raw;
} h8_rtccr1_t;

/**
 * 11.3.6 RTCCR2 enables the periodic interrupts. Bits 0 to 7 match the
 * RTCFLG flags.
 * Mapped to 0xF06D.
 */
typedef union
{
  h8_byte_t raw;
} h8_rtccr2_t;

/**
 * 11.3.7 RTCCSR selects the clock source. RCS3 to RCS0 = 1000 selects subclock/4
 * and RTC operation; other values make RSECDR a free running counter.
 * Mapped to 0xF06F.
 */
typedef union
{
  h8_byte_t raw;
} h8_rtccsr_t;
#define H8_RTCCSR_INITIAL 0x08

/**
 * 11.3.8 RTCFLG holds the interrupt flags, which are cleared by writing 0.
 * Mapped to 0xF067.
 */
typedef union
{
  h8_byte_t raw;
} h8_rtcflg_t;

/** RTCFLG and RTCCR2 bits */
#define H8_RTC_QUARTER_SECOND 0x01
#define H8_RTC_HALF_SECOND 0x02
#define H8_RTC_SECOND 0x04
#define H8_RTC_MINUTE 0x08
#define H8_RTC_HOUR 0x10
#define H8_RTC_DAY 0x20
#define H8_RTC_WEEK 0x40
#define H8_RTC_FREE_RUNNING 0x80

/** The RTC registers in address order, from H'F067 to H'F06F */
typedef struct
{
  h8_rtcflg_t rtcflg;
  h8_rsecdr_t rsecdr;
  h8_rmindr_t rmindr;
  h8_rhrdr_t rhrdr;
  h8_rwkdr_t rwkdr;
  h8_rtccr1_t rtccr1;
  h8_rtccr2_t rtccr2;
  h8_byte_t reserved;
  h8_rtccsr_t rtccsr;
} h8_rtc_t;

/** Internal RTC state that is not visible to software */
typedef struct
{
  /** Progress toward the next quarter second, in system clock states */
  h8_u32 quarter_states;

  /** Quarter seconds elapsed in the current second, from 0 to 3 */
  h8_u8 quarters;

  /** Progress toward the next free running count, in system clock states */
  h8_u32 prescaler;
} h8_rtc_state_t;

/**
 * Updates the system's RTC registers to match a given time_t.
 */
void h8_rtc_set(h8_rtc_t *rtc, const time_t time);

/**
 * Updates the system's RTC registers to match the current time, plus an
 * offset in seconds.
 */
void h8_rtc_set_current(h8_rtc_t *rtc, const time_t offset);

/**
 * Puts the RTC registers and internal state into their reset values.
 */
void h8_rtc_init(struct h8_system_t *system);

/**
 * Advances the RTC by an amount of time.
 * @param states The time that elapsed, in system clock states
 * @param phi_running Whether the system clock runs, for free running mode
 */
void h8_rtc_run(struct h8_system_t *system, unsigned states,
                h8_bool phi_running);

/** RTCCR1 write handler, which applies RST */
void h8_rtc_rtccr1_out(struct h8_system_t *system, h8_byte_t *byte,
                       const h8_byte_t value);

#endif
