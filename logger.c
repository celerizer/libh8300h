#include "logger.h"

#include "config.h"

#include <stdio.h>
#include <stdarg.h>

static h8_log_level log_level = H8_LOGGER_DEFAULT_LEVEL;

void h8_log(h8_log_level level, h8_log_source source, const char *fmt, ...)
{
  if (level < log_level)
    return;
  else
  {
    const char *source_str;
    va_list args;
    va_start(args, fmt);

    switch (source)
    {
    case H8_LOG_CPU:
      source_str = "CPU";
      break;
    case H8_LOG_LCD:
      source_str = "LCD";
      break;
    case H8_LOG_EEP:
      source_str = "EEP";
      break;
    case H8_LOG_SSU:
      source_str = "SSU";
      break;
    case H8_LOG_IR:
      source_str = "IR ";
      break;
    default:
      source_str = "???";
      break;
    }

    printf("[%s] ", source_str);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
  }
}
