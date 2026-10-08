#include <time.h>

#include "rtc.h"
#include "system.h"

void h8_rtc_set(h8_rtc_t *rtc, const time_t time)
{
  time_t ttime = (time_t)time;
  struct tm *local_time;

  local_time = localtime(&ttime);

  /* Update seconds */
  rtc->rsecdr.flags.co = local_time->tm_sec % 10;
  rtc->rsecdr.flags.ct = (h8_u8)local_time->tm_sec / 10;
  rtc->rsecdr.flags.bsy = 0;

  /* Update minutes */
  rtc->rmindr.flags.co = local_time->tm_min % 10;
  rtc->rmindr.flags.ct = (h8_u8)local_time->tm_min / 10;
  rtc->rmindr.flags.bsy = 0;

  /* Update hours */
  if (rtc->rtccr1.flags.om)
  {
    /* Update in 24-hour mode */
    rtc->rhrdr.flags.co = local_time->tm_hour % 10;
    rtc->rhrdr.flags.ct = (h8_u8)local_time->tm_hour / 10;
  }
  else
  {
    /* Update in 12-hour mode */
    int trunctime = local_time->tm_hour;

    if (trunctime >= 12)
    {
      rtc->rtccr1.flags.pm = 1;
      trunctime -= 12;
    }
    else
      rtc->rtccr1.flags.pm = 0;

    rtc->rhrdr.flags.co = trunctime % 10;
    rtc->rhrdr.flags.ct = (h8_u8)trunctime / 10;
  }
  rtc->rhrdr.flags.bsy = 0;

  /* Update day of the week */
  rtc->rwkdr.flags.wk = (h8_u8)local_time->tm_wday;
  rtc->rwkdr.flags.bsy = 0;
}

void h8_rtc_set_current(h8_rtc_t *rtc, const time_t offset)
{
  h8_rtc_set(rtc, time(NULL) + offset);
}

/** Adds 1 to a two-digit BCD value */
static h8_u8 h8_rtc_bcd_inc(h8_u8 value)
{
  value++;
  if ((value & 0x0F) > 9)
    value = (h8_u8)((value & 0xF0) + 0x10);

  return value;
}

/** Sets an RTCFLG flag if its interrupt is enabled in RTCCR2 */
static void h8_rtc_flag(h8_rtc_t *rtc, const h8_u8 flag)
{
  if (rtc->rtccr2.raw.u & flag)
    rtc->rtcflg.raw.u |= flag;
}

/** Advances the clock by one second, carrying into the larger units */
static void h8_rtc_second(h8_rtc_t *rtc)
{
  h8_u8 sec = h8_rtc_bcd_inc(rtc->rsecdr.raw.u & 0x7F);

  h8_rtc_flag(rtc, H8_RTC_SECOND);
  if (sec >= 0x60)
  {
    h8_u8 min = h8_rtc_bcd_inc(rtc->rmindr.raw.u & 0x7F);

    sec = 0;
    h8_rtc_flag(rtc, H8_RTC_MINUTE);
    if (min >= 0x60)
    {
      h8_u8 hour = h8_rtc_bcd_inc(rtc->rhrdr.raw.u & 0x3F);
      h8_bool next_day = FALSE;

      min = 0;
      h8_rtc_flag(rtc, H8_RTC_HOUR);
      if (rtc->rtccr1.flags.om)
      {
        /* 24-hour mode counts 0 to 23 */
        if (hour >= 0x24)
        {
          hour = 0;
          next_day = TRUE;
        }
      }
      else if (hour >= 0x12)
      {
        /* 12-hour mode counts 0 to 11, and P.M. ends at midnight */
        hour = 0;
        if (rtc->rtccr1.flags.pm)
        {
          rtc->rtccr1.flags.pm = 0;
          next_day = TRUE;
        }
        else
          rtc->rtccr1.flags.pm = 1;
      }

      if (next_day)
      {
        h8_rtc_flag(rtc, H8_RTC_DAY);
        if (rtc->rwkdr.flags.wk >= H8_RTC_SATURDAY)
        {
          rtc->rwkdr.flags.wk = H8_RTC_SUNDAY;
          h8_rtc_flag(rtc, H8_RTC_WEEK);
        }
        else
          rtc->rwkdr.flags.wk++;
      }
      rtc->rhrdr.raw.u = hour;
    }
    rtc->rmindr.raw.u = min;
  }
  rtc->rsecdr.raw.u = sec;
}

void h8_rtc_run(h8_system_t *system, unsigned states, h8_bool phi_running)
{
  h8_rtc_t *rtc = &system->vmem.parts.io1.rtc;
  h8_rtc_state_t *state = &system->rtc;

  if (!(system->vmem.raw[H8_REG_CKSTPR1].u & H8_CKSTPR1_RTCCKSTP) ||
      rtc->rtccr1.flags.rst || !rtc->rtccr1.flags.run)
    return;

  if ((rtc->rtccsr.raw.u & 0x0F) == 0x08)
  {
    /**
     * 11.4: The clock counts on subclock/4, 8192 times a second, so a quarter
     * second is 2048 counts.
     */
    h8_u32 quarter = system->clock / (H8_CLOCK_SUB / 8192);

    state->quarter_states += states;
    while (state->quarter_states >= quarter)
    {
      state->quarter_states -= quarter;
      h8_rtc_flag(rtc, H8_RTC_QUARTER_SECOND);
      state->quarters = (state->quarters + 1) & 3;
      if (!(state->quarters & 1))
        h8_rtc_flag(rtc, H8_RTC_HALF_SECOND);
      if (!state->quarters)
        h8_rtc_second(rtc);
    }
  }
  else if (phi_running && (rtc->rtccsr.raw.u & 0x0F) < 0x08)
  {
    /* Free running counter on a divided system clock */
    static const h8_u16 dividers[8] =
      { 8, 32, 128, 256, 512, 2048, 4096, 8192 };
    h8_u16 divider = dividers[rtc->rtccsr.raw.u & 0x07];

    state->prescaler += states;
    while (state->prescaler >= divider)
    {
      state->prescaler -= divider;
      rtc->rsecdr.raw.u++;
      if (!rtc->rsecdr.raw.u)
        h8_rtc_flag(rtc, H8_RTC_FREE_RUNNING);
    }
  }
}

void h8_rtc_rtccr1_out(h8_system_t *system, h8_byte_t *byte,
                       const h8_byte_t value)
{
  h8_rtc_t *rtc = &system->vmem.parts.io1.rtc;

  *byte = value;
  if (rtc->rtccr1.flags.rst)
  {
    /* 11.3.5: RST resets registers and control circuits except RTCCSR */
    rtc->rtcflg.raw.u = 0;
    rtc->rsecdr.raw.u = 0;
    rtc->rmindr.raw.u = 0;
    rtc->rhrdr.raw.u = 0;
    rtc->rwkdr.raw.u = 0;
    rtc->rtccr2.raw.u = 0;
    rtc->rtccr1.raw.u = value.u & 0x10;
    system->rtc.quarter_states = 0;
    system->rtc.quarters = 0;
    system->rtc.prescaler = 0;
  }
}

void h8_rtc_init(h8_system_t *system)
{
  h8_rtc_t *rtc = &system->vmem.parts.io1.rtc;

  rtc->rtcflg.raw.u = 0;
  rtc->rtccr1.raw.u = 0;
  rtc->rtccr2.raw.u = 0;
  rtc->rtccsr.raw.u = H8_RTCCSR_INITIAL;
  system->rtc.quarter_states = 0;
  system->rtc.quarters = 0;
  system->rtc.prescaler = 0;
}
