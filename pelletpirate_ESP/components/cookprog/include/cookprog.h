#ifndef COOKPROG_H
#define COOKPROG_H

/**
 * Cook profile engine — "driver + passengers" (backlog item 17).
 *
 * A program is an ordered list of pit steps executed against ONE driver
 * meat probe; all other probes keep their own goals/alarms but never
 * touch the pit. The engine is a sequencer ONLY: it sets mode/target
 * through grill_state exactly like a human at the UI would, so every
 * safety layer (igniter inhibit, flame-out, fault shutdown) applies
 * unchanged.
 *
 * Program file format (LittleFS /lfs/prog_<Name>.ppg, or a factory
 * template embedded in firmware), key=value lines:
 *   ver=1
 *   name=Brisket_Boss
 *   step=<mode>,<target>,<exit>,<value>,<note>
 * mode: supersmoke | smoke | cook | keepwarm | gate
 * exit: temp (driver >= value F) | time (value minutes) |
 *       ack (gate: banner + wait for alarm ack) | end (terminal hold)
 * A gate step leaves the pit in the previous step's mode.
 *
 * Engine rules: conditions latch one-way; manual mode changes PAUSE the
 * program (explicit resume); a faulted driver probe holds the current
 * step and alarms; run state (name/step/driver) persists in NVS so a
 * power-loss resume continues mid-program.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOKPROG_MAX_STEPS 8
#define COOKPROG_NAME_MAX  32

typedef struct {
    bool running;
    bool paused;                    // manual mode change detected
    char name[COOKPROG_NAME_MAX];
    int step;                       // 0-based current step
    int step_count;
    int driver;                     // meat probe index 0-3
    char step_text[64];             // human summary of the current step
} cookprog_status_t;

/** Load run state from NVS and start the engine task. */
void cookprog_init(void);

/**
 * Start a program on a driver probe (0-3). Source is "web"/"lcd" for the
 * cook log. Returns false if the program can't be found/parsed or one is
 * already running.
 */
bool cookprog_run(const char *name, int driver, const char *source);

/** Stop the program (pit mode is left as-is; user decides what's next). */
void cookprog_stop(const char *source);

/** Resume a program paused by a manual mode change. */
bool cookprog_resume_manual(void);

/** Snapshot of the engine state for status JSON / UIs. */
void cookprog_get_status(cookprog_status_t *out);

/**
 * List available programs: factory templates plus /lfs/prog_*.ppg files.
 * Writes up to max names; returns the count. Factory entries are prefixed
 * with '*' so UIs can mark them read-only.
 */
int cookprog_list(char names[][COOKPROG_NAME_MAX], int max);

/**
 * Read a program's raw text (factory or user) into buf. Returns false if
 * not found.
 */
bool cookprog_read(const char *name, char *buf, int len);

/**
 * Save (or delete, if content is empty/NULL) a user program file.
 * Factory templates can't be written — copy under a new name instead.
 */
bool cookprog_write(const char *name, const char *content);

#ifdef __cplusplus
}
#endif

#endif
