#ifndef PROFILES_H
#define PROFILES_H

/**
 * Cook profiles — snapshots of grill target + all probe configs, stored
 * as small key=value files on the LittleFS partition (shared with cook
 * logs). Auto-named from the date and the first configured meat, e.g.
 * "0714 Brisket". Newest-first listing.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROFILE_NAME_MAX 40
#define PROFILES_LIST_MAX 12

typedef struct {
    char fname[PROFILE_NAME_MAX];   // on-disk filename
    char display[PROFILE_NAME_MAX]; // human name, e.g. "0714 Brisket"
} profile_info_t;

/** List profiles newest-first. Returns count (up to max). */
int profiles_list(profile_info_t *out, int max);

/**
 * Save the current grill target + probe configs as a new profile.
 * Writes the display name into name_out. Returns false on failure.
 */
bool profiles_save_current(char *name_out, int len);

/** Load a profile by filename into grill_state (persists to NVS). */
bool profiles_load(const char *fname);

/** Delete a profile by filename. Returns false if missing or invalid name. */
bool profiles_delete(const char *fname);

/**
 * Bumped on every successful save/delete (from LCD or web). Clients poll
 * this via the status JSON to know when to re-fetch the profile list.
 */
uint32_t profiles_revision(void);

#ifdef __cplusplus
}
#endif

#endif
