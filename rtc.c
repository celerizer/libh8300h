#include <time.h>

#include "rtc.h"

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
