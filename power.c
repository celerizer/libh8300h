#include "power.h"
#include "system.h"

/**
 * Table 5.1: The wait for the system clock to stabilize when leaving standby
 * or watch mode for active mode, in oscillator clock states, selected by STS2 to STS0.
 */
static const h8_u16 h8_power_wait[8] =
  { 8192, 16384, 1024, 2048, 4096, 256, 512, 16 };

/** The active mode selected by MSON */
static h8_power_mode h8_power_active(const h8_system_t *system)
{
  return (system->vmem.raw[H8_REG_SYSCR2].u & H8_SYSCR2_MSON) ?
         H8_POWER_ACTIVE_MEDIUM : H8_POWER_ACTIVE;
}

/** Charges the stabilization wait set by STS2 to STS0 */
static void h8_power_add_wait(h8_system_t *system)
{
  h8_u8 sts = (system->vmem.raw[H8_REG_SYSCR1].u & H8_SYSCR1_STS) >> 4;

  system->power.wait_states += h8_power_wait[sts];
}

h8_bool h8_power_halted(const h8_system_t *system)
{
  return system->power.mode >= H8_POWER_SLEEP;
}

h8_bool h8_power_phi_running(const h8_system_t *system)
{
  switch (system->power.mode)
  {
  case H8_POWER_ACTIVE:
  case H8_POWER_ACTIVE_MEDIUM:
  case H8_POWER_SLEEP:
  case H8_POWER_SLEEP_MEDIUM:
    return TRUE;
  default:
    return FALSE;
  }
}

h8_bool h8_power_sub_running(const h8_system_t *system)
{
  return system->power.mode != H8_POWER_STANDBY;
}

void h8_power_sleep(h8_system_t *system)
{
  h8_power_t *power = &system->power;
  h8_u8 syscr1 = system->vmem.raw[H8_REG_SYSCR1].u;
  h8_bool ssby = (syscr1 & H8_SYSCR1_SSBY) != 0;
  h8_bool lson = (syscr1 & H8_SYSCR1_LSON) != 0;
  h8_bool tma3 = (syscr1 & H8_SYSCR1_TMA3) != 0;
  h8_bool dton = (system->vmem.raw[H8_REG_SYSCR2].u & H8_SYSCR2_DTON) != 0;
  h8_power_mode active = h8_power_active(system);

  if (power->mode == H8_POWER_SUBACTIVE)
  {
    if (!ssby)
    {
      /* Subsleep mode, returning to subactive mode */
      if (lson && tma3 && !dton)
      {
        power->mode = H8_POWER_SUBSLEEP;
        power->wake_mode = H8_POWER_SUBACTIVE;
      }
    }
    else if (tma3)
    {
      if (!dton)
      {
        power->mode = H8_POWER_WATCH;
        power->wake_mode = lson ? H8_POWER_SUBACTIVE : active;
      }
      else if (!lson)
      {
        /* 5.3.5: Direct transition to active mode via watch mode */
        power->mode = active;
        h8_power_add_wait(system);
      }
    }
  }
  else if (!dton)
  {
    if (!ssby)
    {
      power->mode = active == H8_POWER_ACTIVE_MEDIUM ?
                    H8_POWER_SLEEP_MEDIUM : H8_POWER_SLEEP;
      power->wake_mode = active;
    }
    else if (!tma3)
    {
      power->mode = H8_POWER_STANDBY;
      power->wake_mode = active;
    }
    else
    {
      power->mode = H8_POWER_WATCH;
      power->wake_mode = lson ? H8_POWER_SUBACTIVE : active;
    }
  }
  else if (!ssby)
    /* 5.3.1 / 5.3.3: Direct transition between high and medium speed */
    power->mode = active;
  else if (lson && tma3)
    /* 5.3.2 / 5.3.4: Direct transition to subactive mode */
    power->mode = H8_POWER_SUBACTIVE;
}

void h8_power_wake(h8_system_t *system)
{
  h8_power_t *power = &system->power;
  h8_power_mode from = power->mode;

  power->mode = power->wake_mode;

  /* The system clock restarts after standby and watch mode */
  if ((from == H8_POWER_WATCH || from == H8_POWER_STANDBY) &&
      (power->mode == H8_POWER_ACTIVE ||
       power->mode == H8_POWER_ACTIVE_MEDIUM))
    h8_power_add_wait(system);
}

h8_u32 h8_power_to_phi(h8_system_t *system, h8_power_mode mode,
                       unsigned states)
{
  switch (mode)
  {
  case H8_POWER_ACTIVE_MEDIUM:
  case H8_POWER_SLEEP_MEDIUM:
    /* MA1 and MA0 select oscillator clock / 8, 16, 32, or 64 */
    return (h8_u32)states <<
           (3 + (system->vmem.raw[H8_REG_SYSCR1].u & H8_SYSCR1_MA));
  case H8_POWER_SUBACTIVE:
  case H8_POWER_SUBSLEEP:
  {
    /* SA1 and SA0 select subclock / 8, 4, 2, or 1; convert in whole and fraction */
    h8_u32 cycles = (h8_u32)states <<
                    (3 - (system->vmem.raw[H8_REG_SYSCR2].u & H8_SYSCR2_SA));
    h8_u32 whole = cycles * (system->clock / H8_CLOCK_SUB);
    h8_u32 part = cycles * (system->clock % H8_CLOCK_SUB) +
                  system->power.remainder;

    system->power.remainder = part % H8_CLOCK_SUB;
    return whole + part / H8_CLOCK_SUB;
  }
  default:
    return states;
  }
}

h8_u32 h8_power_idle_states(const h8_system_t *system)
{
  /* About two watch clock cycles; interrupts are checked this often */
  h8_u32 states = system->clock / 16384;

  return states ? states : 1;
}

void h8_power_init(h8_system_t *system)
{
  system->power.mode = H8_POWER_ACTIVE;
  system->power.wake_mode = H8_POWER_ACTIVE;
  system->power.remainder = 0;
  system->power.wait_states = 0;
}
