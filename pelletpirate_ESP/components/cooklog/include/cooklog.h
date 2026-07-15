#ifndef COOKLOG_H
#define COOKLOG_H

/**
 * Cook audit logging to the LittleFS partition.
 *
 * One CSV file per cook: opens when the grill leaves Off, closes when it
 * returns. Two row types share one column set:
 *   S = periodic sample (every N seconds while active)
 *   E = event, logged immediately, full state snapshot + note with source
 * Columns: time,ev,mode,grill,tgt,u,fan,aug,ign,p1,p2,p3,p4,rssi,note
 *
 * Events raised while Off (pre-cook setup like target/probe changes) are
 * buffered in RAM and flushed into the file when the cook starts.
 * Oldest cook files are deleted when the partition runs low.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Mount LittleFS, load the interval setting from NVS, start the task. */
void cooklog_init(void);

/** Sampling interval in seconds: 0 = logging off, otherwise 10 or 30. */
void cooklog_set_interval(int seconds);
int cooklog_get_interval(void);

/**
 * Record an event row immediately (or buffer it if no cook is active).
 * source: "lcd", "web", "auto", "safety". printf-style note.
 */
void cooklog_event(const char *source, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif
