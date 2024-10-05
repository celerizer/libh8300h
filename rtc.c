#include <time.h>

#include "rtc.h"

void h8_rtc_set(h8_rtc_t *rtc, const time_t time)
{
  time_t ttime = (time_t)time;
  struct tm *local_time;

  local_time = localtime(&ttime);

  /* Update seconds */
  rtc->rsecdr.co = local_time->tm_sec % 10;
  rtc->rsecdr.ct = (h8_u8)local_time->tm_sec / 10;
  rtc->rsecdr.bsy = 0;

  /* Update minutes */
  rtc->rmindr.co = local_time->tm_min % 10;
  rtc->rmindr.ct = (h8_u8)local_time->tm_min / 10;
  rtc->rmindr.bsy = 0;

  /* Update hours */
  if (rtc->rtccr1.om)
  {
    /* Update in 24-hour mode */
    rtc->rhrdr.co = local_time->tm_hour % 10;
    rtc->rhrdr.ct = (h8_u8)local_time->tm_hour / 10;
  }
  else
  {
    /* Update in 12-hour mode */
    int trunctime = local_time->tm_hour;

    if (trunctime >= 12)
    {
      rtc->rtccr1.pm = 1;
      trunctime -= 12;
    }
    else
      rtc->rtccr1.pm = 0;

    rtc->rhrdr.co = trunctime % 10;
    rtc->rhrdr.ct = (h8_u8)trunctime / 10;
  }
  rtc->rhrdr.bsy = 0;

  /* Update day of the week */
  rtc->rwkdr.wk = (h8_u8)local_time->tm_wday;
  rtc->rwkdr.bsy = 0;
}

void h8_rtc_set_current(h8_rtc_t *rtc, const time_t offset)
{
  h8_rtc_set(rtc, time(NULL) + offset);
}
