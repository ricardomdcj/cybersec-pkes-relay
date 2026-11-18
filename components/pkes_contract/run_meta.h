#pragma once

/*
 * Run header (M-13): prints the RUN_META line at boot with the git commit of the source,
 * the state (+mod if there are uncommitted changes) and the configure date.
 * It links each capture to the code that produced it (the campaign manifest, D-03, reads
 * this line).
 *
 * The CMake step reads the commit at configure time. After committing, run
 * `idf.py reconfigure` before building, otherwise the line shows the previous commit.
 */
void run_meta_print(const char *role);

/* Short commit of the source (with +mod if there are changes), for the screen splash (V-15). */
const char *run_meta_commit(void);
