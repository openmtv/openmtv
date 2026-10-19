/* Multiplayer: the network game's savegame (src/mpsave.c). */
#ifndef MTV_MPSAVE_H
#define MTV_MPSAVE_H
#include <stdint.h>

/* src/actions.c: game_load reads these bytes instead of a slot's resource
 * while mp_load_buf is set; mp_savegame_write writes what game_save would,
 * without changing the game (0: does not fit) */
extern const uint8_t *mp_load_buf;
extern uint16_t mp_load_len;
uint32_t mp_savegame_write(uint8_t *buf, uint32_t cap);

/* the options are runtime/main.c's (runtime.h): mp_save_dir, mp_load_opt,
 * mp_load_data/mp_load_size, mp_files_hash */
extern int mp_loaded;                   /* the game came from a network game's savegame */
void mpsave_night(void);                /* the day change: write DIR/tag-NNN.mtvmp (with --mp-save) */
void mpsave_load_start(void);           /* the setup's Start with --mp-load: the savegame, then the morning */
int mpsave_apply_human(int station);    /* that station's human's own data into memory; 0 if the save has none */
#endif
