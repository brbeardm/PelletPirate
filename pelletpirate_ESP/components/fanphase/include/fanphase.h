#ifndef FANPHASE_H
#define FANPHASE_H

// Fan AC phase control for the V5 board (MOC3053M random-phase opto +
// H11AA1M zero-cross detector). The control loop closes in firmware by
// design: there is no copper between U3 and the MOC — the ZC interrupt
// on BOARD_ZC_DET_GPIO times the trigger on BOARD_FAN_GPIO.
//
// V5 only (CONFIG_PELLETPIRATE_BOARD_V5). On V2/V4 builds the functions
// compile to no-ops: V2's fan opto is a zero-cross MOC3063 (phase control
// impossible) and V4's ZC net was never validated.
//
// ZC signal, measured on V5 board 1, AC, 2026-11-07 (rung 2):
//   - qualified rising edges at 120 Hz, period 8331 us avg, +/-9 us spread
//   - HIGH pulse ~312 us wide, centered on the zero crossing -> the rising
//     edge leads true zero by ~156 us
//   - raw line CHATTERS through the GPIO threshold (slow H11AA1 ramp):
//     edges must be qualified with a dead-time lockout, never used raw
//
// Phase engine: free-running 1 MHz GPTimer. The ZC ISR drops the fan
// GPIO (LED off ~156 us before the voltage zero; the latched triac
// conducts to its current zero and commutates off), zeroes the counter
// and sets the alarm to the fire delay. The alarm ISR raises the GPIO,
// which then HOLDS until the next ZC — holding (vs a short pulse)
// guarantees the triac latches even with the inductive fan load; the
// MOC LED at ~11 mA continuous is well within rating.
//
// Failsafe: no qualified ZC edge for 100 ms while the fan is commanded
// on -> the pin is driven solid HIGH (full speed = full combustion air,
// the safe direction) until edges return. Bench power behaves this way
// automatically (H11AA1 only runs on mains).

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring up the phase engine: ZC ISR + GPTimer + fan pin (taken over from
 * the boot-time LOW hold), plus the 1 Hz supervisor task (ZC diagnostics
 * log, failsafe watchdog, test sweep when compiled in). Call after
 * actuator_init(). No-op unless built for V5.
 */
void fanphase_init(void);

/**
 * Command the fan power level, 0.0 (off) to 1.0 (full conduction).
 * Values in (0, FANPHASE_LEVEL_FLOOR) clamp UP to the floor — a phase-
 * starved fan motor stalls and hums. The mapping is phase-linear, not
 * power-linear (fine for airflow). Safe to call before fanphase_init()
 * (the level is applied at init) and from any task. On V2/V4 builds the
 * actuator never calls this.
 */
void fanphase_set_level(float level);

#ifdef __cplusplus
}
#endif

#endif // FANPHASE_H
